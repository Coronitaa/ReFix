#pragma once

#include "mod_types.h"
#include <string>
#include <set>
#include <map>
#include <cstdint>

namespace refix::mods {

struct ModTrustEntry {
    std::string mod_id;
    std::string pinned_pubkey;           // Pinned author Ed25519 public key hex
    std::set<std::string> approved_caps;  // Capabilities explicitly approved by user
    bool user_approved = false;           // True if user approved, false if rejected/pending
    uint64_t timestamp = 0;              // UNIX timestamp of approval
};

class ModTrustStore {
public:
    explicit ModTrustStore(const std::string& custom_storage_path = "");

    // Load store from disk with DPAPI + HMAC decryption and integrity check
    bool Load(std::string& out_error);

    // Save store to disk with DPAPI + HMAC encryption
    bool Save(std::string& out_error);

    // Query approval status
    bool GetEntry(const std::string& mod_id, ModTrustEntry& out_entry) const;

    // Check if mod needs user approval prompt
    // Returns true if:
    // 1. Mod never approved before.
    // 2. Author key changed (TOFU violation).
    // 3. Manifest requests capabilities that were not in approved_caps.
    bool NeedsUserApproval(const ModManifest& manifest, std::vector<std::string>& out_new_caps) const;

    // Record user decision (approve or reject)
    void RecordDecision(const ModManifest& manifest, bool approved);

    // Revoke trust for a mod
    void RevokeMod(const std::string& mod_id);

    // Clear all entries (for testing)
    void Clear();

    const std::map<std::string, ModTrustEntry>& GetAllEntries() const { return entries_; }

private:
    std::string storage_path_;
    std::map<std::string, ModTrustEntry> entries_;

    static std::string GetDefaultPath();
    static bool ProtectData(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext);
    static bool UnprotectData(const std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& plaintext);
};

// Global default trust store instance
ModTrustStore& GetGlobalTrustStore();

} // namespace refix::mods
