#pragma once

#include "mod_types.h"
#include <string>
#include <vector>
#include <set>

namespace refix::mods {

// Generates an Ed25519 keypair using OS cryptographic entropy.
// out_pubkey: 32 bytes, out_privkey: 64 bytes (seed + pubkey).
bool CryptoGenerateKeypair(uint8_t out_pubkey[32], uint8_t out_privkey[64]);

// Signs a message buffer using an Ed25519 private key (64 bytes).
void CryptoSign(const uint8_t* msg, size_t msg_len, const uint8_t privkey[64], uint8_t out_sig[64]);

// Verifies an Ed25519 signature (64 bytes) against a public key (32 bytes). Returns true if authentic.
bool CryptoVerify(const uint8_t* msg, size_t msg_len, const uint8_t pubkey[32], const uint8_t sig[64]);

// Hex encoding and decoding helpers
std::string ToHex(const uint8_t* data, size_t len);
bool FromHex(const std::string& hex, uint8_t* out, size_t expected_len);

// Computes SHA-256 hash of a file on disk (lowercase hex string).
std::string ComputeFileSha256(const std::string& filepath);

// Computes SHA-256 hash of memory buffer.
std::string ComputeMemorySha256(const void* data, size_t len);

// Revocation registry for compromised author keys and revoked mod packages
class RevocationList {
    std::set<std::string> revoked_keys;     // Revoked pubkey hex
    std::set<std::string> revoked_packages; // Revoked package SHA-256 hex
public:
    void RevokeKey(const std::string& pubkey_hex);
    void RevokePackage(const std::string& package_sha256);
    bool IsRevoked(const std::string& pubkey_hex, const std::string& package_sha256) const;
    void Clear();
};

// Global Revocation List instance
RevocationList& GetGlobalRevocationList();

// ReFix Official Root Public Key (32 bytes)
extern const uint8_t g_RefixRootPublicKey[32];

// Determines trust level of a verified manifest
TrustLevel DetermineTrustLevel(const ModManifest& manifest, const std::string& package_sha256);

} // namespace refix::mods
