#include <windows.h>
#include <bcrypt.h>
#include "pn_crypto.h"
#include <cstdint>
#include <random>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace {

// RFC 2409 First Oakley Group (768-bit MODP), big-endian.
const uint8_t kPrime768[96] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x3A, 0x36, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
constexpr uint8_t kGenerator = 22;
constexpr size_t kSecretBytes = 20; // 160-bit, as the Photon client
constexpr size_t kAesKeyBytes = 32;
constexpr size_t kAesBlock = 16;

using Limbs = std::vector<uint32_t>; // little-endian 32-bit limbs

Limbs FromBE(const std::string& be) {
    Limbs out((be.size() + 3) / 4, 0);
    for (size_t i = 0; i < be.size(); i++) {
        size_t bit = (be.size() - 1 - i) * 8;
        out[bit / 32] |= (uint32_t)(uint8_t)be[i] << (bit % 32);
    }
    return out;
}

std::string ToBE(const Limbs& v) {
    std::string be;
    for (size_t i = v.size(); i-- > 0;) for (int s = 24; s >= 0; s -= 8) be += (char)(v[i] >> s);
    size_t first = be.find_first_not_of('\0');
    return first == std::string::npos ? std::string(1, '\0') : be.substr(first);
}

void Trim(Limbs& v) { while (!v.empty() && v.back() == 0) v.pop_back(); }

int Compare(const Limbs& a, const Limbs& b) { // both trimmed
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0;) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

void SubInPlace(Limbs& a, const Limbs& b) { // a >= b
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); i++) {
        int64_t d = (int64_t)a[i] - (i < b.size() ? b[i] : 0) - borrow;
        borrow = d < 0;
        a[i] = (uint32_t)(d + (borrow << 32));
    }
    Trim(a);
}

// Binary long division remainder; the operands are small (<= 1536 bits) and this runs once per connection.
Limbs Mod(const Limbs& value, const Limbs& m) {
    Limbs r;
    for (size_t bit = value.size() * 32; bit-- > 0;) {
        uint32_t carry = (value[bit / 32] >> (bit % 32)) & 1;
        for (uint32_t& limb : r) { uint32_t next = limb >> 31; limb = limb << 1 | carry; carry = next; }
        if (carry) r.push_back(carry);
        Trim(r);
        if (Compare(r, m) >= 0) SubInPlace(r, m);
    }
    return r;
}

Limbs MulMod(const Limbs& a, const Limbs& b, const Limbs& m) {
    Limbs p(a.size() + b.size() + 1, 0);
    for (size_t i = 0; i < a.size(); i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); j++) {
            uint64_t t = (uint64_t)a[i] * b[j] + p[i + j] + carry;
            p[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        p[i + b.size()] += (uint32_t)carry;
    }
    Trim(p);
    return Mod(p, m);
}

bool Sha256(const std::string& data, std::string& digest) {
    digest.assign(32, '\0');
    return BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PUCHAR)data.data(), (ULONG)data.size(),
                                     (PUCHAR)&digest[0], (ULONG)digest.size()));
}

// One-shot AES-256-CBC with a zero IV and PKCS7 padding.
bool AesCbc(const std::string& key, const std::string& in, std::string& out, bool encrypt) {
    if (key.size() != kAesKeyBytes) return false;
    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(BCRYPT_AES_CBC_ALG_HANDLE, &handle, nullptr, 0, (PUCHAR)key.data(),
                                                   (ULONG)key.size(), 0))) {
        return false;
    }
    uint8_t iv[kAesBlock] = { 0 };
    ULONG size = 0;
    NTSTATUS st = encrypt
        ? BCryptEncrypt(handle, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv, sizeof(iv), nullptr, 0, &size, BCRYPT_BLOCK_PADDING)
        : BCryptDecrypt(handle, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv, sizeof(iv), nullptr, 0, &size, BCRYPT_BLOCK_PADDING);
    if (BCRYPT_SUCCESS(st)) {
        out.assign(size, '\0');
        memset(iv, 0, sizeof(iv));
        st = encrypt
            ? BCryptEncrypt(handle, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv, sizeof(iv), (PUCHAR)&out[0], size, &size, BCRYPT_BLOCK_PADDING)
            : BCryptDecrypt(handle, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, iv, sizeof(iv), (PUCHAR)&out[0], size, &size, BCRYPT_BLOCK_PADDING);
        out.resize(size);
    }
    BCryptDestroyKey(handle);
    return BCRYPT_SUCCESS(st);
}

std::string PrimeBE() { return std::string((const char*)kPrime768, sizeof(kPrime768)); }

} // namespace

namespace PnCrypto {

std::string PowMod(const std::string& base, const std::string& exponent, const std::string& modulus) {
    Limbs m = FromBE(modulus);
    Trim(m);
    Limbs b = FromBE(base);
    Trim(b);
    b = Mod(b, m);
    Limbs result{ 1 };
    Limbs e = FromBE(exponent);
    for (size_t bit = e.size() * 32; bit-- > 0;) {
        result = MulMod(result, result, m);
        if ((e[bit / 32] >> (bit % 32)) & 1) result = MulMod(result, b, m);
    }
    return ToBE(result);
}

bool AesCbcDecrypt(const std::string& key, const std::string& cipher, std::string& plain) {
    return AesCbc(key, cipher, plain, false);
}

bool AesCbcEncrypt(const std::string& key, const std::string& plain, std::string& cipher) {
    return AesCbc(key, plain, cipher, true);
}

} // namespace PnCrypto

PnDiffieHellman::PnDiffieHellman() {
    std::random_device rd;
    m_secret.resize(kSecretBytes);
    for (char& c : m_secret) c = (char)(rd() & 0xFF);
    m_secret[0] = (char)((uint8_t)m_secret[0] | 0x80); // full 160-bit secret
}

std::string PnDiffieHellman::PublicKey() const {
    return PnCrypto::PowMod(std::string(1, (char)kGenerator), m_secret, PrimeBE());
}

bool PnDiffieHellman::DeriveKey(const std::string& clientPublicKey, std::string& aesKey) const {
    if (clientPublicKey.empty() || clientPublicKey.size() > sizeof(kPrime768)) return false;
    std::string shared = PnCrypto::PowMod(clientPublicKey, m_secret, PrimeBE());
    return Sha256(shared, aesKey);
}
