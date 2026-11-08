#include <windows.h>
#include "../../src/mods/mod_package.h"
#include "../../src/mods/mod_manifest.h"
#include "../../src/mods/mod_signature.h"
#include "../../src/mods/miniz/miniz.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <algorithm>

namespace stdfs = std::filesystem;
using namespace refix::mods;

static void PrintHelp() {
    std::cout << "ReFix Mods CLI Tool (refixmod)\n"
              << "Usage: refixmod <command> [options]\n\n"
              << "Commands:\n"
              << "  keygen [--out <name>]                Generate Ed25519 author keypair (32-byte pub, 64-byte priv)\n"
              << "  lint <manifest.json>                 Strictly validate manifest schema, limits, and capabilities\n"
              << "  pack <mod_dir> <out.rfmod> [--key <privkey_hex_or_file>]\n"
              << "                                       Pack directory into .rfmod package, hashing files and signing\n"
              << "  sign <mod.rfmod> --key <privkey_hex_or_file>\n"
              << "                                       Sign an existing .rfmod package manifest\n"
              << "  verify <mod.rfmod> [--key <pubkey_hex_or_file>]\n"
              << "                                       Verify cryptographic signature, file hashes, and safe structure\n"
              << "  inspect <mod.rfmod>                  Inspect package manifest, files, risk rating, and signature\n";
}

