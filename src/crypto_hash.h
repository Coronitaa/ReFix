#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

#pragma comment(lib, "advapi32.lib")

namespace ReFixCrypto {

inline std::string ComputeSHA256Hex(const void* data, size_t len) {
    if (!data || len == 0) return "0000000000000000000000000000000000000000000000000000000000000000";
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result = "0000000000000000000000000000000000000000000000000000000000000000";
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (const BYTE*)data, (DWORD)len, 0)) {
                BYTE hash[32] = { 0 };
                DWORD hashLen = sizeof(hash);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    char hex[65] = { 0 };
                    for (DWORD i = 0; i < hashLen; i++) {
                        sprintf_s(&hex[i * 2], 3, "%02x", hash[i]);
                    }
                    result = hex;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return result;
}

inline std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.length() % 2 != 0) return bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        char byteString[3] = { hex[i], hex[i + 1], '\0' };
        char* endPtr = nullptr;
        uint8_t byte = (uint8_t)strtoul(byteString, &endPtr, 16);
        if (endPtr != &byteString[2]) return std::vector<uint8_t>();
        bytes.push_back(byte);
    }
    return bytes;
}

} // namespace ReFixCrypto
