#pragma once

#include "mod_types.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>

namespace refix::mods {

using PeerHandle = uint64_t;

struct RealPeerIdentity {
    std::string real_id;        // e.g. SteamID64, EOS ID, or LAN Transport GUID
    std::string ip_address;     // Physical IP (NEVER given to mod)
    uint16_t port = 0;          // Physical port (NEVER given to mod)
    std::string display_name;   // Visible player name
};

class PrivacyGate {
public:
    PrivacyGate();

    // Initialize session salt with secure cryptographic entropy
    void InitializeSession();

    // Register a remote player in the transport layer
    void RegisterPeer(const std::string& real_id,
                      const std::string& ip_address,
                      uint16_t port,
                      const std::string& display_name);

    // Unregister a remote player on disconnect
    void UnregisterPeer(const std::string& real_id);

    // Derive the opaque ephemeral PeerHandle for a specific mod and real peer ID.
    // PeerHandle = Truncate64(HMAC-SHA256(SessionSalt ^ ModSalt, real_id))
    PeerHandle GetPeerHandleForMod(const std::string& mod_id, const std::string& real_id);

    // Reverse lookup: transport layer only! Given a mod's PeerHandle, retrieve real ID.
    bool ResolveRealIdFromHandle(const std::string& mod_id, PeerHandle handle, std::string& out_real_id) const;

    // Get display name for a peer handle, filtered by mod capabilities
    bool GetPeerDisplayName(const std::string& mod_id,
                            PeerHandle handle,
                            const std::set<std::string>& mod_approved_caps,
                            std::string& out_display_name) const;

    // Filter incoming/outgoing game payload:
    // Ensures only game payload on permitted channels is visible to mod.
    // Blocks transport headers, presence packets, auth/identity frames.
    bool CanModInspectChannel(const std::string& mod_id,
                              uint32_t channel,
                              const std::set<std::string>& mod_approved_caps) const;

    bool CanModOverrideChannel(const std::string& mod_id,
                               uint32_t channel,
                               const std::set<std::string>& mod_approved_caps) const;

    // Declare which game channels a mod operates on
    void RegisterModChannel(const std::string& mod_id, uint32_t channel);

private:
    uint8_t session_salt_[32];
    std::map<std::string, RealPeerIdentity> peers_by_real_id_;

    // Map: mod_id -> (real_id -> PeerHandle) and (PeerHandle -> real_id)
    std::map<std::string, std::map<std::string, PeerHandle>> mod_real_to_handle_;
    std::map<std::string, std::map<PeerHandle, std::string>> mod_handle_to_real_;

    // Allowed channels per mod
    std::map<std::string, std::set<uint32_t>> mod_declared_channels_;

    void DeriveModSalt(const std::string& mod_id, uint8_t out_combined_salt[32]);
};

// Global Privacy Gate instance
PrivacyGate& GetGlobalPrivacyGate();

} // namespace refix::mods
