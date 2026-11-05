#include "../../bindings/cpp/refix_mod.hpp"

// Callback triggered whenever a game packet arrives on declared channel 1
extern "C" __attribute__((visibility("default"))) int on_net_packet(RefixPeerHandle peer, uint32_t channel, const uint8_t* data, uint32_t len) {
    refix::Mod::LogInfo("Intercepted game packet on channel: " + std::to_string(channel));

    // Privacy Gate guarantee: peer is an opaque ephemeral handle, never an IP or physical address
    std::string sender_name = refix::Mod::GetPeerDisplayName(peer);
    refix::Mod::LogInfo("Packet received from peer: " + sender_name);

    return 0; // 0 = accept and forward, 1 = drop
}

extern "C" __attribute__((visibility("default"))) int refix_init() {
    refix::Mod::LogInfo("Protocol override initialized on channel 1");
    return 0;
}
