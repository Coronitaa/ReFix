#include "steam_backend.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "refix_wire.h"

#include "steam_api_common.h"
#include "isteammatchmaking.h"
#include "isteamfriends.h"
#include "isteamuser.h"
#include "isteamnetworkingmessages.h"

#include <atomic>

namespace refix {
namespace {

// ---------------------------------------------------------------------------
// Dynamic binding
//
// We deliberately bind by name out of the already-loaded steam_api64.dll rather
// than linking the import library: the emulator must work whether that DLL is
// Valve's, ReFix's own proxy, or another emulator entirely.
// ---------------------------------------------------------------------------
struct SteamApi {
    HMODULE Module = nullptr;

    ISteamMatchmaking*         Matchmaking = nullptr;
    ISteamFriends*             FriendsIface = nullptr;
    ISteamUser*                User = nullptr;
    ISteamNetworkingMessages*  Messages = nullptr;

    // matchmaking
    SteamAPICall_t (*CreateLobby)(ISteamMatchmaking*, ELobbyType, int) = nullptr;
    SteamAPICall_t (*JoinLobby)(ISteamMatchmaking*, uint64) = nullptr;
    void           (*LeaveLobby)(ISteamMatchmaking*, uint64) = nullptr;
    steam_bool     (*SetLobbyData)(ISteamMatchmaking*, uint64, const char*, const char*) = nullptr;
    const char*    (*GetLobbyData)(ISteamMatchmaking*, uint64, const char*) = nullptr;
    int            (*GetLobbyDataCount)(ISteamMatchmaking*, uint64) = nullptr;
    steam_bool     (*GetLobbyDataByIndex)(ISteamMatchmaking*, uint64, int, char*, int, char*, int) = nullptr;
    void           (*SetLobbyMemberData)(ISteamMatchmaking*, uint64, const char*, const char*) = nullptr;
    const char*    (*GetLobbyMemberData)(ISteamMatchmaking*, uint64, uint64, const char*) = nullptr;
    int            (*GetNumLobbyMembers)(ISteamMatchmaking*, uint64) = nullptr;
    uint64         (*GetLobbyMemberByIndex)(ISteamMatchmaking*, uint64, int) = nullptr;
    uint64         (*GetLobbyOwner)(ISteamMatchmaking*, uint64) = nullptr;
    steam_bool     (*SetLobbyMemberLimit)(ISteamMatchmaking*, uint64, int) = nullptr;
    steam_bool     (*SetLobbyJoinable)(ISteamMatchmaking*, uint64, bool) = nullptr;
    steam_bool     (*SetLobbyType)(ISteamMatchmaking*, uint64, ELobbyType) = nullptr;
    SteamAPICall_t (*RequestLobbyList)(ISteamMatchmaking*) = nullptr;
    uint64         (*GetLobbyByIndex)(ISteamMatchmaking*, int) = nullptr;
    steam_bool     (*RequestLobbyData)(ISteamMatchmaking*, uint64) = nullptr;
    steam_bool     (*InviteUserToLobby)(ISteamMatchmaking*, uint64, uint64) = nullptr;
    void           (*AddResultCountFilter)(ISteamMatchmaking*, int) = nullptr;
    void           (*AddDistanceFilter)(ISteamMatchmaking*, int) = nullptr;
    void           (*AddStringFilter)(ISteamMatchmaking*, const char*, const char*, int) = nullptr;

    // friends / user
    const char*    (*GetPersonaName)(ISteamFriends*) = nullptr;
    int            (*GetFriendCount)(ISteamFriends*, int) = nullptr;
    uint64         (*GetFriendByIndex)(ISteamFriends*, int, int) = nullptr;
    const char*    (*GetFriendPersonaName)(ISteamFriends*, uint64) = nullptr;
    steam_bool     (*RequestUserInformation)(ISteamFriends*, uint64, steam_bool) = nullptr;
    steam_bool     (*GetFriendGamePlayed)(ISteamFriends*, uint64, FriendGameInfo_t*) = nullptr;
    void           (*ActivateInviteDialog)(ISteamFriends*, uint64) = nullptr;
    uint64         (*GetSteamID)(ISteamUser*) = nullptr;

    // networking messages
    EResult (*SendMessageToUser)(ISteamNetworkingMessages*, const SteamNetworkingIdentity&,
                                 const void*, uint32, int, int) = nullptr;
    int     (*ReceiveMessagesOnChannel)(ISteamNetworkingMessages*, int, SteamNetworkingMessage_t**, int) = nullptr;
    steam_bool (*AcceptSessionWithUser)(ISteamNetworkingMessages*, const SteamNetworkingIdentity&) = nullptr;
    void    (*ReleaseMessage)(SteamNetworkingMessage_t*) = nullptr;

