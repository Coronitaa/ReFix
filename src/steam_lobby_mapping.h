#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace refix::steam {

// High-avalanche 64-bit FNV-1a hash followed by Murmur3/SplitMix64 finalizer
inline uint32_t ComputeLobbyAccountId(const std::string& coreLobbyId) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (char c : coreLobbyId) {
        h ^= static_cast<uint8_t>(c);
        h *= 0x100000001B3ULL;
    }
    h ^= (h >> 33);
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= (h >> 33);
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= (h >> 33);
    uint32_t accountId = static_cast<uint32_t>(h ^ (h >> 32));
    if (accountId == 0) accountId = 1;
    return accountId;
}

// Canonical Steamworks Lobby CSteamID:
// - Universe: k_EUniversePublic (1)
// - AccountType: k_EAccountTypeChat (8)
// - Instance: k_EChatInstanceFlagLobby (0x40000)
// - AccountID: deterministic 32-bit ID derived from LanCore Lobby ID
inline uint64_t ComputeLobbySteamID(const std::string& coreLobbyId) {
    if (coreLobbyId.empty()) return 0;
    uint32_t accountId = ComputeLobbyAccountId(coreLobbyId);
    return (1ULL << 56) | (8ULL << 52) | (0x40000ULL << 32) | static_cast<uint64_t>(accountId);
}

// Centralized bidirectional 1:1 mapping manager between LanCore Lobby IDs and Steamworks CSteamIDs.
class SteamLobbyRegistry {
public:
    static SteamLobbyRegistry& Get() {
        static SteamLobbyRegistry s_instance;
        return s_instance;
    }

    uint64_t EnsureSteamLobbyID(const std::string& coreLobbyId, uint64_t fallbackId = 0) {
        if (coreLobbyId.empty()) return (fallbackId != 0) ? fallbackId : 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_coreToSteam.find(coreLobbyId);
        if (it != m_coreToSteam.end()) return it->second;

        uint64_t steamLobbyId = (fallbackId != 0) ? fallbackId : ComputeLobbySteamID(coreLobbyId);

        // Explicit collision policy: detect if steamLobbyId already belongs to a different coreLobbyId
        auto revIt = m_steamToCore.find(steamLobbyId);
        if (revIt != m_steamToCore.end() && revIt->second != coreLobbyId) {
            return 0; // Collision rejected
        }

        m_coreToSteam[coreLobbyId] = steamLobbyId;
        m_steamToCore[steamLobbyId] = coreLobbyId;
        return steamLobbyId;
    }

    std::string GetCoreLobbyId(uint64_t steamLobbyId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_steamToCore.find(steamLobbyId);
        if (it != m_steamToCore.end()) return it->second;
        return "";
    }

    uint64_t GetSteamLobbyId(const std::string& coreLobbyId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_coreToSteam.find(coreLobbyId);
        if (it != m_coreToSteam.end()) return it->second;
        return 0;
    }

    bool UnregisterLobby(const std::string& coreLobbyId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_coreToSteam.find(coreLobbyId);
        if (it != m_coreToSteam.end()) {
            m_steamToCore.erase(it->second);
            m_coreToSteam.erase(it);
            return true;
        }
        return false;
    }

    bool UnregisterBySteamId(uint64_t steamLobbyId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_steamToCore.find(steamLobbyId);
        if (it != m_steamToCore.end()) {
            m_coreToSteam.erase(it->second);
            m_steamToCore.erase(it);
            return true;
        }
        return false;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_coreToSteam.clear();
        m_steamToCore.clear();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coreToSteam.size();
    }

    bool HasCoreLobby(const std::string& coreLobbyId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coreToSteam.find(coreLobbyId) != m_coreToSteam.end();
    }

    bool HasSteamLobby(uint64_t steamLobbyId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_steamToCore.find(steamLobbyId) != m_steamToCore.end();
    }

private:
    SteamLobbyRegistry() = default;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, uint64_t> m_coreToSteam;
    std::unordered_map<uint64_t, std::string> m_steamToCore;
};

} // namespace refix::steam
