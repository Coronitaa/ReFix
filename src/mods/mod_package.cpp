#include "mod_package.h"
#include "miniz/miniz.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>

namespace refix::mods {

static bool EnsureDirectoryRecursive(const std::string& path) {
    if (path.empty()) return true;
    char temp[MAX_PATH];
    strncpy_s(temp, path.c_str(), _TRUNCATE);
    for (char* p = temp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char orig = *p;
            *p = 0;
            CreateDirectoryA(temp, NULL);
            *p = orig;
        }
    }
    CreateDirectoryA(temp, NULL);
    return true;
}

static std::string GetDirectoryOfPath(const std::string& filepath) {
    size_t last_slash = filepath.find_last_of("/\\");
    if (last_slash == std::string::npos) return "";
    return filepath.substr(0, last_slash);
}

bool ModPackage::ReadDiskFile(const std::string& path, std::vector<uint8_t>& out_data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);
    out_data.resize(static_cast<size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(out_data.data()), size);
    }
    return true;
}

bool ModPackage::WriteDiskFile(const std::string& path, const uint8_t* data, size_t size) {
    std::string dir = GetDirectoryOfPath(path);
    if (!dir.empty()) {
        EnsureDirectoryRecursive(dir);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    if (size > 0 && data != nullptr) {
        file.write(reinterpret_cast<const char*>(data), size);
    }
    return true;
}

bool ModPackage::CreatePackage(const std::string& output_rfmod_path,
                              ModManifest manifest,
                              const std::vector<PackageFileEntry>& files,
                              const uint8_t private_key_64[64],
                              std::string& out_error) {
    // 1. Update manifest file hashes
    manifest.files.clear();
    for (const auto& file_entry : files) {
        if (!IsSafeRelativePath(file_entry.relative_path)) {
            out_error = "Invalid or unsafe file path: " + file_entry.relative_path;
            return false;
        }
        if (file_entry.relative_path == "refixmod.json" || file_entry.relative_path == "refixmod.sig") {
            continue; // Will be added separately
        }

        std::string hash = file_entry.sha256_hex;
        if (hash.empty()) {
            hash = ComputeMemorySha256(file_entry.data.data(), file_entry.data.size());
        }

        manifest.files[file_entry.relative_path] = hash;
    }

    // 2. Validate manifest strictly
    if (!ValidateManifest(manifest, out_error)) {
        return false;
    }

    // 3. Serialize canonical manifest
    std::string canonical_manifest = ToCanonicalJson(manifest);

    // 4. Compute signature if private key provided
    std::vector<uint8_t> signature_bytes;
    if (private_key_64 != nullptr) {
        signature_bytes.resize(64);
        CryptoSign(reinterpret_cast<const uint8_t*>(canonical_manifest.data()),
                   canonical_manifest.size(),
                   private_key_64,
                   signature_bytes.data());
    }

    // 5. Build ZIP archive with miniz
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_writer_init_file(&zip_archive, output_rfmod_path.c_str(), 0)) {
        out_error = "Failed to initialize ZIP file: " + output_rfmod_path;
        return false;
    }

    // Add refixmod.json
    if (!mz_zip_writer_add_mem(&zip_archive, "refixmod.json", canonical_manifest.data(), canonical_manifest.size(), MZ_BEST_COMPRESSION)) {
        mz_zip_writer_end(&zip_archive);
        out_error = "Failed to add refixmod.json to archive";
        return false;
    }

    // Add refixmod.sig if generated
    if (!signature_bytes.empty()) {
        if (!mz_zip_writer_add_mem(&zip_archive, "refixmod.sig", signature_bytes.data(), signature_bytes.size(), MZ_BEST_COMPRESSION)) {
            mz_zip_writer_end(&zip_archive);
            out_error = "Failed to add refixmod.sig to archive";
            return false;
        }
    }

    // Add all payload files
    for (const auto& file_entry : files) {
        if (file_entry.relative_path == "refixmod.json" || file_entry.relative_path == "refixmod.sig") {
            continue;
        }
        if (!mz_zip_writer_add_mem(&zip_archive, file_entry.relative_path.c_str(),
                                   file_entry.data.data(), file_entry.data.size(), MZ_BEST_COMPRESSION)) {
            mz_zip_writer_end(&zip_archive);
            out_error = "Failed to add file to archive: " + file_entry.relative_path;
            return false;
        }
    }

    if (!mz_zip_writer_finalize_archive(&zip_archive)) {
        mz_zip_writer_end(&zip_archive);
        out_error = "Failed to finalize ZIP archive";
        return false;
    }

    mz_zip_writer_end(&zip_archive);
    return true;
}

