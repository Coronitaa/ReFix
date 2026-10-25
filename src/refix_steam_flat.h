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
    // Immediate friends whose rich presence `key` is non-empty. Also asks Steam to refresh their presence.
    std::vector<FriendPresence> FriendsWithRichPresence(const char* key);

    // Rich presence `key` of one user (friends only; empty when unknown).
    std::string FriendRichPresence(uint64_t steamID, const char* key);

    // Unreliable, no-Nagle send over Steam Datagram Relay on `channel`.
    bool SendMessage(uint64_t steamID, const void* data, uint32_t size, int channel);

    // Drains up to `maxMessages` messages received on `channel`, invoking `onMessage` for each.
    int ReceiveMessages(int channel, int maxMessages, const std::function<void(uint64_t, const uint8_t*, uint32_t)>& onMessage);

    // Accepts a pending SteamNetworkingMessages session from steamID (false when none is pending).
    bool AcceptSession(uint64_t steamID);

    // Handles a SteamNetworkingMessagesSessionRequest_t (callback 1251) payload by accepting the session.
    void OnSessionRequestCallback(const void* param, int32_t size);

    // Accepts pending sessions from every immediate friend (cheap; for hosts that miss callback 1251).
    void AcceptFriendSessions();
}