    // callback plumbing
    void (*RegisterCallback)(CCallbackBase*, int) = nullptr;
    void (*UnregisterCallback)(CCallbackBase*) = nullptr;
    void (*RegisterCallResult)(CCallbackBase*, SteamAPICall_t) = nullptr;
};

SteamApi g_api;

template <typename T>
bool Bind(T& fn, const char* name) {
    fn = (T)GetProcAddress(g_api.Module, name);
    if (!fn) RFLOG(Net, "SteamBackend: missing export %s", name);
    return fn != nullptr;
}

// ---------------------------------------------------------------------------
// Metadata keys
//
// The lobby carries the *real* AppID of the game being emulated, which is the
// convention other Steam emulators use to recognise which game a Spacewar lobby
// belongs to. Several spellings are written so a browser looking for any of
// them finds us; on our side we accept any of them and never filter server-side,
// so we can never exclude someone else's lobby by accident.
// ---------------------------------------------------------------------------
const char* const kAppIdKeys[] = { "appid", "AppID", "game_appid", "gameappid" };
const char* const kNameKeys[]  = { "name", "lobby_name", "servername" };

const char* const kKeyRecordCount = "refix_eos_parts";
const char* const kKeyOwnerPuid   = "refix_eos_owner";
const char* const kKeyLobbyId     = "refix_eos_id";
const char* const kKeyMemberPuid  = "refix_eos_member_puid";
const char* const kKeyMemberName  = "refix_eos_member_name";

const int kRecordChunkChars = 3800;         // well inside Steam's 8 KB value cap
const int kSteamChannel     = 4711;

// ---------------------------------------------------------------------------
std::string Base64Encode(const std::vector<uint8_t>& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < in.size()) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < in.size()) v |= in[i + 2];
        out += t[(v >> 18) & 63];
        out += t[(v >> 12) & 63];
        out += (i + 1 < in.size()) ? t[(v >> 6) & 63] : '=';
        out += (i + 2 < in.size()) ? t[v & 63] : '=';
    }
    return out;
}

std::vector<uint8_t> Base64Decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        int v = val(c);
        if (v < 0) continue;                       // '=' and any stray character
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((buf >> bits) & 0xFF)); }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Callback adapters. CCallbackBase has three virtuals and no virtual
// destructor, so a plain subclass is binary-compatible with what Steam expects.
// ---------------------------------------------------------------------------
using CallbackFn = std::function<void(void*)>;

// The class is a template over a tag so that every callback kind gets its own
// vtable. That is not cosmetic: Steam emulators and proxies in the wild (ReFix's
// own steam_api64 shim among them) "hook" a callback by patching the first two
// slots of the vtable of the object handed to SteamAPI_RegisterCallback. With a
// single shared class every one of our receivers shares one vtable, so a patch
// aimed at one callback silently hijacks all of them. A distinct type per tag
// keeps the blast radius to the one callback that was actually hooked.
template <int Tag>
class Dispatcher : public CCallbackBase {
public:
    Dispatcher(int callbackId, int sizeBytes, CallbackFn fn)
        : m_size(sizeBytes), m_fn(std::move(fn)) { m_iCallback = callbackId; }
    void Run(void* param) override { if (m_fn) m_fn(param); }
    void Run(void* param, bool ioFailure, SteamAPICall_t) override { if (!ioFailure) Run(param); }
    int  GetCallbackSizeBytes() override { return m_size; }
private:
    int        m_size;
    CallbackFn m_fn;
};

// Registered dispatchers are owned for the process lifetime: Steam keeps raw
// pointers to them.
std::vector<CCallbackBase*> g_dispatchers;

template <typename T>
void OnCallback(CallbackFn fn) {
    auto* d = new Dispatcher<(int)T::k_iCallback>(T::k_iCallback, (int)sizeof(T), std::move(fn));
    g_dispatchers.push_back(d);
    g_api.RegisterCallback(d, T::k_iCallback);
}

// Negative tags keep a call result's vtable distinct from the broadcast
// callback of the same id.
template <typename T>
void OnCallResult(SteamAPICall_t call, CallbackFn fn) {
    auto* d = new Dispatcher<-(int)T::k_iCallback>(T::k_iCallback, (int)sizeof(T), std::move(fn));
    g_dispatchers.push_back(d);
    g_api.RegisterCallResult(d, call);
}

// ---------------------------------------------------------------------------
struct HostedLobby {
    std::string LobbyId;        // our EOS lobby id
    uint64_t    SteamLobby = 0;
    bool        Creating = false;
    LobbyRecord Pending;        // published as soon as the Steam lobby exists
};

// Work raised by a Steam callback but performed later, on the tick thread.
//
// Steam dispatches callbacks from inside the title's own SteamAPI_RunCallbacks,
// so anything we do there runs re-entrantly inside Steam and, worse, would take
// the lobby directory's lock in the opposite order from the maintenance thread.
// Callbacks therefore only record what happened; Pump() does the work.
struct Deferred {
    enum Kind { WriteRecord, ScanList, ReadLobby, MemberChanged, AnnounceSelf } K = ScanList;
    std::string LobbyId;                 // our EOS lobby id
    uint64_t    SteamLobby = 0;
    uint32_t    Count = 0;
    uint64_t    SteamId = 0;
    std::string Puid;
    std::string Name;
    bool        Joined = false;
};

struct State {
    std::mutex                              Mutex;
    std::vector<Deferred>                   Queue;
    std::map<std::string, HostedLobby>      Hosted;      // by EOS lobby id
    std::map<std::string, LobbyRecord>      Discovered;  // by EOS lobby id
    std::map<uint64_t, std::string>         SteamToLobby;
    uint64_t                                JoinedSteamLobby = 0;
    std::string                             LocalPuid;
    std::string                             LocalName;
};

