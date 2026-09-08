// ReFix EOS v3 - Steam transport backend (WAN).
//
// The LAN backend finds players on the same subnet. This one finds them
// anywhere, by putting the lobby on Steam's own matchmaking service and sending
// game traffic through SteamNetworkingMessages, which relays through Valve when
// a direct path is not available. That also brings along the two things players
// actually expect: the friends list and Steam invites.
//
// Nothing here depends on ReFix's Steam emulator. The backend binds the
// Steamworks flat C API out of whatever `steam_api64.dll` is already loaded in
// the process, so it works the same against the real Steam client or against an
// emulator that exports the same surface. If no such DLL is present the backend
// simply reports itself unavailable and the LAN path carries on alone.
#pragma once
#include "../core/refix_common.h"
#include "../core/lobby_model.h"
#include <functional>

namespace refix {

struct SteamFriendInfo {
    uint64_t    SteamId = 0;
    std::string PersonaName;
    bool        NameResolved = false;   // false while Steam is still fetching it
    bool        InSameGame = false;
    uint64_t    LobbyId = 0;      // the friend's current lobby, if joinable
};

class SteamBackend {
public:
    static SteamBackend& Get();

    // Binds the Steamworks exports and registers callbacks. Safe to call often.
    bool Start();
    bool Available() const { return m_available; }

    uint64_t    LocalSteamId() const;
    std::string LocalPersonaName() const;

    // Who we are, as the EOS layer resolved it. Published as lobby member data
    // so the host can tie a Steam member to its EOS ProductUserId.
    void SetLocalIdentity(const std::string& puid, const std::string& name);

    // --- hosting ----------------------------------------------------------
    // Creates (once) and keeps a Steam lobby in step with an EOS lobby we own.
    void PublishLobby(const LobbyRecord& record);
    void WithdrawLobby(const std::string& lobbyId);

    // --- discovery --------------------------------------------------------
    void RequestList();
    std::vector<LobbyRecord> Snapshot() const;

    // --- joining ----------------------------------------------------------
    bool JoinByLobbyId(const std::string& lobbyId);   // our EOS lobby id
    bool JoinSteamLobby(uint64_t steamLobbyId);
    void LeaveLobby(const std::string& lobbyId);

    // --- messaging --------------------------------------------------------
    bool SendTo(uint64_t steamId, const void* data, size_t len);

    // --- friends and invites ---------------------------------------------
    std::vector<SteamFriendInfo> Friends() const;
    bool InviteToLobby(const std::string& lobbyId, uint64_t steamId);
    bool OpenInviteOverlay(const std::string& lobbyId);

    // Called every EOS_Platform_Tick: drains Steam messages and our own
    // deferred work. Steam's callbacks are dispatched by the game's own
    // SteamAPI_RunCallbacks, so this does not run them itself.
    void Pump();

    // Raised from the Steam callback thread.
    std::function<void(uint64_t fromSteamId, const uint8_t* data, size_t len)> OnMessage;
    std::function<void(const LobbyRecord&)>                                     OnLobbyDiscovered;
    std::function<void(const std::string& lobbyId, uint64_t steamId,
                       const std::string& puid, const std::string& name, bool joined)> OnMemberChanged;
    std::function<void(const std::string& lobbyId, uint64_t fromSteamId)>        OnInviteReceived;
    std::function<void(const std::string& lobbyId, uint64_t fromSteamId)>        OnJoinRequested;

    // The AppID published so other emulators recognise the lobby as belonging
    // to this game, and the one we look for in theirs.
    const std::string& GameAppId() const { return m_gameAppId; }

private:
    SteamBackend() = default;
    void HandleLobbyList(uint32_t count);
    void DrainDeferred();
    void AdoptCreatedLobby(int result, uint64_t steamLobby);
    void WriteLobbyRecord(uint64_t steamLobby, const LobbyRecord& record);
    bool ReadLobbyRecord(uint64_t steamLobby, LobbyRecord& out);

    bool         m_available = false;
    std::string  m_gameAppId;
    // Which spelling of the AppID key the next lobby-list request filters on.
    mutable int  m_listSweep = 0;
    uint64_t     m_lastListMs = 0;
};

} // namespace refix
