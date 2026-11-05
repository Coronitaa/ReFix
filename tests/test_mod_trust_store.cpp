#include "../src/mods/mod_trust_store.h"
#include "../src/mods/mod_signature.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace refix::mods;

int main() {
    std::cout << "Running test_mod_trust_store..." << std::endl;

    fs::path temp_dir = fs::temp_directory_path() / "refix_trust_test";
    fs::create_directories(temp_dir);
    std::string store_path = (temp_dir / "trust.json").string();

    std::string err;
    ModTrustStore store(store_path);

    // 1. Initial empty store
    assert(store.Load(err));
    assert(store.GetAllEntries().empty());
    std::cout << "[PASS] Empty store initialized" << std::endl;

    // 2. Prepare sample manifest
    ModManifest m1;
    m1.id = "com.refix.samplemod";
    m1.author_pubkey = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    m1.capabilities = { Capability::Log, Capability::Config };

    std::vector<std::string> new_caps;
    assert(store.NeedsUserApproval(m1, new_caps));
    assert(new_caps.size() == 2);
    std::cout << "[PASS] Unapproved mod correctly flagged as needing approval" << std::endl;

    // 3. User approves mod
    store.RecordDecision(m1, true);
    assert(store.Save(err));
    assert(fs::exists(store_path));
    std::cout << "[PASS] Decision recorded and DPAPI-saved" << std::endl;

    // 4. Reload into new instance and verify persistence
    ModTrustStore store2(store_path);
    assert(store2.Load(err));
    assert(!store2.NeedsUserApproval(m1, new_caps));
    assert(new_caps.empty());
    std::cout << "[PASS] Approved mod persisted and reloaded cleanly" << std::endl;

    // 5. Test Capability Escalation
    ModManifest m1_escalated = m1;
    m1_escalated.capabilities.insert(Capability::MemoryWrite); // High risk!
    assert(store2.NeedsUserApproval(m1_escalated, new_caps));
    assert(new_caps.size() == 1);
    assert(new_caps[0] == Capability::MemoryWrite);
    std::cout << "[PASS] Capability escalation correctly detected" << std::endl;

    // 6. Test TOFU Key Pinning Violation
    ModManifest m1_hijacked = m1;
    m1_hijacked.author_pubkey = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    assert(store2.NeedsUserApproval(m1_hijacked, new_caps));
    std::cout << "[PASS] TOFU public key tampering detected" << std::endl;

    // 7. Test Anti-tamper on store file
    // Corrupt ciphertext in file
    std::fstream f(store_path, std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(80); // Middle of ciphertext
    f.put('X');
    f.close();

    ModTrustStore store_tampered(store_path);
    assert(!store_tampered.Load(err));
    std::cout << "[PASS] Tampered ciphertext rejected with integrity error: " << err << std::endl;

    // Cleanup
    fs::remove_all(temp_dir);

    std::cout << "All test_mod_trust_store tests passed!" << std::endl;
    return 0;
}