State& S() { static State s; return s; }

SteamNetworkingIdentity IdentityOf(uint64_t steamId) {
    SteamNetworkingIdentity id;
    std::memset(&id, 0, sizeof(id));
    id.m_eType = k_ESteamNetworkingIdentityType_SteamID;
    id.m_cbSize = sizeof(uint64);
    std::memcpy(id.m_szUnknownRawString, &steamId, sizeof(steamId));
    return id;
}

std::string LobbyString(uint64_t lobby, const char* key) {
    if (!g_api.GetLobbyData) return {};
    const char* v = g_api.GetLobbyData(g_api.Matchmaking, lobby, key);
    return v ? std::string(v) : std::string();
}

} // namespace

// ---------------------------------------------------------------------------
SteamBackend& SteamBackend::Get() { static SteamBackend b; return b; }

uint64_t SteamBackend::LocalSteamId() const {
    if (!m_available || !g_api.GetSteamID) return 0;
    return g_api.GetSteamID(g_api.User);
}

std::string SteamBackend::LocalPersonaName() const {
    if (!m_available || !g_api.GetPersonaName) return {};
    const char* n = g_api.GetPersonaName(g_api.FriendsIface);
    return n ? std::string(n) : std::string();
}

bool SteamBackend::Start() {
    if (m_available) return true;

    // The online layer comes up before the title has called SteamAPI_Init, so a
    // single failed attempt must not disable Steam for the rest of the session.
    // Retry, but not on every call.
    static uint64_t lastAttemptMs = 0;
    const uint64_t now = NowMs();
    if (lastAttemptMs != 0 && now - lastAttemptMs < 2000) return false;
    lastAttemptMs = now;

    auto& cfg = Config::Get();
    // The AppID of the game being emulated, not the Spacewar id the lobby lives
    // on. Published so other emulators recognise the lobby.
    m_gameAppId = cfg.GetString("Steam", "RealAppId", "");
    if (m_gameAppId.empty() || m_gameAppId == "480")
        m_gameAppId = cfg.GetString("Steam", "WorkshopAppId", "");

    g_api.Module = GetModuleHandleA("steam_api64.dll");
    if (!g_api.Module) {
        RFLOG(Net, "SteamBackend: steam_api64.dll is not loaded; WAN play is unavailable");
        return false;
    }

    auto accessor = [&](const char* name) -> void* {
        auto fn = (void* (*)())GetProcAddress(g_api.Module, name);
        return fn ? fn() : nullptr;
    };
    g_api.Matchmaking  = (ISteamMatchmaking*)accessor("SteamAPI_SteamMatchmaking_v009");
    g_api.FriendsIface = (ISteamFriends*)accessor("SteamAPI_SteamFriends_v017");
    g_api.User         = (ISteamUser*)accessor("SteamAPI_SteamUser_v021");
    g_api.Messages     = (ISteamNetworkingMessages*)accessor("SteamAPI_SteamNetworkingMessages_SteamAPI_v002");

    if (!g_api.Matchmaking || !g_api.User) {
        RFLOG(Net, "SteamBackend: Steam interfaces unavailable (matchmaking=%p user=%p)",
              (void*)g_api.Matchmaking, (void*)g_api.User);
        return false;
    }

    bool ok = true;
    ok &= Bind(g_api.CreateLobby,           "SteamAPI_ISteamMatchmaking_CreateLobby");
    ok &= Bind(g_api.JoinLobby,             "SteamAPI_ISteamMatchmaking_JoinLobby");
    ok &= Bind(g_api.LeaveLobby,            "SteamAPI_ISteamMatchmaking_LeaveLobby");
    ok &= Bind(g_api.SetLobbyData,          "SteamAPI_ISteamMatchmaking_SetLobbyData");
    ok &= Bind(g_api.GetLobbyData,          "SteamAPI_ISteamMatchmaking_GetLobbyData");
    ok &= Bind(g_api.GetLobbyDataCount,     "SteamAPI_ISteamMatchmaking_GetLobbyDataCount");
    ok &= Bind(g_api.GetLobbyDataByIndex,   "SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex");
    ok &= Bind(g_api.SetLobbyMemberData,    "SteamAPI_ISteamMatchmaking_SetLobbyMemberData");
    ok &= Bind(g_api.GetLobbyMemberData,    "SteamAPI_ISteamMatchmaking_GetLobbyMemberData");
    ok &= Bind(g_api.GetNumLobbyMembers,    "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    ok &= Bind(g_api.GetLobbyMemberByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    ok &= Bind(g_api.GetLobbyOwner,         "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    ok &= Bind(g_api.SetLobbyMemberLimit,   "SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit");
    ok &= Bind(g_api.RequestLobbyList,      "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    ok &= Bind(g_api.GetLobbyByIndex,       "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    ok &= Bind(g_api.RequestLobbyData,      "SteamAPI_ISteamMatchmaking_RequestLobbyData");
    ok &= Bind(g_api.InviteUserToLobby,     "SteamAPI_ISteamMatchmaking_InviteUserToLobby");
    ok &= Bind(g_api.GetSteamID,            "SteamAPI_ISteamUser_GetSteamID");
    ok &= Bind(g_api.RegisterCallback,      "SteamAPI_RegisterCallback");
    ok &= Bind(g_api.RegisterCallResult,    "SteamAPI_RegisterCallResult");
    // Optional: absence degrades a feature rather than the whole backend.
    Bind(g_api.SetLobbyJoinable,      "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    Bind(g_api.SetLobbyType,          "SteamAPI_ISteamMatchmaking_SetLobbyType");
    Bind(g_api.AddResultCountFilter,  "SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter");
    Bind(g_api.AddDistanceFilter,     "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    Bind(g_api.AddStringFilter,       "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    Bind(g_api.UnregisterCallback,    "SteamAPI_UnregisterCallback");
    Bind(g_api.GetPersonaName,        "SteamAPI_ISteamFriends_GetPersonaName");
    Bind(g_api.GetFriendCount,        "SteamAPI_ISteamFriends_GetFriendCount");
    Bind(g_api.GetFriendByIndex,      "SteamAPI_ISteamFriends_GetFriendByIndex");
    Bind(g_api.GetFriendPersonaName,  "SteamAPI_ISteamFriends_GetFriendPersonaName");
    Bind(g_api.RequestUserInformation,"SteamAPI_ISteamFriends_RequestUserInformation");
    Bind(g_api.GetFriendGamePlayed,   "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    Bind(g_api.ActivateInviteDialog,  "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog");
    Bind(g_api.SendMessageToUser,     "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
    Bind(g_api.ReceiveMessagesOnChannel, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
    Bind(g_api.AcceptSessionWithUser, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
    Bind(g_api.ReleaseMessage,        "SteamAPI_SteamNetworkingMessage_t_Release");

    if (!ok) {
        RFLOG(Net, "SteamBackend: required Steamworks exports missing; WAN play is unavailable");
        return false;
    }

    // --- events we care about -------------------------------------------
    OnCallback<LobbyDataUpdate_t>([this](void* p) {
        auto* u = (LobbyDataUpdate_t*)p;
        if (u->m_ulSteamIDLobby != u->m_ulSteamIDMember) return;   // member data, not lobby data
        std::lock_guard<std::mutex> lock(S().Mutex);
        Deferred d; d.K = Deferred::ReadLobby; d.SteamLobby = u->m_ulSteamIDLobby;
        S().Queue.push_back(std::move(d));
    });

    OnCallback<LobbyChatUpdate_t>([this](void* p) {
        auto* u = (LobbyChatUpdate_t*)p;
        std::lock_guard<std::mutex> lock(S().Mutex);
        std::string lobbyId;
        for (auto& kv : S().Hosted)
            if (kv.second.SteamLobby == u->m_ulSteamIDLobby) { lobbyId = kv.first; break; }
        if (lobbyId.empty()) return;                               // not a lobby we own
        Deferred d;
        d.K          = Deferred::MemberChanged;
        d.LobbyId    = lobbyId;
        d.SteamLobby = u->m_ulSteamIDLobby;
        d.SteamId    = u->m_ulSteamIDUserChanged;
        d.Joined     = (u->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered) != 0;
        S().Queue.push_back(std::move(d));
    });

    OnCallback<LobbyEnter_t>([this](void* p) {
        auto* e = (LobbyEnter_t*)p;
        std::lock_guard<std::mutex> lock(S().Mutex);
        S().JoinedSteamLobby = e->m_ulSteamIDLobby;
        Deferred a; a.K = Deferred::AnnounceSelf; a.SteamLobby = e->m_ulSteamIDLobby;
        S().Queue.push_back(std::move(a));
        Deferred r; r.K = Deferred::ReadLobby;    r.SteamLobby = e->m_ulSteamIDLobby;
        S().Queue.push_back(std::move(r));
    });

    OnCallback<LobbyInvite_t>([this](void* p) {
        auto* inv = (LobbyInvite_t*)p;
        std::string lobbyId = LobbyString(inv->m_ulSteamIDLobby, kKeyLobbyId);
        RFLOG(Net, "SteamBackend: invite from %llu to lobby %llu",
              (unsigned long long)inv->m_ulSteamIDUser, (unsigned long long)inv->m_ulSteamIDLobby);
        if (OnInviteReceived) OnInviteReceived(lobbyId, inv->m_ulSteamIDUser);
    });

    OnCallback<GameLobbyJoinRequested_t>([this](void* p) {
        auto* req = (GameLobbyJoinRequested_t*)p;
        const uint64_t lobby = req->m_steamIDLobby.ConvertToUint64();
        RFLOG(Net, "SteamBackend: the player accepted an invite to Steam lobby %llu",
              (unsigned long long)lobby);
        JoinSteamLobby(lobby);
        if (OnJoinRequested) OnJoinRequested(LobbyString(lobby, kKeyLobbyId),
                                             req->m_steamIDFriend.ConvertToUint64());
    });

    // Lobby creation and lobby-list results are documented as call results, but
    // a proxy sitting in front of steam_api64 can swallow those. Both payloads
    // are also broadcast as ordinary callbacks, which survives any proxy, so
    // that is the path we take; the handlers ignore anything that is not ours.
    OnCallback<LobbyCreated_t>([this](void* p) {
        auto* c = (LobbyCreated_t*)p;
        AdoptCreatedLobby((int)c->m_eResult, c->m_ulSteamIDLobby);
    });
    OnCallback<LobbyMatchList_t>([this](void* p) {
        HandleLobbyList(((LobbyMatchList_t*)p)->m_nLobbiesMatching);
    });

    if (g_api.Messages) {
        OnCallback<SteamNetworkingMessagesSessionRequest_t>([](void* p) {
            auto* req = (SteamNetworkingMessagesSessionRequest_t*)p;
            // Anyone who can reach us through Steam is already authenticated by
            // Steam; the EOS layer still gates the game's own connection.
            if (g_api.AcceptSessionWithUser)
                g_api.AcceptSessionWithUser(g_api.Messages, req->m_identityRemote);
        });
    }

    m_available = true;
    RFLOG(Net, "SteamBackend up: steamId=%llu persona='%s' gameAppId=%s relay=%s",
          (unsigned long long)LocalSteamId(), LocalPersonaName().c_str(),
          m_gameAppId.empty() ? "<unset>" : m_gameAppId.c_str(),
          g_api.Messages ? "SteamNetworkingMessages" : "unavailable");
    return true;
}

void SteamBackend::SetLocalIdentity(const std::string& puid, const std::string& name) {
    std::lock_guard<std::mutex> lock(S().Mutex);
    S().LocalPuid = puid;
    S().LocalName = name;
}

// ---------------------------------------------------------------------------
// Hosting
// ---------------------------------------------------------------------------
void SteamBackend::PublishLobby(const LobbyRecord& record) {
    if (!m_available) return;

    HostedLobby* hosted = nullptr;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        hosted = &S().Hosted[record.LobbyId];
        hosted->LobbyId = record.LobbyId;
        hosted->Pending = record;
        if (hosted->SteamLobby == 0 && !hosted->Creating) {
            hosted->Creating = true;
        } else if (hosted->SteamLobby == 0) {
            return;                                   // creation already in flight
        }
    }

    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        steamLobby = S().Hosted[record.LobbyId].SteamLobby;
    }

    if (steamLobby == 0) {
        const int maxMembers = record.MaxMembers > 0 ? (int)record.MaxMembers : 8;
        SteamAPICall_t call = g_api.CreateLobby(g_api.Matchmaking, k_ELobbyTypePublic, maxMembers);
        (void)call;   // the broadcast LobbyCreated_t callback carries the result
        return;
    }

    WriteLobbyRecord(steamLobby, record);
}

void SteamBackend::WriteLobbyRecord(uint64_t steamLobby, const LobbyRecord& record) {
    if (!m_available || !steamLobby) return;

    Writer w;
    record.Write(w);
    const std::string encoded = Base64Encode(w.Data());

    int parts = 0;
    for (size_t off = 0; off < encoded.size(); off += kRecordChunkChars, parts++) {
        char key[64];
        snprintf(key, sizeof(key), "refix_eos_%d", parts);
        g_api.SetLobbyData(g_api.Matchmaking, steamLobby, key,
                           encoded.substr(off, kRecordChunkChars).c_str());
    }
    g_api.SetLobbyData(g_api.Matchmaking, steamLobby, kKeyRecordCount, std::to_string(parts).c_str());
    g_api.SetLobbyData(g_api.Matchmaking, steamLobby, kKeyLobbyId, record.LobbyId.c_str());
    g_api.SetLobbyData(g_api.Matchmaking, steamLobby, kKeyOwnerPuid, record.OwnerPuid.c_str());

    // Identify the game so other emulators' browsers can recognise this lobby.
    if (!m_gameAppId.empty())
        for (const char* key : kAppIdKeys)
            g_api.SetLobbyData(g_api.Matchmaking, steamLobby, key, m_gameAppId.c_str());

    // A human-readable name, taken from whatever the title called the lobby.
    std::string name;
    for (const auto& a : record.Attributes) {
        std::string lower = ToLower(a.Key);
        if (lower.find("name") != std::string::npos && !a.Value.AsUtf8.empty()) { name = a.Value.AsUtf8; break; }
    }
    if (!name.empty())
        for (const char* key : kNameKeys)
            g_api.SetLobbyData(g_api.Matchmaking, steamLobby, key, name.c_str());

    if (g_api.SetLobbyMemberLimit && record.MaxMembers > 0)
        g_api.SetLobbyMemberLimit(g_api.Matchmaking, steamLobby, (int)record.MaxMembers);
    if (g_api.SetLobbyJoinable)
        g_api.SetLobbyJoinable(g_api.Matchmaking, steamLobby, record.AvailableSlots() > 0);
}

bool SteamBackend::ReadLobbyRecord(uint64_t steamLobby, LobbyRecord& out) {
    if (!m_available) return false;
    const std::string countText = LobbyString(steamLobby, kKeyRecordCount);
    if (countText.empty()) return false;

    int parts = std::atoi(countText.c_str());
    if (parts <= 0 || parts > 64) return false;

    std::string encoded;
    for (int i = 0; i < parts; i++) {
        char key[64];
        snprintf(key, sizeof(key), "refix_eos_%d", i);
        std::string chunk = LobbyString(steamLobby, key);
        if (chunk.empty()) return false;            // not fully replicated yet
        encoded += chunk;
    }

    std::vector<uint8_t> bytes = Base64Decode(encoded);
    Reader r(bytes.data(), bytes.size());
    if (!LobbyRecord::Read(r, out)) return false;

    out.LastSeenMs = NowMs();
    out.SteamLobbyId = steamLobby;
    if (g_api.GetLobbyOwner) out.OwnerSteamId = g_api.GetLobbyOwner(g_api.Matchmaking, steamLobby);
    return true;
}

void SteamBackend::WithdrawLobby(const std::string& lobbyId) {
    if (!m_available) return;
    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        auto it = S().Hosted.find(lobbyId);
        if (it == S().Hosted.end()) return;
        steamLobby = it->second.SteamLobby;
        S().Hosted.erase(it);
        S().SteamToLobby.erase(steamLobby);
    }
    if (steamLobby && g_api.LeaveLobby) g_api.LeaveLobby(g_api.Matchmaking, steamLobby);
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------
void SteamBackend::RequestList() {
    if (!m_available) return;

    // The lobbies live on Spacewar, which every Steam emulator shares, so an
    // unfiltered list is thousands of unrelated rooms and the fifty we are
    // allowed back would almost never contain the game we are looking for.
    // The convention emulators already follow is to publish the *real* AppID of
    // the emulated game as lobby data, so that is what we filter on - never on
    // anything ReFix-specific, which would hide every lobby but our own. The
    // key has several spellings in the wild, so each call asks for the next one
    // and the results accumulate; one unfiltered sweep is included so a lobby
    // that publishes no AppID at all can still be found.
    const int keyCount = (int)(sizeof(kAppIdKeys) / sizeof(kAppIdKeys[0]));
    const int step = m_listSweep++ % (keyCount + 1);

    if (g_api.AddDistanceFilter) {
        // Steam defaults to "near me", which quietly hides most of the world.
        const std::string d = Config::Get().GetString("Matchmaking", "LobbyDistanceFilter", "worldwide");
        int distance = k_ELobbyDistanceFilterWorldwide;
        if      (d == "close")   distance = k_ELobbyDistanceFilterClose;
        else if (d == "default") distance = k_ELobbyDistanceFilterDefault;
        else if (d == "far")     distance = k_ELobbyDistanceFilterFar;
        g_api.AddDistanceFilter(g_api.Matchmaking, distance);
    }
    if (g_api.AddResultCountFilter)
        g_api.AddResultCountFilter(g_api.Matchmaking,
                                   Config::Get().GetInt("Matchmaking", "MaxLobbyResults", 50));
    if (step < keyCount && g_api.AddStringFilter && !m_gameAppId.empty())
        g_api.AddStringFilter(g_api.Matchmaking, kAppIdKeys[step], m_gameAppId.c_str(),
                              k_ELobbyComparisonEqual);

    SteamAPICall_t call = g_api.RequestLobbyList(g_api.Matchmaking);
    (void)call;   // the broadcast LobbyMatchList_t callback carries the result
}

// Walks whatever the last lobby-list request returned and keeps every entry we
// can read. Reached from the call result and, when a proxy swallows call
// results, from the broadcast LobbyMatchList_t callback; it is idempotent so it
// does not matter which arrives, or that both do.
void SteamBackend::HandleLobbyList(uint32_t n) {
    if (!m_available || !n) return;
    std::lock_guard<std::mutex> lock(S().Mutex);
    Deferred d; d.K = Deferred::ScanList; d.Count = n;
    S().Queue.push_back(std::move(d));
}

// Binds a freshly created Steam lobby to the EOS lobby whose creation is in
// flight. Idempotent, and it ignores lobbies the title created for itself.
void SteamBackend::AdoptCreatedLobby(int result, uint64_t steamLobby) {
    if (!m_available || !steamLobby) return;
    std::string lobbyId;
    LobbyRecord pending;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        if (S().SteamToLobby.count(steamLobby)) return;          // already adopted
        for (auto& kv : S().Hosted)
            if (kv.second.Creating && kv.second.SteamLobby == 0) { lobbyId = kv.first; break; }
        if (lobbyId.empty()) return;                             // not one of ours
        auto& h = S().Hosted[lobbyId];
        h.Creating = false;
        if (result != (int)k_EResultOK) {
            RFLOG(Net, "SteamBackend: CreateLobby failed (result=%d)", result);
            return;
        }
        h.SteamLobby = steamLobby;
        pending = h.Pending;
        S().SteamToLobby[steamLobby] = lobbyId;
        Deferred d; d.K = Deferred::WriteRecord; d.LobbyId = lobbyId; d.SteamLobby = steamLobby;
        S().Queue.push_back(std::move(d));
    }
    RFLOG(Net, "SteamBackend: Steam lobby %llu now carries EOS lobby %s",
          (unsigned long long)steamLobby, lobbyId.c_str());
}

std::vector<LobbyRecord> SteamBackend::Snapshot() const {
    std::vector<LobbyRecord> out;
    std::lock_guard<std::mutex> lock(S().Mutex);
    for (const auto& kv : S().Discovered) out.push_back(kv.second);
    return out;
}

bool SteamBackend::JoinByLobbyId(const std::string& lobbyId) {
    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        for (const auto& kv : S().SteamToLobby) if (kv.second == lobbyId) { steamLobby = kv.first; break; }
        if (!steamLobby) {
            auto it = S().Discovered.find(lobbyId);
            if (it != S().Discovered.end()) steamLobby = it->second.SteamLobbyId;
        }
    }
    return steamLobby ? JoinSteamLobby(steamLobby) : false;
}

bool SteamBackend::JoinSteamLobby(uint64_t steamLobbyId) {
    if (!m_available || !steamLobbyId) return false;
    RFLOG(Net, "SteamBackend: joining Steam lobby %llu", (unsigned long long)steamLobbyId);
    g_api.JoinLobby(g_api.Matchmaking, steamLobbyId);
    return true;
}

void SteamBackend::LeaveLobby(const std::string& lobbyId) {
    if (!m_available) return;
    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        for (const auto& kv : S().SteamToLobby) if (kv.second == lobbyId) { steamLobby = kv.first; break; }
        if (steamLobby == S().JoinedSteamLobby) S().JoinedSteamLobby = 0;
        S().Discovered.erase(lobbyId);
    }
    if (steamLobby && g_api.LeaveLobby) g_api.LeaveLobby(g_api.Matchmaking, steamLobby);
}

// ---------------------------------------------------------------------------
// Messaging
// ---------------------------------------------------------------------------
bool SteamBackend::SendTo(uint64_t steamId, const void* data, size_t len) {
    if (!m_available || !g_api.SendMessageToUser || !steamId) return false;
    SteamNetworkingIdentity id = IdentityOf(steamId);
    EResult r = g_api.SendMessageToUser(g_api.Messages, id, data, (uint32)len,
                                        k_nSteamNetworkingSend_Reliable, kSteamChannel);
    return r == k_EResultOK;
}

void SteamBackend::Pump() {
    if (!m_available) return;
    DrainDeferred();

    // Steam's lobby list is asked for one AppID-key spelling at a time, so keep
    // asking in the background: by the time the title runs a search the results
    // are already here instead of one round trip away. This runs on the tick
    // thread, which is where the title itself calls Steam.
    const uint64_t now = NowMs();
    if (now - m_lastListMs >= 3000) {
        m_lastListMs = now;
        RequestList();
    }
    if (!g_api.ReceiveMessagesOnChannel) return;
    SteamNetworkingMessage_t* msgs[32];
    int n = g_api.ReceiveMessagesOnChannel(g_api.Messages, kSteamChannel, msgs, 32);
    for (int i = 0; i < n; i++) {
        SteamNetworkingMessage_t* m = msgs[i];
        if (!m) continue;
        uint64_t from = 0;
        std::memcpy(&from, m->m_identityPeer.m_szUnknownRawString, sizeof(from));
        if (OnMessage && m->m_pData && m->m_cbSize > 0)
            OnMessage(from, (const uint8_t*)m->m_pData, (size_t)m->m_cbSize);
        if (g_api.ReleaseMessage) g_api.ReleaseMessage(m);
    }
}

// ---------------------------------------------------------------------------
// Friends and invites
// ---------------------------------------------------------------------------
std::vector<SteamFriendInfo> SteamBackend::Friends() const {
    std::vector<SteamFriendInfo> out;
    if (!m_available || !g_api.GetFriendCount || !g_api.GetFriendByIndex) return out;

    const int count = g_api.GetFriendCount(g_api.FriendsIface, k_EFriendFlagImmediate);
    for (int i = 0; i < count; i++) {
        SteamFriendInfo f;
        f.SteamId = g_api.GetFriendByIndex(g_api.FriendsIface, i, k_EFriendFlagImmediate);
        if (!f.SteamId) continue;
        if (g_api.GetFriendPersonaName) {
            const char* n = g_api.GetFriendPersonaName(g_api.FriendsIface, f.SteamId);
            if (n) f.PersonaName = n;
        }
        // Steam only keeps persona names for people it has recently had reason
        // to cache. For everyone else the name comes back empty (or as the
        // literal placeholder Steam uses), which is what turns a friends list
        // into a column of blank rows. Ask Steam for the name so the next
        // refresh has it, and meanwhile give the title something it can render.
        if (f.PersonaName.empty() || f.PersonaName == "[unknown]") {
            if (g_api.RequestUserInformation)
                g_api.RequestUserInformation(g_api.FriendsIface, f.SteamId, 1);
            f.PersonaName.clear();
            f.NameResolved = false;
        } else {
            f.NameResolved = true;
        }
        if (g_api.GetFriendGamePlayed) {
            FriendGameInfo_t info{};
            if (g_api.GetFriendGamePlayed(g_api.FriendsIface, f.SteamId, &info)) {
                f.InSameGame = true;
                f.LobbyId = info.m_steamIDLobby.ConvertToUint64();
            }
        }
        out.push_back(std::move(f));
    }
    return out;
}

bool SteamBackend::InviteToLobby(const std::string& lobbyId, uint64_t steamId) {
    if (!m_available || !g_api.InviteUserToLobby || !steamId) return false;
    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        auto it = S().Hosted.find(lobbyId);
        if (it != S().Hosted.end()) steamLobby = it->second.SteamLobby;
        if (!steamLobby) for (const auto& kv : S().SteamToLobby) if (kv.second == lobbyId) { steamLobby = kv.first; break; }
    }
    if (!steamLobby) return false;
    const bool sent = g_api.InviteUserToLobby(g_api.Matchmaking, steamLobby, steamId) != 0;
    RFLOG(Net, "SteamBackend: invite to %llu for Steam lobby %llu -> %s",
          (unsigned long long)steamId, (unsigned long long)steamLobby, sent ? "sent" : "refused");
    return sent;
}

bool SteamBackend::OpenInviteOverlay(const std::string& lobbyId) {
    if (!m_available || !g_api.ActivateInviteDialog) return false;
    uint64_t steamLobby = 0;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        auto it = S().Hosted.find(lobbyId);
        if (it != S().Hosted.end()) steamLobby = it->second.SteamLobby;
        if (!steamLobby) for (const auto& kv : S().SteamToLobby) if (kv.second == lobbyId) { steamLobby = kv.first; break; }
    }
    if (!steamLobby) return false;
    g_api.ActivateInviteDialog(g_api.FriendsIface, steamLobby);
    return true;
}

// ---------------------------------------------------------------------------
// Everything a Steam callback asked for, performed on the tick thread where
// calling back into Steam and into the lobby directory is safe.
// ---------------------------------------------------------------------------
void SteamBackend::DrainDeferred() {
    std::vector<Deferred> work;
    {
        std::lock_guard<std::mutex> lock(S().Mutex);
        if (S().Queue.empty()) return;
        work.swap(S().Queue);
    }

    for (const Deferred& d : work) {
        switch (d.K) {
        case Deferred::WriteRecord: {
            LobbyRecord pending;
            {
                std::lock_guard<std::mutex> lock(S().Mutex);
                auto it = S().Hosted.find(d.LobbyId);
                if (it == S().Hosted.end()) break;
                pending = it->second.Pending;
            }
            WriteLobbyRecord(d.SteamLobby, pending);
            break;
        }
        case Deferred::AnnounceSelf: {
            std::string puid, name;
            {
                std::lock_guard<std::mutex> lock(S().Mutex);
                puid = S().LocalPuid;
                name = S().LocalName;
            }
            if (g_api.SetLobbyMemberData) {
                g_api.SetLobbyMemberData(g_api.Matchmaking, d.SteamLobby, kKeyMemberPuid, puid.c_str());
                g_api.SetLobbyMemberData(g_api.Matchmaking, d.SteamLobby, kKeyMemberName, name.c_str());
            }
            RFLOG(Net, "SteamBackend: entered Steam lobby %llu", (unsigned long long)d.SteamLobby);
            break;
        }
        case Deferred::ReadLobby: {
            LobbyRecord record;
            if (!ReadLobbyRecord(d.SteamLobby, record)) break;
            {
                std::lock_guard<std::mutex> lock(S().Mutex);
                if (S().Hosted.count(record.LobbyId)) break;       // ours; ignore the echo
                S().Discovered[record.LobbyId] = record;
                S().SteamToLobby[d.SteamLobby] = record.LobbyId;
            }
            if (OnLobbyDiscovered) OnLobbyDiscovered(record);
            break;
        }
        case Deferred::MemberChanged: {
            std::string puid, name;
            if (d.Joined && g_api.GetLobbyMemberData) {
                const char* p1 = g_api.GetLobbyMemberData(g_api.Matchmaking, d.SteamLobby, d.SteamId, kKeyMemberPuid);
                const char* p2 = g_api.GetLobbyMemberData(g_api.Matchmaking, d.SteamLobby, d.SteamId, kKeyMemberName);
                if (p1) puid = p1;
                if (p2) name = p2;
            }
            RFLOG(Net, "SteamBackend: member %llu %s lobby %s (puid='%s')",
                  (unsigned long long)d.SteamId, d.Joined ? "entered" : "left",
                  d.LobbyId.c_str(), puid.c_str());
            if (OnMemberChanged) OnMemberChanged(d.LobbyId, d.SteamId, puid, name, d.Joined);
            break;
        }
        case Deferred::ScanList: {
            int ours = 0;
            if (!g_api.GetLobbyByIndex) break;
            for (uint32_t i = 0; i < d.Count; i++) {
                uint64_t lobby = g_api.GetLobbyByIndex(g_api.Matchmaking, (int)i);
                if (!lobby) continue;
                LobbyRecord record;
                if (ReadLobbyRecord(lobby, record)) {
                    ours++;
                    bool mine = false;
                    {
                        std::lock_guard<std::mutex> lock(S().Mutex);
                        mine = S().Hosted.count(record.LobbyId) != 0;
                        if (!mine) {
                            S().Discovered[record.LobbyId] = record;
                            S().SteamToLobby[lobby] = record.LobbyId;
                        }
                    }
                    if (!mine && OnLobbyDiscovered) OnLobbyDiscovered(record);
                } else if (g_api.RequestLobbyData) {
                    // Metadata has not replicated yet, or the lobby belongs to
                    // another emulator. Ask for its data either way; the update
                    // brings us back here with the content.
                    g_api.RequestLobbyData(g_api.Matchmaking, lobby);
                }
            }
            RFLOG(Net, "SteamBackend: Steam returned %u lobby(ies), %d readable", d.Count, ours);
            break;
        }
        }
    }
}

} // namespace refix