static bool LoadKeyFileOrHex(const std::string& input, uint8_t* out_buf, size_t expected_len) {
    if (stdfs::exists(input)) {
        std::ifstream f(input, std::ios::binary);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            content.erase(std::remove_if(content.begin(), content.end(), ::isspace), content.end());
            if (content.size() == expected_len * 2) {
                return FromHex(content, out_buf, expected_len);
            }
            if (content.size() == expected_len) {
                memcpy(out_buf, content.data(), expected_len);
                return true;
            }
        }
    }
    std::string hex = input;
    hex.erase(std::remove_if(hex.begin(), hex.end(), ::isspace), hex.end());
    if (hex.size() == expected_len * 2) {
        return FromHex(hex, out_buf, expected_len);
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "keygen") {
        std::string out_prefix = "author_key";
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--out" && i + 1 < argc) {
                out_prefix = argv[++i];
            }
        }

        uint8_t pubkey[32];
        uint8_t privkey[64];
        if (!CryptoGenerateKeypair(pubkey, privkey)) {
            std::cerr << "[ERROR] Cryptographic entropy failure generating Ed25519 keypair\n";
            return 1;
        }

        std::string pub_hex = ToHex(pubkey, 32);
        std::string priv_hex = ToHex(privkey, 64);

        std::ofstream fpub(out_prefix + ".pub");
        fpub << pub_hex << "\n";
        fpub.close();

        std::ofstream fpriv(out_prefix + ".priv");
        fpriv << priv_hex << "\n";
        fpriv.close();

        std::cout << "[SUCCESS] Generated Ed25519 keypair:\n"
                  << "  Public key (" << out_prefix << ".pub): " << pub_hex << "\n"
                  << "  Private key (" << out_prefix << ".priv): [SAVED TO FILE]\n";
        return 0;
    }

    if (cmd == "lint") {
        if (argc < 3) {
            std::cerr << "Usage: refixmod lint <manifest.json>\n";
            return 1;
        }
        std::string path = argv[2];
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[ERROR] Cannot open manifest file: " << path << "\n";
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        ModManifest manifest;
        std::string error;
        if (!ParseManifest(content, manifest, error)) {
            std::cerr << "[FAIL] JSON parsing failed: " << error << "\n";
            return 1;
        }
        if (!ValidateManifest(manifest, error)) {
            std::cerr << "[FAIL] Manifest semantic validation failed: " << error << "\n";
            return 1;
        }

        std::cout << "[SUCCESS] Manifest is valid.\n"
                  << "  Mod ID: " << manifest.id << " (v" << manifest.version << ")\n"
                  << "  Tier:   " << (manifest.execution_tier == ExecutionTier::Sandboxed ? "Sandboxed (WASM)" : "Native (DLL)") << "\n"
                  << "  Author: " << manifest.author << "\n"
                  << "  Target games: " << manifest.target_games.size() << "\n"
                  << "  Capabilities: " << manifest.capabilities.size() << "\n";
        for (const auto& cap : manifest.capabilities) {
            if (Capability::IsHighRisk(cap)) {
                std::cout << "    - " << cap << " [HIGH RISK]\n";
            } else {
                std::cout << "    - " << cap << "\n";
            }
        }
        return 0;
    }

    if (cmd == "pack") {
        if (argc < 4) {
            std::cerr << "Usage: refixmod pack <mod_dir> <out.rfmod> [--key <privkey>]\n";
            return 1;
        }
        std::string mod_dir = argv[2];
        std::string out_rfmod = argv[3];
        std::string privkey_arg;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--key" && i + 1 < argc) {
                privkey_arg = argv[++i];
            }
        }

        stdfs::path manifest_path = stdfs::path(mod_dir) / "refixmod.json";
        if (!stdfs::exists(manifest_path)) {
            std::cerr << "[ERROR] Missing refixmod.json in directory: " << mod_dir << "\n";
            return 1;
        }

        std::ifstream fman(manifest_path);
        std::string man_content((std::istreambuf_iterator<char>(fman)), std::istreambuf_iterator<char>());
        ModManifest manifest;
        std::string err;
        if (!ParseManifest(man_content, manifest, err)) {
            std::cerr << "[ERROR] Manifest parsing error: " << err << "\n";
            return 1;
        }

        uint8_t privkey_bytes[64];
        bool has_key = false;
        if (!privkey_arg.empty()) {
            if (!LoadKeyFileOrHex(privkey_arg, privkey_bytes, 64)) {
                std::cerr << "[ERROR] Invalid private key: " << privkey_arg << "\n";
                return 1;
            }
            has_key = true;
            if (manifest.author_pubkey.empty()) {
                manifest.author_pubkey = ToHex(privkey_bytes + 32, 32);
            }
        }

        std::vector<PackageFileEntry> files;
        for (const auto& entry : stdfs::recursive_directory_iterator(mod_dir)) {
            if (entry.is_regular_file()) {
                std::string rel_path = stdfs::relative(entry.path(), mod_dir).generic_string();
                if (rel_path == "refixmod.json" || rel_path == "refixmod.sig") {
                    continue;
                }
                PackageFileEntry fe;
                fe.relative_path = rel_path;
                if (!ModPackage::ReadDiskFile(entry.path().string(), fe.data)) {
                    std::cerr << "[ERROR] Failed to read file: " << entry.path().string() << "\n";
                    return 1;
                }
                fe.sha256_hex = ComputeMemorySha256(fe.data.data(), fe.data.size());
                files.push_back(fe);
            }
        }

        if (!ModPackage::CreatePackage(out_rfmod, manifest, files, has_key ? privkey_bytes : nullptr, err)) {
            std::cerr << "[ERROR] Package creation failed: " << err << "\n";
            return 1;
        }

        std::cout << "[SUCCESS] Packed " << files.size() << " file(s) into " << out_rfmod << "\n";
        if (has_key) {
            std::cout << "  Package digitally signed with author key.\n";
        } else {
            std::cout << "  [WARNING] Package is unsigned. Sign with 'refixmod sign' before publishing.\n";
        }
        return 0;
    }

    if (cmd == "sign") {
        if (argc < 4) {
            std::cerr << "Usage: refixmod sign <mod.rfmod> --key <privkey>\n";
            return 1;
        }
        std::string mod_rfmod = argv[2];
        std::string privkey_arg;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--key" && i + 1 < argc) {
                privkey_arg = argv[++i];
            }
        }
        if (privkey_arg.empty()) {
            std::cerr << "[ERROR] --key argument required\n";
            return 1;
        }

        uint8_t privkey[64];
        if (!LoadKeyFileOrHex(privkey_arg, privkey, 64)) {
            std::cerr << "[ERROR] Invalid private key: " << privkey_arg << "\n";
            return 1;
        }

        PackageInspectInfo inspect_info;
        std::string err;
        ModPackage::InspectPackage(mod_rfmod, nullptr, nullptr, inspect_info, err);
        if (inspect_info.manifest.id.empty()) {
            std::cerr << "[ERROR] Failed to inspect package manifest: " << err << "\n";
            return 1;
        }

        std::string pub_hex = ToHex(privkey + 32, 32);
        inspect_info.manifest.author_pubkey = pub_hex;

        std::vector<PackageFileEntry> files;
        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));
        if (!mz_zip_reader_init_file(&zip_archive, mod_rfmod.c_str(), 0)) {
            std::cerr << "[ERROR] Failed to open zip archive: " << mod_rfmod << "\n";
            return 1;
        }

        mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
        for (mz_uint i = 0; i < num_files; ++i) {
            mz_zip_archive_file_stat zip_stat;
            if (mz_zip_reader_file_stat(&zip_archive, i, &zip_stat) && !zip_stat.m_is_directory) {
                std::string fn = zip_stat.m_filename;
                if (fn == "refixmod.json" || fn == "refixmod.sig") continue;
                size_t sz = 0;
                void* p = mz_zip_reader_extract_to_heap(&zip_archive, i, &sz, 0);
                if (p) {
                    PackageFileEntry fe;
                    fe.relative_path = fn;
                    fe.data.assign((uint8_t*)p, (uint8_t*)p + sz);
                    fe.sha256_hex = ComputeMemorySha256(p, sz);
                    files.push_back(fe);
                    mz_free(p);
                }
            }
        }
        mz_zip_reader_end(&zip_archive);

        if (!ModPackage::CreatePackage(mod_rfmod, inspect_info.manifest, files, privkey, err)) {
            std::cerr << "[ERROR] Failed to repack and sign package: " << err << "\n";
            return 1;
        }

        std::cout << "[SUCCESS] Package successfully signed with author key: " << pub_hex << "\n";
        return 0;
    }

    if (cmd == "verify") {
        if (argc < 3) {
            std::cerr << "Usage: refixmod verify <mod.rfmod> [--key <pubkey>]\n";
            return 1;
        }
        std::string mod_rfmod = argv[2];
        std::string pubkey_arg;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--key" && i + 1 < argc) {
                pubkey_arg = argv[++i];
            }
        }

        uint8_t pubkey[32];
        bool has_key = false;
        if (!pubkey_arg.empty()) {
            if (!LoadKeyFileOrHex(pubkey_arg, pubkey, 32)) {
                std::cerr << "[ERROR] Invalid public key: " << pubkey_arg << "\n";
                return 1;
            }
            has_key = true;
        }

        PackageInspectInfo info;
        std::string err;
        if (!ModPackage::InspectPackage(mod_rfmod, has_key ? pubkey : nullptr, nullptr, info, err)) {
            std::cerr << "[FAIL] Verification failed: " << err << "\n";
            return 1;
        }

        if (!info.is_valid) {
            std::cerr << "[FAIL] Package verification failed:\n";
            for (const auto& ve : info.validation_errors) {
                std::cerr << "  - " << ve << "\n";
            }
            return 1;
        }

        std::cout << "[SUCCESS] Package is authentic and intact.\n"
                  << "  Mod ID:        " << info.manifest.id << "\n"
                  << "  Author:        " << info.manifest.author << "\n"
                  << "  Author PubKey: " << info.manifest.author_pubkey << "\n"
                  << "  Verified files: " << info.manifest.files.size() << "\n";
        return 0;
    }

    if (cmd == "inspect") {
        if (argc < 3) {
            std::cerr << "Usage: refixmod inspect <mod.rfmod>\n";
            return 1;
        }
        std::string mod_rfmod = argv[2];
        PackageInspectInfo info;
        std::string err;
        ModPackage::InspectPackage(mod_rfmod, nullptr, nullptr, info, err);

        std::cout << "=== ReFix Mod Inspection ===\n"
                  << "Package:      " << mod_rfmod << "\n"
                  << "Mod ID:       " << info.manifest.id << "\n"
                  << "Name:         " << info.manifest.name << "\n"
                  << "Version:      " << info.manifest.version << "\n"
                  << "Author:       " << info.manifest.author << "\n"
                  << "Pubkey:       " << info.manifest.author_pubkey << "\n"
                  << "Execution:    " << (info.manifest.execution_tier == ExecutionTier::Sandboxed ? "Sandboxed (WASM)" : "Native (DLL)") << "\n"
                  << "Phase:        " << info.manifest.phase << "\n"
                  << "Priority:     " << info.manifest.priority << "\n"
                  << "Signature:    " << (info.signature_valid ? "VALID" : "INVALID / UNSIGNED") << "\n"
                  << "Overall Valid:" << (info.is_valid ? "YES" : "NO") << "\n\n";

        std::cout << "Target Games:\n";
        for (const auto& tg : info.manifest.target_games) {
            std::cout << "  - AppID=" << tg.app_id << ", Exe=" << tg.exe_name << ", Engine=" << tg.engine << "\n";
        }

        std::cout << "\nDeclared Capabilities (" << info.manifest.capabilities.size() << "):\n";
        for (const auto& cap : info.manifest.capabilities) {
            if (Capability::IsHighRisk(cap)) {
                std::cout << "  [!] " << cap << "  <-- HIGH RISK PERMISSION\n";
            } else {
                std::cout << "  [*] " << cap << "\n";
            }
        }

        std::cout << "\nFiles in Package (" << info.manifest.files.size() << "):\n";
        for (const auto& [path, hash] : info.manifest.files) {
            std::cout << "  " << std::left << std::setw(30) << path << " SHA256: " << hash << "\n";
        }

        if (!info.validation_errors.empty()) {
            std::cout << "\nValidation Warnings/Errors:\n";
            for (const auto& ve : info.validation_errors) {
                std::cout << "  - " << ve << "\n";
            }
        }
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    PrintHelp();
    return 1;
}
