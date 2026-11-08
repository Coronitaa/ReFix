#include "mod_trust_store.h"
#include "mod_signature.h"

#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>

#pragma comment(lib, "Crypt32.lib")

namespace stdfs = std::filesystem;

namespace refix::mods {

static const char* TRUST_STORE_MAGIC = "RFTRUST1"; // 8 bytes magic header

std::string ModTrustStore::GetDefaultPath() {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        stdfs::path p = stdfs::path(appdata) / "ReFix" / "mods" / "trust.json";
        return p.string();
    }
    return "trust.json";
}

ModTrustStore::ModTrustStore(const std::string& custom_storage_path) {
    if (!custom_storage_path.empty()) {
        storage_path_ = custom_storage_path;
    } else {
        storage_path_ = GetDefaultPath();
    }
}

bool ModTrustStore::ProtectData(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext) {
    DATA_BLOB data_in;
    data_in.pbData = const_cast<BYTE*>(plaintext.data());
    data_in.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB data_out;
    // DPAPI with current user credentials
    if (!CryptProtectData(&data_in, L"ReFix Mod Trust Store", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &data_out)) {
        return false;
    }

    ciphertext.assign(data_out.pbData, data_out.pbData + data_out.cbData);
    LocalFree(data_out.pbData);
    return true;
}

bool ModTrustStore::UnprotectData(const std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& plaintext) {
    DATA_BLOB data_in;
    data_in.pbData = const_cast<BYTE*>(ciphertext.data());
    data_in.cbData = static_cast<DWORD>(ciphertext.size());

    DATA_BLOB data_out;
    if (!CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &data_out)) {
        return false;
    }

    plaintext.assign(data_out.pbData, data_out.pbData + data_out.cbData);
    LocalFree(data_out.pbData);
    return true;
}

void ModTrustStore::Clear() {
    entries_.clear();
}

bool ModTrustStore::GetEntry(const std::string& mod_id, ModTrustEntry& out_entry) const {
    auto it = entries_.find(mod_id);
    if (it != entries_.end()) {
        out_entry = it->second;
        return true;
    }
    return false;
}

bool ModTrustStore::NeedsUserApproval(const ModManifest& manifest, std::vector<std::string>& out_new_caps) const {
    out_new_caps.clear();

    auto it = entries_.find(manifest.id);
    if (it == entries_.end()) {
        // Never approved before -> needs approval for all declared capabilities
        for (const auto& cap : manifest.capabilities) {
            out_new_caps.push_back(cap);
        }
        return true;
    }

    const ModTrustEntry& entry = it->second;
    if (!entry.user_approved) {
        // Previously rejected or revoked
        return true;
    }

    // TOFU check: author public key must not change!
    if (!entry.pinned_pubkey.empty() && !manifest.author_pubkey.empty()) {
        if (_stricmp(entry.pinned_pubkey.c_str(), manifest.author_pubkey.c_str()) != 0) {
            // Key changed! Flag as requiring re-approval
            for (const auto& cap : manifest.capabilities) {
                out_new_caps.push_back(cap);
            }
            return true;
        }
    }

    // Check for newly requested capabilities
    for (const auto& cap : manifest.capabilities) {
        if (entry.approved_caps.find(cap) == entry.approved_caps.end()) {
            out_new_caps.push_back(cap);
        }
    }

    return !out_new_caps.empty();
}

void ModTrustStore::RecordDecision(const ModManifest& manifest, bool approved) {
    ModTrustEntry entry;
    entry.mod_id = manifest.id;
    entry.pinned_pubkey = manifest.author_pubkey;
    entry.user_approved = approved;
    entry.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1));
    if (approved) {
        entry.approved_caps = manifest.capabilities;
    } else {
        entry.approved_caps.clear();
    }
    entries_[manifest.id] = entry;
}

void ModTrustStore::RevokeMod(const std::string& mod_id) {
    auto it = entries_.find(mod_id);
    if (it != entries_.end()) {
        it->second.user_approved = false;
        it->second.approved_caps.clear();
    }
}

