#pragma once
#include <string>
#include <vector>

// Resolves configured host suffixes (e.g. Photon Cloud's "photonengine.io") to a loopback address by hooking
// ws2_32 getaddrinfo / GetAddrInfoW / gethostbyname, so any runtime (Mono, IL2CPP, native) that resolves
// a cloud host reaches the local emulator instead.
namespace PnDns {
    // `target`: the emulator's listen address (127.0.0.1, or 127.0.0.N for further instances on one PC).
    bool Install(const std::vector<std::string>& hostSuffixes, const std::string& target = "127.0.0.1");
}
