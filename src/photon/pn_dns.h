#pragma once
#include <string>
#include <vector>

// Resolves configured host suffixes (e.g. Photon Cloud's "photonengine.io") to 127.0.0.1 by hooking
// ws2_32 getaddrinfo / GetAddrInfoW / gethostbyname, so any runtime (Mono, IL2CPP, native) that resolves
// a cloud host reaches the local emulator instead.
namespace PnDns {
    bool Install(const std::vector<std::string>& hostSuffixes);
}
