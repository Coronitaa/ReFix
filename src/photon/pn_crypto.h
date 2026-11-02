#pragma once
#include <string>

// Photon payload encryption, server side (Photon.SocketServer.Security.DiffieHellmanCryptoProvider):
// Diffie-Hellman over the 768-bit Oakley group (generator 22); the AES-256-CBC key is SHA-256 of the shared
// secret, IV all zeros, PKCS7 padding. Big integers travel as minimal big-endian byte strings.
class PnDiffieHellman {
public:
    PnDiffieHellman();                 // generates a random 160-bit secret
    std::string PublicKey() const;     // minimal big-endian g^secret mod p
    // Derives the AES key from the client's public key; false on invalid input.
    bool DeriveKey(const std::string& clientPublicKey, std::string& aesKey) const;

private:
    std::string m_secret; // big-endian
};

namespace PnCrypto {
    bool AesCbcDecrypt(const std::string& key, const std::string& cipher, std::string& plain);
    bool AesCbcEncrypt(const std::string& key, const std::string& plain, std::string& cipher);
    // Modular exponentiation on big-endian byte strings (exposed for tests).
    std::string PowMod(const std::string& base, const std::string& exponent, const std::string& modulus);
}
