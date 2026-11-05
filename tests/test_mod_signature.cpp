#include "../src/mods/mod_signature.h"
#include "../src/mods/mod_manifest.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace refix::mods;

int main() {
    printf("[*] Running ReFix Mod Cryptography & Signatures Unit Tests (TDD)...\n");

    // 1. Ed25519 Key Generation
    uint8_t alice_pub[32] = {0};
    uint8_t alice_priv[64] = {0};
    bool ok = CryptoGenerateKeypair(alice_pub, alice_priv);
    assert(ok && "Keypair generation must succeed");

    std::string alice_pub_hex = ToHex(alice_pub, 32);
    assert(alice_pub_hex.size() == 64);
    uint8_t decoded_pub[32] = {0};
    assert(FromHex(alice_pub_hex, decoded_pub, 32));
    assert(memcmp(alice_pub, decoded_pub, 32) == 0);
    printf("  [PASS] Ed25519 keypair generation and hex encoding verified\n");

    // 2. Sign and Verify Message
    const char* test_msg = "{\"id\":\"community.test\",\"name\":\"Test Mod\"}";
    size_t msg_len = strlen(test_msg);
    uint8_t sig[64] = {0};
    CryptoSign(reinterpret_cast<const uint8_t*>(test_msg), msg_len, alice_priv, sig);

    assert(CryptoVerify(reinterpret_cast<const uint8_t*>(test_msg), msg_len, alice_pub, sig) == true);
    printf("  [PASS] Authentic signature verification passed\n");

    // 3. Tampered Message Detection
    const char* tampered_msg = "{\"id\":\"community.test\",\"name\":\"Malicious Mod\"}";
    assert(CryptoVerify(reinterpret_cast<const uint8_t*>(tampered_msg), strlen(tampered_msg), alice_pub, sig) == false);
    printf("  [PASS] Tampered message strictly rejected\n");

    // 4. Altered Signature Detection
    uint8_t corrupted_sig[64];
    memcpy(corrupted_sig, sig, 64);
    corrupted_sig[10] ^= 0xFF;
    assert(CryptoVerify(reinterpret_cast<const uint8_t*>(test_msg), msg_len, alice_pub, corrupted_sig) == false);
    printf("  [PASS] Corrupted signature strictly rejected\n");

    // 5. Wrong Public Key Detection
    uint8_t bob_pub[32] = {0};
    uint8_t bob_priv[64] = {0};
    CryptoGenerateKeypair(bob_pub, bob_priv);
    assert(CryptoVerify(reinterpret_cast<const uint8_t*>(test_msg), msg_len, bob_pub, sig) == false);
    printf("  [PASS] Wrong public key verification strictly fails\n");

    // 6. SHA-256 Memory and File
    std::string h1 = ComputeMemorySha256("hello world", 11);
    assert(h1 == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    printf("  [PASS] SHA-256 memory hashing verified against standard vector\n");

    // 7. Trust Chain & Revocation
    ModManifest manifest;
    manifest.id = "community.alice.mod";
    manifest.author_pubkey = alice_pub_hex;
    std::string fake_pkg_hash = "1111111111111111111111111111111111111111111111111111111111111111";

    // Self-signed -> UnverifiedCommunity
    TrustLevel tl = DetermineTrustLevel(manifest, fake_pkg_hash);
    assert(tl == TrustLevel::UnverifiedCommunity);
    printf("  [PASS] Self-signed mod resolved to UnverifiedCommunity\n");

    // Revocation by public key
    auto& rev = GetGlobalRevocationList();
    rev.RevokeKey(alice_pub_hex);
    assert(DetermineTrustLevel(manifest, fake_pkg_hash) == TrustLevel::Revoked);
    printf("  [PASS] Key revocation actively blocks mod execution\n");

    // Revocation by package hash
    rev.Clear();
    rev.RevokePackage(fake_pkg_hash);
    assert(DetermineTrustLevel(manifest, fake_pkg_hash) == TrustLevel::Revoked);
    printf("  [PASS] Package hash revocation actively blocks mod execution\n");
    rev.Clear();

    // Official Key trust
    manifest.author_pubkey = ToHex(g_RefixRootPublicKey, 32);
    assert(DetermineTrustLevel(manifest, fake_pkg_hash) == TrustLevel::Official);
    printf("  [PASS] Root/Official key recognized as TrustLevel::Official\n");

    printf("[SUCCESS] All ReFix Mod Cryptography unit tests passed!\n");
    return 0;
}
