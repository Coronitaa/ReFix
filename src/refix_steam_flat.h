#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Thin, thread-safe access to the real Steam flat API (the original steam_api64 loaded by steam_proxy) for
// ReFix features that need friends' rich presence or raw ISteamNetworkingMessages channels.
// Every call degrades to a no-op / empty result when Steam or the export is unavailable.
namespace ReFixSteamFlat {
    // Local SteamID64 (0 until Steam is initialized).
    uint64_t LocalSteamID();

    // Sets (or clears, with an empty value) a rich presence key of the local user.
    bool SetRichPresence(const char* key, const std::string& value);

    struct FriendPresence {
        uint64_t steamID = 0;
        std::string value;
    };
    // Immediate friends whose rich presence `key` is non-empty, from a cache refreshed on the main thread
    // (ServiceMainThread). Safe to call from any thread; registers `key` for the next refresh.
    std::vector<FriendPresence> FriendsWithRichPresence(const char* key);

    // Immediate friend SteamIDs from the cache refreshed on the main thread. Safe to call from any thread.
    std::vector<uint64_t> FriendSteamIDs();

    // Services Steam friend/presence state on the game's MAIN thread (call from the Steam callback pumps):
    // applies queued SetRichPresence writes and refreshes the friend caches. ISteamFriends enumeration is
    // unsafe off the main thread, so worker threads must rely on the caches instead of calling Steam.
    void ServiceMainThread();

    // Rich presence `key` of one user (friends only; empty when unknown).
    std::string FriendRichPresence(uint64_t steamID, const char* key);

    // Owner of a Steam lobby the local user can see (0 when unknown).
    uint64_t LobbyOwner(uint64_t lobbyId);

    // Sends a message over Steam Datagram Relay on `channel`. When `reliable` is true,
    // uses k_nSteamNetworkingSend_ReliableNoNagle | k_nSteamNetworkingSend_AutoRestartBrokenSession.
    bool SendMessage(uint64_t steamID, const void* data, uint32_t size, int channel, bool reliable = false);

    // Currently active Steam lobby ID if known (0 when not in a lobby).
    uint64_t ActiveLobbyID();

    // Drains up to `maxMessages` messages received on `channel`, invoking `onMessage` for each.
    int ReceiveMessages(int channel, int maxMessages, const std::function<void(uint64_t, const uint8_t*, uint32_t)>& onMessage);

    // Process-wide SteamNetworkingSockets settings; 0 leaves a value at Steam's default.
    struct NetworkingConfig {
        int32_t iceEnable = 0;          // k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_* bits (direct routes)
        int32_t sendRateMin = 0;        // bytes/s
        int32_t sendRateMax = 0;        // bytes/s
        int32_t timeoutInitialMs = 0;   // connect phase
        int32_t timeoutConnectedMs = 0; // established session
    };
    // Starts relay network access and applies `config`. False until ISteamNetworkingUtils is available.
    bool ConfigureNetworking(const NetworkingConfig& config);

    struct SessionStatus {
        int state = 0;                  // ESteamNetworkingConnectionState
        bool relayed = false;           // SDR/TURN instead of a direct route
        int pingMs = -1;
        float qualityLocal = -1.0f;     // fraction of packets delivered, -1 unknown
        int64_t queueTimeUs = 0;        // how long a new message would wait in the send queue
        int pendingUnreliable = 0, pendingReliable = 0, sendRate = 0;
    };
    // Route and quality of the SteamNetworkingMessages session with steamID (false when there is none).
    bool SessionInfo(uint64_t steamID, SessionStatus& out);

    // Accepts a pending SteamNetworkingMessages session from steamID (false when none is pending).
    bool AcceptSession(uint64_t steamID);

    // Handles a SteamNetworkingMessagesSessionRequest_t (callback 1251) payload by accepting the session.
    void OnSessionRequestCallback(const void* param, int32_t size);

    // Accepts pending sessions from every immediate friend (cheap; for hosts that miss callback 1251).
    void AcceptFriendSessions();
}