bool ModPackage::InspectPackage(const std::string& rfmod_path,
                               const uint8_t expected_public_key_32[32],
                               const RevocationList* revocation_list,
                               PackageInspectInfo& out_info,
                               std::string& out_error) {
    out_info = PackageInspectInfo();
    out_info.is_valid = false;

    std::string package_sha256 = ComputeFileSha256(rfmod_path);

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, rfmod_path.c_str(), 0)) {
        out_error = "Failed to open package: " + rfmod_path;
        return false;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    std::string raw_manifest;
    std::vector<uint8_t> signature_bytes;
    std::set<std::string> zip_files;

    for (mz_uint i = 0; i < num_files; ++i) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            continue;
        }
        if (file_stat.m_is_directory) continue;

        std::string filename = file_stat.m_filename;
        std::replace(filename.begin(), filename.end(), '\\', '/');

        if (!IsSafeRelativePath(filename)) {
            out_info.validation_errors.push_back("Unsafe file path in zip: " + filename);
            continue;
        }

        zip_files.insert(filename);
        out_info.files_in_zip.push_back(filename);

        if (filename == "refixmod.json") {
            size_t uncomp_size = 0;
            void* p = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncomp_size, 0);
            if (p) {
                raw_manifest.assign(reinterpret_cast<char*>(p), uncomp_size);
                mz_free(p);
            }
        } else if (filename == "refixmod.sig") {
            size_t uncomp_size = 0;
            void* p = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncomp_size, 0);
            if (p) {
                signature_bytes.assign(reinterpret_cast<uint8_t*>(p), reinterpret_cast<uint8_t*>(p) + uncomp_size);
                mz_free(p);
            }
        }
    }

    if (raw_manifest.empty()) {
        mz_zip_reader_end(&zip_archive);
        out_error = "Package is missing refixmod.json";
        return false;
    }

    // Parse manifest
    if (!ParseManifest(raw_manifest, out_info.manifest, out_error)) {
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Check revocation first
    const RevocationList& active_rev = revocation_list ? *revocation_list : GetGlobalRevocationList();
    if (active_rev.IsRevoked(out_info.manifest.author_pubkey, package_sha256)) {
        out_info.trust_level = TrustLevel::Revoked;
        out_info.validation_errors.push_back("Package or author public key is in revocation list");
    }

    // Verify signature
    if (signature_bytes.size() != 64) {
        out_info.signature_valid = false;
        out_info.error_message = "Missing or invalid refixmod.sig length";
        out_info.validation_errors.push_back(out_info.error_message);
    } else {
        std::string canonical_manifest = ToCanonicalJson(out_info.manifest);
        const uint8_t* pubkey_to_use = expected_public_key_32;
        uint8_t manifest_pubkey[32];
        if (pubkey_to_use == nullptr && !out_info.manifest.author_pubkey.empty()) {
            if (FromHex(out_info.manifest.author_pubkey, manifest_pubkey, 32)) {
                pubkey_to_use = manifest_pubkey;
            }
        }

        if (pubkey_to_use == nullptr) {
            out_info.signature_valid = false;
            out_info.error_message = "No public key available for verification";
            out_info.validation_errors.push_back(out_info.error_message);
        } else {
            out_info.signature_valid = CryptoVerify(
                reinterpret_cast<const uint8_t*>(canonical_manifest.data()),
                canonical_manifest.size(),
                pubkey_to_use,
                signature_bytes.data()
            );

            if (!out_info.signature_valid) {
                out_info.error_message = "Cryptographic signature verification failed";
                out_info.validation_errors.push_back(out_info.error_message);
            } else {
                out_info.trust_level = DetermineTrustLevel(out_info.manifest, package_sha256);
            }
        }
    }

    // Verify all files match hashes
    for (const auto& [rel_path, declared_hash] : out_info.manifest.files) {
        if (zip_files.find(rel_path) == zip_files.end()) {
            out_info.validation_errors.push_back("Declared file missing in archive: " + rel_path);
            continue;
        }

        int file_index = mz_zip_reader_locate_file(&zip_archive, rel_path.c_str(), nullptr, 0);
        if (file_index < 0) {
            out_info.validation_errors.push_back("Unable to locate file in archive: " + rel_path);
            continue;
        }

        size_t uncomp_size = 0;
        void* p = mz_zip_reader_extract_to_heap(&zip_archive, file_index, &uncomp_size, 0);
        if (!p) {
            out_info.validation_errors.push_back("Failed to decompress file: " + rel_path);
            continue;
        }

        std::string actual_hash = ComputeMemorySha256(p, uncomp_size);
        mz_free(p);

        if (_stricmp(actual_hash.c_str(), declared_hash.c_str()) != 0) {
            out_info.validation_errors.push_back("Hash mismatch for file " + rel_path + " (expected: " + declared_hash + ", got: " + actual_hash + ")");
        }
    }

    // Check for rogue unlisted files
    for (const auto& path_in_zip : zip_files) {
        if (path_in_zip == "refixmod.json" || path_in_zip == "refixmod.sig") continue;
        if (out_info.manifest.files.find(path_in_zip) == out_info.manifest.files.end()) {
            out_info.validation_errors.push_back("Rogue unlisted file detected in archive: " + path_in_zip);
        }
    }

    mz_zip_reader_end(&zip_archive);

    out_info.is_valid = out_info.validation_errors.empty() && out_info.signature_valid && (out_info.trust_level != TrustLevel::Revoked);
    return true;
}

