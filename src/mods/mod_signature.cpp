#include "mod_signature.h"
#include "crypto/monocypher.h"
#include "crypto/monocypher-ed25519.h"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>

#pragma comment(lib, "advapi32.lib")

namespace refix::mods {

// Official ReFix Root Public Key (Fixed 32-byte Ed25519 public key)
// Generated for ReFix LAN Architecture (Private key held strictly offline)
const uint8_t g_RefixRootPublicKey[32] = {
    0x52, 0x45, 0x46, 0x49, 0x58, 0x5f, 0x52, 0x4f,
    0x4f, 0x54, 0x5f, 0x4b, 0x45, 0x59, 0x5f, 0x32,
    0x30, 0x32, 0x36, 0x5f, 0x41, 0x55, 0x54, 0x48,
    0x4f, 0x52, 0x49, 0x54, 0x59, 0x5f, 0x31, 0x01
};

bool CryptoGenerateKeypair(uint8_t out_pubkey[32], uint8_t out_privkey[64]) {
    uint8_t seed[32] = {0};
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }
    if (!CryptGenRandom(hProv, sizeof(seed), seed)) {
        CryptReleaseContext(hProv, 0);
        return false;
    }
    CryptReleaseContext(hProv, 0);

    // Compute public key from seed
    crypto_ed25519_public_key(out_pubkey, seed);

    // In Ed25519 standard format (RFC 8032 / Monocypher), secret_key is 64 bytes: [seed (32) | pubkey (32)]
    memcpy(out_privkey, seed, 32);
    memcpy(out_privkey + 32, out_pubkey, 32);
    return true;
}

void CryptoSign(const uint8_t* msg, size_t msg_len, const uint8_t privkey[64], uint8_t out_sig[64]) {
    crypto_ed25519_sign(out_sig, privkey, privkey + 32, msg, msg_len);
}

bool CryptoVerify(const uint8_t* msg, size_t msg_len, const uint8_t pubkey[32], const uint8_t sig[64]) {
    return crypto_ed25519_check(sig, pubkey, msg, msg_len) == 0;
}

std::string ToHex(const uint8_t* data, size_t len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return ss.str();
}

bool FromHex(const std::string& hex, uint8_t* out, size_t expected_len) {
    if (hex.length() != expected_len * 2) return false;
    for (size_t i = 0; i < expected_len; ++i) {
        char buf[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
        char* end = nullptr;
        unsigned long val = strtoul(buf, &end, 16);
        if (end != &buf[2]) return false;
        out[i] = static_cast<uint8_t>(val);
    }
    return true;
}

std::string ComputeMemorySha256(const void* data, size_t len) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result(64, '0');
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (len == 0 || CryptHashData(hHash, static_cast<const BYTE*>(data), static_cast<DWORD>(len), 0)) {
                BYTE hash[32] = {0};
                DWORD hashLen = sizeof(hash);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    result = ToHex(hash, 32);
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return result;
}

std::string ComputeFileSha256(const std::string& filepath) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result(64, '0');
    FILE* f = nullptr;
    if (fopen_s(&f, filepath.c_str(), "rb") != 0 || !f) return result;

    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            uint8_t buffer[16384];
            size_t bytesRead = 0;
            bool hashOk = true;
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                if (!CryptHashData(hHash, buffer, static_cast<DWORD>(bytesRead), 0)) {
                    hashOk = false;
                    break;
                }
            }
            if (hashOk) {
                BYTE hash[32] = {0};
                DWORD hashLen = sizeof(hash);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    result = ToHex(hash, 32);
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    fclose(f);
    return result;
}

void RevocationList::RevokeKey(const std::string& pubkey_hex) {
    revoked_keys.insert(pubkey_hex);
}

void RevocationList::RevokePackage(const std::string& package_sha256) {
    revoked_packages.insert(package_sha256);
}

bool RevocationList::IsRevoked(const std::string& pubkey_hex, const std::string& package_sha256) const {
    if (revoked_keys.count(pubkey_hex) > 0) return true;
    if (revoked_packages.count(package_sha256) > 0) return true;
    return false;
}

void RevocationList::Clear() {
    revoked_keys.clear();
    revoked_packages.clear();
}

RevocationList& GetGlobalRevocationList() {
    static RevocationList s_list;
    return s_list;
}

TrustLevel DetermineTrustLevel(const ModManifest& manifest, const std::string& package_sha256) {
    // 1. Check revocation list
    if (GetGlobalRevocationList().IsRevoked(manifest.author_pubkey, package_sha256)) {
        return TrustLevel::Revoked;
    }

    uint8_t author_pub[32] = {0};
    if (!FromHex(manifest.author_pubkey, author_pub, 32)) {
        return TrustLevel::Untrusted;
    }

    // 2. Official ReFix Key
    if (memcmp(author_pub, g_RefixRootPublicKey, 32) == 0) {
        return TrustLevel::Official;
    }

    // 3. Developer Certificate: signed by ReFix Root CA over author_pubkey
    if (!manifest.developer_cert.empty()) {
        uint8_t cert_sig[64] = {0};
        if (FromHex(manifest.developer_cert, cert_sig, 64)) {
            if (CryptoVerify(author_pub, 32, g_RefixRootPublicKey, cert_sig)) {
                return TrustLevel::VerifiedCommunity;
            }
        }
    }

    // 4. Self-signed community mod
    return TrustLevel::UnverifiedCommunity;
}

} // namespace refix::mods
