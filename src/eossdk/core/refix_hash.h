// ReFix EOS v3 - hashing.
//
// Identities are derived, never invented: a SHA-256 over stable inputs gives
// every player the same EOS ids on every machine and every run, which is what
// lets two independent installs recognise each other.
#pragma once
#include "refix_common.h"

namespace refix {

void        Sha256(const void* data, size_t len, uint8_t out[32]);
std::string Sha256Hex(const std::string& s);

// First `bytes` of SHA-256(input) as lowercase hex - EOS ids are 32 hex chars,
// i.e. 16 bytes.
std::string DerivedId(const std::string& input, size_t bytes = 16);

} // namespace refix