bool ModPackage::VerifyAndExtract(const std::string& rfmod_path,
                                 const std::string& dest_dir,
                                 const uint8_t expected_public_key_32[32],
                                 const RevocationList* revocation_list,
                                 PackageInspectInfo& out_info,
                                 std::string& out_error) {
    if (!InspectPackage(rfmod_path, expected_public_key_32, revocation_list, out_info, out_error)) {
        return false;
    }

    if (!out_info.is_valid) {
        std::ostringstream ss;
        ss << "Package verification failed with " << out_info.validation_errors.size() << " error(s): ";
        for (size_t i = 0; i < out_info.validation_errors.size(); ++i) {
            if (i > 0) ss << "; ";
            ss << out_info.validation_errors[i];
        }
        out_error = ss.str();
        return false;
    }

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_file(&zip_archive, rfmod_path.c_str(), 0)) {
        out_error = "Failed to open zip archive for extraction: " + rfmod_path;
        return false;
    }

    EnsureDirectoryRecursive(dest_dir);

    for (const auto& rel_path : out_info.files_in_zip) {
        int idx = mz_zip_reader_locate_file(&zip_archive, rel_path.c_str(), nullptr, 0);
        if (idx < 0) continue;

        size_t uncomp_size = 0;
        void* p = mz_zip_reader_extract_to_heap(&zip_archive, idx, &uncomp_size, 0);
        if (!p) {
            mz_zip_reader_end(&zip_archive);
            out_error = "Extraction failed for file: " + rel_path;
            return false;
        }

        std::string full_dest = dest_dir + "/" + rel_path;
        bool write_ok = WriteDiskFile(full_dest, reinterpret_cast<const uint8_t*>(p), uncomp_size);
        mz_free(p);

        if (!write_ok) {
            mz_zip_reader_end(&zip_archive);
            out_error = "Failed to write extracted file to disk: " + full_dest;
            return false;
        }
    }

    mz_zip_reader_end(&zip_archive);
    return true;
}

} // namespace refix::mods