bool ModTrustStore::Save(std::string& out_error) {
    // 1. Serialize entries to plaintext JSON
    std::ostringstream ss;
    ss << "{\n  \"version\": 1,\n  \"entries\": [\n";
    size_t idx = 0;
    for (const auto& [id, entry] : entries_) {
        if (idx > 0) ss << ",\n";
        ss << "    {\n"
           << "      \"mod_id\": \"" << entry.mod_id << "\",\n"
           << "      \"pinned_pubkey\": \"" << entry.pinned_pubkey << "\",\n"
           << "      \"user_approved\": " << (entry.user_approved ? "true" : "false") << ",\n"
           << "      \"timestamp\": " << entry.timestamp << ",\n"
           << "      \"approved_caps\": [";
        size_t cap_idx = 0;
        for (const auto& cap : entry.approved_caps) {
            if (cap_idx > 0) ss << ", ";
            ss << "\"" << cap << "\"";
            cap_idx++;
        }
        ss << "]\n    }";
        idx++;
    }
    ss << "\n  ]\n}\n";

    std::string json_str = ss.str();
    std::vector<uint8_t> plaintext(json_str.begin(), json_str.end());

    // 2. Encrypt with DPAPI
    std::vector<uint8_t> encrypted_blob;
    if (!ProtectData(plaintext, encrypted_blob)) {
        out_error = "DPAPI encryption failed";
        return false;
    }

    // 3. Compute SHA-256 HMAC / checksum of encrypted blob
    std::string sha256_hex = ComputeMemorySha256(encrypted_blob.data(), encrypted_blob.size());

    // Ensure directory exists
    stdfs::path p(storage_path_);
    if (p.has_parent_path()) {
        std::error_code ec;
        stdfs::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(storage_path_, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        out_error = "Failed to open storage file for writing: " + storage_path_;
        return false;
    }

    // Write header: MAGIC (8 bytes) + SHA256 (64 hex chars) + payload
    out.write(TRUST_STORE_MAGIC, 8);
    out.write(sha256_hex.data(), 64);
    out.write(reinterpret_cast<const char*>(encrypted_blob.data()), encrypted_blob.size());
    out.close();

    return true;
}

bool ModTrustStore::Load(std::string& out_error) {
    entries_.clear();

    if (!stdfs::exists(storage_path_)) {
        // Non-existent store is not an error; start empty
        return true;
    }

    std::ifstream in(storage_path_, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        out_error = "Failed to open storage file: " + storage_path_;
        return false;
    }

    std::streamsize size = in.tellg();
    if (size < (8 + 64)) {
        out_error = "Corrupted trust store file (truncated header)";
        return false;
    }

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buffer.data()), size);
    in.close();

    // Verify magic header
    if (memcmp(buffer.data(), TRUST_STORE_MAGIC, 8) != 0) {
        out_error = "Invalid trust store magic header";
        return false;
    }

    std::string stored_sha256(reinterpret_cast<char*>(buffer.data() + 8), 64);
    const uint8_t* cipher_data = buffer.data() + 8 + 64;
    size_t cipher_size = buffer.size() - 8 - 64;

    std::string computed_sha256 = ComputeMemorySha256(cipher_data, cipher_size);
    if (_stricmp(stored_sha256.c_str(), computed_sha256.c_str()) != 0) {
        out_error = "Trust store integrity verification failed (tampered ciphertext)";
        return false;
    }

    std::vector<uint8_t> ciphertext(cipher_data, cipher_data + cipher_size);
    std::vector<uint8_t> plaintext;
    if (!UnprotectData(ciphertext, plaintext)) {
        out_error = "DPAPI decryption failed (store was encrypted by another user or corrupted)";
        return false;
    }

    std::string json_str(plaintext.begin(), plaintext.end());

    // Simple parser for trust store entries
    size_t pos = 0;
    while ((pos = json_str.find("\"mod_id\":", pos)) != std::string::npos) {
        ModTrustEntry entry;

        // Parse mod_id
        size_t start = json_str.find('"', pos + 9);
        size_t end = json_str.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            entry.mod_id = json_str.substr(start + 1, end - start - 1);
        }

        // Parse pinned_pubkey
        size_t pk_pos = json_str.find("\"pinned_pubkey\":", end);
        if (pk_pos != std::string::npos && pk_pos < json_str.find('}', end)) {
            start = json_str.find('"', pk_pos + 16);
            end = json_str.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                entry.pinned_pubkey = json_str.substr(start + 1, end - start - 1);
            }
        }

        // Parse user_approved
        size_t app_pos = json_str.find("\"user_approved\":", end);
        if (app_pos != std::string::npos && app_pos < json_str.find('}', end)) {
            if (json_str.substr(app_pos, 25).find("true") != std::string::npos) {
                entry.user_approved = true;
            } else {
                entry.user_approved = false;
            }
        }

        // Parse approved_caps array
        size_t caps_pos = json_str.find("\"approved_caps\":", end);
        if (caps_pos != std::string::npos && caps_pos < json_str.find('}', end)) {
            size_t arr_start = json_str.find('[', caps_pos);
            size_t arr_end = json_str.find(']', arr_start);
            if (arr_start != std::string::npos && arr_end != std::string::npos) {
                std::string caps_str = json_str.substr(arr_start + 1, arr_end - arr_start - 1);
                size_t cpos = 0;
                while ((cpos = caps_str.find('"', cpos)) != std::string::npos) {
                    size_t cend = caps_str.find('"', cpos + 1);
                    if (cend != std::string::npos) {
                        entry.approved_caps.insert(caps_str.substr(cpos + 1, cend - cpos - 1));
                        cpos = cend + 1;
                    } else {
                        break;
                    }
                }
            }
        }

        if (!entry.mod_id.empty()) {
            entries_[entry.mod_id] = entry;
        }

        pos = json_str.find('}', end);
        if (pos == std::string::npos) break;
        pos++;
    }

    return true;
}

ModTrustStore& GetGlobalTrustStore() {
    static ModTrustStore s_instance;
    return s_instance;
}

} // namespace refix::mods
