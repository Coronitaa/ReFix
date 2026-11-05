#include "privacy_gate.h"
#include "mod_signature.h"

#include <windows.h>
#include <wincrypt.h>
#include <iomanip>
#include <sstream>

namespace refix::mods {

PrivacyGate::PrivacyGate() {
    InitializeSession();
}

void PrivacyGate::InitializeSession() {
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, sizeof(session_salt_), session_salt_);
        CryptReleaseContext(hProv, 0);
    } else {
        // Fallback entropy
        uint64_t tick = GetTickCount64();
        uint64_t qpc = 0;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&qpc));
        memcpy(session_salt_, &tick, 8);
        memcpy(session_salt_ + 8, &qpc, 8);
        memset(session_salt_ + 16, 0x5A, 16);
    }

    peers_by_real_id_.clear();
    mod_real_to_handle_.clear();
    mod_handle_to_real_.clear();
    mod_declared_channels_.clear();
}

void PrivacyGate::RegisterPeer(const std::string& real_id,
                               const std::string& ip_address,
                               uint16_t port,
                               const std::string& display_name) {
    if (real_id.empty()) return;

    RealPeerIdentity identity;
    identity.real_id = real_id;
    identity.ip_address = ip_address;
    identity.port = port;
    identity.display_name = display_name;

    peers_by_real_id_[real_id] = identity;
}

void PrivacyGate::UnregisterPeer(const std::string& real_id) {
    peers_by_real_id_.erase(real_id);
    for (auto& [mod_id, map_real] : mod_real_to_handle_) {
        auto it = map_real.find(real_id);
        if (it != map_real.end()) {
            PeerHandle h = it->second;
            map_real.erase(it);
            mod_handle_to_real_[mod_id].erase(h);
        }
    }
}

void PrivacyGate::DeriveModSalt(const std::string& mod_id, uint8_t out_combined_salt[32]) {
    std::string mod_hash = ComputeMemorySha256(mod_id.data(), mod_id.size());
    uint8_t mod_salt_bytes[32];
    FromHex(mod_hash, mod_salt_bytes, 32);

    for (int i = 0; i < 32; ++i) {
        out_combined_salt[i] = session_salt_[i] ^ mod_salt_bytes[i];
    }
}

PeerHandle PrivacyGate::GetPeerHandleForMod(const std::string& mod_id, const std::string& real_id) {
    auto& r_map = mod_real_to_handle_[mod_id];
    auto it = r_map.find(real_id);
    if (it != r_map.end()) {
        return it->second;
    }

    uint8_t combined_salt[32];
    DeriveModSalt(mod_id, combined_salt);

    // Compute SHA-256 over (CombinedSalt || RealID)
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), combined_salt, combined_salt + 32);
    payload.insert(payload.end(), real_id.begin(), real_id.end());

    std::string derived_hex = ComputeMemorySha256(payload.data(), payload.size());
    uint8_t hash_bytes[32];
    FromHex(derived_hex, hash_bytes, 32);

    PeerHandle handle = 0;
    memcpy(&handle, hash_bytes, sizeof(PeerHandle));
    if (handle == 0) handle = 1; // 0 reserved for invalid/null handle

    // Store bi-directional mappings
    r_map[real_id] = handle;
    mod_handle_to_real_[mod_id][handle] = real_id;

    return handle;
}

bool PrivacyGate::ResolveRealIdFromHandle(const std::string& mod_id, PeerHandle handle, std::string& out_real_id) const {
    auto mod_it = mod_handle_to_real_.find(mod_id);
    if (mod_it != mod_handle_to_real_.end()) {
        auto h_it = mod_it->second.find(handle);
        if (h_it != mod_it->second.end()) {
            out_real_id = h_it->second;
            return true;
        }
    }
    return false;
}

bool PrivacyGate::GetPeerDisplayName(const std::string& mod_id,
                                     PeerHandle handle,
                                     const std::set<std::string>& mod_approved_caps,
                                     std::string& out_display_name) const {
    std::string real_id;
    if (!ResolveRealIdFromHandle(mod_id, handle, real_id)) {
        return false;
    }

    auto peer_it = peers_by_real_id_.find(real_id);
    if (peer_it == peers_by_real_id_.end()) {
        return false;
    }

    // Capability check: players.display_name
    if (mod_approved_caps.find(Capability::PlayersDisplayName) != mod_approved_caps.end()) {
        out_display_name = peer_it->second.display_name;
    } else {
        // Redact to anonymized handle
        std::ostringstream ss;
        ss << "Player_" << std::hex << std::setw(4) << std::setfill('0') << (handle & 0xFFFF);
        out_display_name = ss.str();
    }

    return true;
}

void PrivacyGate::RegisterModChannel(const std::string& mod_id, uint32_t channel) {
    mod_declared_channels_[mod_id].insert(channel);
}

bool PrivacyGate::CanModInspectChannel(const std::string& mod_id,
                                       uint32_t channel,
                                       const std::set<std::string>& mod_approved_caps) const {
    if (mod_approved_caps.find(Capability::NetInspectChannel) == mod_approved_caps.end() &&
        mod_approved_caps.find(Capability::NetOverrideChannel) == mod_approved_caps.end()) {
        return false;
    }

    auto it = mod_declared_channels_.find(mod_id);
    if (it != mod_declared_channels_.end()) {
        return it->second.find(channel) != it->second.end();
    }
    return false;
}

bool PrivacyGate::CanModOverrideChannel(const std::string& mod_id,
                                        uint32_t channel,
                                        const std::set<std::string>& mod_approved_caps) const {
    if (mod_approved_caps.find(Capability::NetOverrideChannel) == mod_approved_caps.end()) {
        return false;
    }

    auto it = mod_declared_channels_.find(mod_id);
    if (it != mod_declared_channels_.end()) {
        return it->second.find(channel) != it->second.end();
    }
    return false;
}

PrivacyGate& GetGlobalPrivacyGate() {
    static PrivacyGate s_instance;
    return s_instance;
}

} // namespace refix::mods
