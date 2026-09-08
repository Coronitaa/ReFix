// ReFix EOS v3 - EOS_Sessions_*, EOS_SessionModification_*, EOS_SessionDetails_*,
// EOS_SessionSearch_*, EOS_ActiveSession_*.
//
// Unreal titles reach matchmaking through either lobbies or sessions depending
// on how their online subsystem is configured, and some use both at once. Both
// faces here sit on the same replicated, host-authoritative record, so a session
// and a lobby can never disagree about who is in the game.
//
// Two failures of the old proxy are fixed structurally rather than patched:
// a session id is derived per creation (so re-hosting after leaving is not
// "already exists"), and the settings a title reads back are the ones it wrote.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/refix_config.h"
#include "../core/refix_hash.h"
#include "../core/eos_handles.h"
#include "../core/session_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../core/eos_online.h"
#include "../net/lobby_directory.h"
#include "../net/steam_backend.h"
#include "../eos_module.h"

using namespace refix;

namespace {

// --- local session registry ---------------------------------------------
// EOS addresses a session the title created by the *name* it chose, while the
// network knows it by its lobby id. This maps between the two.
struct LocalSession {
    std::string              Name;
    std::string              LobbyId;
    std::string              SessionId;
    EOS_EOnlineSessionState  State = EOS_EOnlineSessionState::EOS_OSS_NoSession;
};

std::mutex                             g_sessionsMutex;
std::map<std::string, LocalSession>    g_sessions;      // by name

bool LookupByName(const std::string& name, LocalSession& out) {
    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    auto it = g_sessions.find(name);
    if (it == g_sessions.end()) return false;
    out = it->second;
    return true;
}

std::string PuidString(EOS_ProductUserId id) {
    const char* s = IdRegistry::ToString(id);
    return s ? std::string(s) : std::string();
}

// --- reserved-attribute helpers -----------------------------------------
bool IsReserved(const std::string& key) {
    return key.rfind("__refix_", 0) == 0;
}

const Attribute* Reserved(const LobbyRecord& r, const char* key) {
    return r.FindAttribute(key);
}

std::string ReservedString(const LobbyRecord& r, const char* key, const std::string& def = "") {
    const Attribute* a = Reserved(r, key);
    return a ? a->Value.AsUtf8 : def;
}

bool ReservedBool(const LobbyRecord& r, const char* key, bool def) {
    const Attribute* a = Reserved(r, key);
    return a ? a->Value.AsBool : def;
}

void SetReserved(LobbyRecord& r, const char* key, const std::string& value) {
    for (auto& a : r.Attributes) {
        if (a.Key == key) { a.Value.Type = EOS_ELobbyAttributeType::EOS_AT_STRING; a.Value.AsUtf8 = value; return; }
    }
    Attribute a;
    a.Key = key;
    a.Value.Type = EOS_ELobbyAttributeType::EOS_AT_STRING;
    a.Value.AsUtf8 = value;
    a.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
    r.Attributes.push_back(std::move(a));
}

void SetReservedBool(LobbyRecord& r, const char* key, bool value) {
    for (auto& a : r.Attributes) {
        if (a.Key == key) { a.Value.Type = EOS_ELobbyAttributeType::EOS_AT_BOOLEAN; a.Value.AsBool = value; return; }
    }
    Attribute a;
    a.Key = key;
    a.Value.Type = EOS_ELobbyAttributeType::EOS_AT_BOOLEAN;
    a.Value.AsBool = value;
    a.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
    r.Attributes.push_back(std::move(a));
}

bool IsSessionRecord(const LobbyRecord& r) {
    const Attribute* kind = r.FindAttribute(REFIX_SESSION_KIND_KEY);
    return kind && kind->Value.AsUtf8 == REFIX_SESSION_KIND_VALUE;
}

// --- blocks handed to the title ------------------------------------------
struct SessionAttributeBlock {
    EOS_SessionDetails_Attribute Attribute;
    EOS_Sessions_AttributeData   Data;
    std::string                  Key;
    std::string                  Utf8;
};
std::mutex                                                    g_attrMutex;
std::map<EOS_SessionDetails_Attribute*, SessionAttributeBlock*> g_attrBlocks;

EOS_SessionDetails_Attribute* MakeSessionAttribute(const Attribute& src) {
    auto* block = new SessionAttributeBlock();
    block->Key  = src.Key;
    block->Utf8 = src.Value.AsUtf8;
    std::memset(&block->Data, 0, sizeof(block->Data));
    block->Data.ApiVersion = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;
    block->Data.Key        = block->Key.c_str();
    switch (src.Value.Type) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:
            block->Data.ValueType = EOS_ESessionAttributeType::EOS_AT_INT64;
            block->Data.Value.AsInt64 = src.Value.AsInt64; break;
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:
            block->Data.ValueType = EOS_ESessionAttributeType::EOS_AT_DOUBLE;
            block->Data.Value.AsDouble = src.Value.AsDouble; break;
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN:
            block->Data.ValueType = EOS_ESessionAttributeType::EOS_AT_BOOLEAN;
            block->Data.Value.AsBool = src.Value.AsBool ? EOS_TRUE : EOS_FALSE; break;
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:
            block->Data.ValueType = EOS_ESessionAttributeType::EOS_AT_STRING;
            block->Data.Value.AsUtf8 = block->Utf8.c_str(); break;
    }
    std::memset(&block->Attribute, 0, sizeof(block->Attribute));
    block->Attribute.ApiVersion        = EOS_SESSIONDETAILS_ATTRIBUTE_API_LATEST;
    block->Attribute.Data              = &block->Data;
    block->Attribute.AdvertisementType = EOS_ESessionAttributeAdvertisementType::EOS_SAAT_Advertise;

    std::lock_guard<std::mutex> lock(g_attrMutex);
    g_attrBlocks[&block->Attribute] = block;
    return &block->Attribute;
}

struct SessionInfoBlock {
    EOS_SessionDetails_Info     Info;
    EOS_SessionDetails_Settings Settings;
    std::string                 SessionId;
    std::string                 HostAddress;
    std::string                 BucketId;
};
std::mutex                                          g_infoMutex;
std::map<EOS_SessionDetails_Info*, SessionInfoBlock*> g_infoBlocks;

EOS_SessionDetails_Info* MakeSessionInfo(const LobbyRecord& r) {
    auto* block = new SessionInfoBlock();
    block->SessionId   = ReservedString(r, REFIX_SESSION_ID_KEY, r.LobbyId);
    block->HostAddress = ReservedString(r, REFIX_SESSION_HOST_KEY, r.HostAddress.ToString());
    block->BucketId    = r.BucketId;

    std::memset(&block->Settings, 0, sizeof(block->Settings));
    block->Settings.ApiVersion           = EOS_SESSIONDETAILS_SETTINGS_API_LATEST;
    block->Settings.BucketId             = block->BucketId.c_str();
    block->Settings.NumPublicConnections = r.MaxMembers;
    block->Settings.bAllowJoinInProgress = ReservedBool(r, REFIX_SESSION_JIP_KEY, true) ? EOS_TRUE : EOS_FALSE;
    block->Settings.PermissionLevel      =
        r.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY
            ? EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly
            : (r.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE
                   ? EOS_EOnlineSessionPermissionLevel::EOS_OSPF_JoinViaPresence
                   : EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised);
    block->Settings.bInvitesAllowed   = ReservedBool(r, REFIX_SESSION_INVITES_KEY, r.AllowInvites) ? EOS_TRUE : EOS_FALSE;
    block->Settings.bSanctionsEnabled = EOS_FALSE;
    block->Settings.AllowedPlatformIds = nullptr;
    block->Settings.AllowedPlatformIdsCount = 0;

    std::memset(&block->Info, 0, sizeof(block->Info));
    block->Info.ApiVersion               = EOS_SESSIONDETAILS_INFO_API_LATEST;
    block->Info.SessionId                = block->SessionId.c_str();
    block->Info.HostAddress              = block->HostAddress.c_str();
    block->Info.NumOpenPublicConnections = r.AvailableSlots();
    block->Info.Settings                 = &block->Settings;
    block->Info.OwnerUserId              = IdRegistry::Get().Puid(r.OwnerPuid);
    block->Info.OwnerServerClientId      = nullptr;

    std::lock_guard<std::mutex> lock(g_infoMutex);
    g_infoBlocks[&block->Info] = block;
    return &block->Info;
}

AttributeValue FromSdk(const EOS_Sessions_AttributeData& data) {
    AttributeValue v;
    switch (data.ValueType) {
        case EOS_ESessionAttributeType::EOS_AT_INT64:
            v.Type = EOS_ELobbyAttributeType::EOS_AT_INT64;   v.AsInt64  = data.Value.AsInt64; break;
        case EOS_ESessionAttributeType::EOS_AT_DOUBLE:
            v.Type = EOS_ELobbyAttributeType::EOS_AT_DOUBLE;  v.AsDouble = data.Value.AsDouble; break;
        case EOS_ESessionAttributeType::EOS_AT_BOOLEAN:
            v.Type = EOS_ELobbyAttributeType::EOS_AT_BOOLEAN; v.AsBool   = data.Value.AsBool != EOS_FALSE; break;
        case EOS_ESessionAttributeType::EOS_AT_STRING:
        default:
            v.Type = EOS_ELobbyAttributeType::EOS_AT_STRING;
            v.AsUtf8 = data.Value.AsUtf8 ? data.Value.AsUtf8 : ""; break;
    }
    return v;
}

std::vector<Attribute> VisibleAttributes(const LobbyRecord& r) {
    std::vector<Attribute> out;
    for (const auto& a : r.Attributes) if (!IsReserved(a.Key)) out.push_back(a);
    return out;
}

EOS_SessionModificationHandle* AsModification(EOS_HSessionModification h) {
    auto* m = (EOS_SessionModificationHandle*)h;
    return (m && m->Magic == kMagicSessionModification) ? m : nullptr;
}
EOS_SessionDetailsHandle* AsDetails(EOS_HSessionDetails h) {
    auto* d = (EOS_SessionDetailsHandle*)h;
    return (d && d->Magic == kMagicSessionDetails) ? d : nullptr;
}
EOS_SessionSearchHandle* AsSearch(EOS_HSessionSearch h) {
    auto* s = (EOS_SessionSearchHandle*)h;
    return (s && s->Magic == kMagicSessionSearch) ? s : nullptr;
}
EOS_ActiveSessionHandle* AsActive(EOS_HActiveSession h) {
    auto* a = (EOS_ActiveSessionHandle*)h;
    return (a && a->Magic == kMagicActiveSession) ? a : nullptr;
}

LobbyMember LocalMember() {
    const UserRecord& user = Identity::Get().LocalUser();
    LobbyMember m;
    m.Puid        = user.Puid;
    m.DisplayName = user.DisplayName;
    m.Address     = LobbyDirectory::Get().LocalEndpoint();
    m.SteamId     = SteamBackend::Get().LocalSteamId();
    return m;
}

} // namespace

// ===========================================================================
// EOS_Sessions_*
// ===========================================================================
EOS_DECLARE_FUNC(EOS_EResult) EOS_Sessions_CreateSessionModification(EOS_HSessions Handle, const EOS_Sessions_CreateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle) {
    if (!Options || !OutSessionModificationHandle || !Options->SessionName) return ER::EOS_InvalidParameters;
    *OutSessionModificationHandle = nullptr;

    LocalSession existing;
    if (LookupByName(Options->SessionName, existing))
        return ER::EOS_Sessions_SessionAlreadyExists;

    auto* mod = new EOS_SessionModificationHandle();
    mod->Magic         = kMagicSessionModification;
    mod->SessionName   = Options->SessionName;
    mod->LocalPuid     = PuidString(Options->LocalUserId);
    mod->Creating      = true;
    mod->SetBucket     = Options->BucketId != nullptr;
    mod->Bucket        = Options->BucketId ? Options->BucketId : "";
    mod->SetMaxPlayers = true;
    mod->MaxPlayers    = Options->MaxPlayers;
    *OutSessionModificationHandle = (EOS_HSessionModification)mod;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Sessions_UpdateSessionModification(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle) {
    if (!Options || !OutSessionModificationHandle || !Options->SessionName) return ER::EOS_InvalidParameters;
    *OutSessionModificationHandle = nullptr;

    LocalSession session;
    if (!LookupByName(Options->SessionName, session)) return ER::EOS_NotFound;

    auto* mod = new EOS_SessionModificationHandle();
    mod->Magic       = kMagicSessionModification;
    mod->SessionName = Options->SessionName;
    mod->Creating    = false;
    *OutSessionModificationHandle = (EOS_HSessionModification)mod;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Sessions_UpdateSession(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionOptions* Options, void* ClientData, const EOS_Sessions_OnUpdateSessionCallback CompletionDelegate) {
    std::string sessionName, sessionId;
    ER result = ER::EOS_InvalidParameters;

    auto* mod = Options ? AsModification(Options->SessionModificationHandle) : nullptr;
    if (mod && StartOnlineSubsystem()) {
        sessionName = mod->SessionName;
        LocalSession session;
        const bool exists = LookupByName(sessionName, session);

        if (mod->Creating && !exists) {
            if (mod->MaxPlayers == 0) {
                result = ER::EOS_InvalidParameters;
            } else {
                LobbyRecord record;
                record.OwnerPuid  = Identity::Get().LocalUser().Puid;
                record.BucketId   = mod->Bucket;
                record.MaxMembers = mod->MaxPlayers;
                record.Permission =
                    mod->Permission == EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly
                        ? EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY
                        : (mod->Permission == EOS_EOnlineSessionPermissionLevel::EOS_OSPF_JoinViaPresence
                               ? EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE
                               : EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED);
                record.AllowInvites = mod->InvitesAllowed;

                LobbyMember owner = LocalMember();
                owner.IsOwner = true;
                record.Members.push_back(owner);

                // A fresh id every time a session is created, so leaving and
                // re-hosting can never collide with the previous one.
                sessionId = "sess" + DerivedId(sessionName + record.OwnerPuid +
                                               std::to_string(NowMs()) + std::to_string(GetCurrentProcessId()), 10);
                SetReserved(record, REFIX_SESSION_KIND_KEY, REFIX_SESSION_KIND_VALUE);
                SetReserved(record, REFIX_SESSION_ID_KEY, sessionId);
                SetReserved(record, REFIX_SESSION_HOST_KEY,
                            mod->SetHostAddress ? mod->HostAddress
                                                : LobbyDirectory::Get().LocalEndpoint().ToString());
                SetReservedBool(record, REFIX_SESSION_JIP_KEY, mod->JoinInProgress);
                SetReservedBool(record, REFIX_SESSION_INVITES_KEY, mod->InvitesAllowed);
                SetReserved(record, REFIX_SESSION_STATE_KEY, "Pending");
                for (const auto& a : mod->AddAttributes) record.Attributes.push_back(a);

                std::string lobbyId = LobbyDirectory::Get().CreateLobby(std::move(record));

                LocalSession created;
                created.Name      = sessionName;
                created.LobbyId   = lobbyId;
                created.SessionId = sessionId;
                created.State     = EOS_EOnlineSessionState::EOS_OSS_Pending;
                {
                    std::lock_guard<std::mutex> lock(g_sessionsMutex);
                    g_sessions[sessionName] = created;
                }
                RFLOG(Session, "created session '%s' id=%s lobby=%s maxPlayers=%u bucket='%s'",
                      sessionName.c_str(), sessionId.c_str(), lobbyId.c_str(),
                      mod->MaxPlayers, mod->Bucket.c_str());
                result = ER::EOS_Success;
            }
        } else if (exists) {
            LobbyRecord record;
            if (!LobbyDirectory::Get().Get(session.LobbyId, record)) {
                result = ER::EOS_NotFound;
            } else {
                sessionId = session.SessionId;
                if (mod->SetBucket)         record.BucketId     = mod->Bucket;
                if (mod->SetMaxPlayers)     record.MaxMembers   = mod->MaxPlayers;
                if (mod->SetInvitesAllowed) { record.AllowInvites = mod->InvitesAllowed;
                                              SetReservedBool(record, REFIX_SESSION_INVITES_KEY, mod->InvitesAllowed); }
                if (mod->SetJoinInProgress) SetReservedBool(record, REFIX_SESSION_JIP_KEY, mod->JoinInProgress);
                if (mod->SetHostAddress)    SetReserved(record, REFIX_SESSION_HOST_KEY, mod->HostAddress);
                if (mod->SetPermission) {
                    record.Permission =
                        mod->Permission == EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly
                            ? EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY
                            : (mod->Permission == EOS_EOnlineSessionPermissionLevel::EOS_OSPF_JoinViaPresence
                                   ? EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE
                                   : EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED);
                }
                for (const auto& key : mod->RemoveAttributes) {
                    for (size_t i = 0; i < record.Attributes.size(); ) {
                        if (record.Attributes[i].Key == key) record.Attributes.erase(record.Attributes.begin() + i);
                        else ++i;
                    }
                }
                for (const auto& a : mod->AddAttributes) {
                    bool replaced = false;
                    for (auto& existingAttr : record.Attributes)
                        if (existingAttr.Key == a.Key) { existingAttr = a; replaced = true; break; }
                    if (!replaced) record.Attributes.push_back(a);
                }
                result = LobbyDirectory::Get().UpdateLobby(record) ? ER::EOS_Success : ER::EOS_NotFound;
                RFLOG(Session, "updated session '%s' -> %d", sessionName.c_str(), (int)result);
            }
        } else {
            result = ER::EOS_NotFound;
        }
    }

    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, sessionName, sessionId]() {
        EOS_Sessions_UpdateSessionCallbackInfo info{};
        info.ResultCode  = result;
        info.ClientData  = ClientData;
        info.SessionName = sessionName.c_str();
        info.SessionId   = sessionId.empty() ? nullptr : sessionId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Sessions_DestroySession(EOS_HSessions Handle, const EOS_Sessions_DestroySessionOptions* Options, void* ClientData, const EOS_Sessions_OnDestroySessionCallback CompletionDelegate) {
    ER result = ER::EOS_NotFound;
    std::string name = (Options && Options->SessionName) ? Options->SessionName : "";
    LocalSession session;
    if (!name.empty() && LookupByName(name, session)) {
        if (LobbyDirectory::Get().IsHosting(session.LobbyId))
            LobbyDirectory::Get().DestroyLobby(session.LobbyId);
        else
            LobbyDirectory::Get().Leave(session.LobbyId, Identity::Get().LocalUser().Puid);
        {
            std::lock_guard<std::mutex> lock(g_sessionsMutex);
            g_sessions.erase(name);
        }
        result = ER::EOS_Success;
    }
    RFLOG(Session, "destroy session '%s' -> %d", name.c_str(), (int)result);
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
        EOS_Sessions_DestroySessionCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Sessions_JoinSession(EOS_HSessions Handle, const EOS_Sessions_JoinSessionOptions* Options, void* ClientData, const EOS_Sessions_OnJoinSessionCallback CompletionDelegate) {
    ER result = ER::EOS_InvalidParameters;
    if (Options && Options->SessionName && Options->SessionHandle) {
        auto* details = AsDetails(Options->SessionHandle);
        if (details && StartOnlineSubsystem()) {
            if (LobbyDirectory::Get().Join(details->Record, LocalMember())) {
                LocalSession joined;
                joined.Name      = Options->SessionName;
                joined.LobbyId   = details->Record.LobbyId;
                joined.SessionId = ReservedString(details->Record, REFIX_SESSION_ID_KEY, details->Record.LobbyId);
                joined.State     = EOS_EOnlineSessionState::EOS_OSS_Pending;
                {
                    std::lock_guard<std::mutex> lock(g_sessionsMutex);
                    g_sessions[joined.Name] = joined;
                }
                RFLOG(Session, "joining session '%s' (%s)", joined.Name.c_str(), joined.SessionId.c_str());
                result = ER::EOS_Success;
            } else {
                result = ER::EOS_NoConnection;
            }
        }
    }
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
        EOS_Sessions_JoinSessionCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

namespace {
// Transitions the recorded lifecycle state of a session the local player owns.
ER SetSessionState(const std::string& name, EOS_EOnlineSessionState state, const char* label) {
    LocalSession session;
    if (name.empty() || !LookupByName(name, session)) return ER::EOS_NotFound;
    {
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        g_sessions[name].State = state;
    }
    LobbyRecord record;
    if (LobbyDirectory::Get().IsHosting(session.LobbyId) &&
        LobbyDirectory::Get().Get(session.LobbyId, record)) {
        SetReserved(record, REFIX_SESSION_STATE_KEY, label);
        LobbyDirectory::Get().UpdateLobby(record);
    }
    RFLOG(Session, "session '%s' -> %s", name.c_str(), label);
    return ER::EOS_Success;
}
} // namespace

EOS_DECLARE_FUNC(void) EOS_Sessions_StartSession(EOS_HSessions Handle, const EOS_Sessions_StartSessionOptions* Options, void* ClientData, const EOS_Sessions_OnStartSessionCallback CompletionDelegate) {
    ER result = SetSessionState(Options && Options->SessionName ? Options->SessionName : "",
                                EOS_EOnlineSessionState::EOS_OSS_InProgress, "InProgress");
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
        EOS_Sessions_StartSessionCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Sessions_EndSession(EOS_HSessions Handle, const EOS_Sessions_EndSessionOptions* Options, void* ClientData, const EOS_Sessions_OnEndSessionCallback CompletionDelegate) {
    ER result = SetSessionState(Options && Options->SessionName ? Options->SessionName : "",
                                EOS_EOnlineSessionState::EOS_OSS_Ended, "Ended");
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
        EOS_Sessions_EndSessionCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Sessions_RegisterPlayers(EOS_HSessions Handle, const EOS_Sessions_RegisterPlayersOptions* Options, void* ClientData, const EOS_Sessions_OnRegisterPlayersCallback CompletionDelegate) {
    // Membership is already maintained by the host; registration is an
    // acknowledgement, and the real player ids are echoed back so the caller
    // sees exactly the users it asked about.
    std::vector<EOS_ProductUserId> ids;
    if (Options && Options->PlayersToRegister)
        for (uint32_t i = 0; i < Options->PlayersToRegisterCount; i++)
            ids.push_back(Options->PlayersToRegister[i]);

    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, ids]() mutable {
        EOS_Sessions_RegisterPlayersCallbackInfo info{};
        info.ResultCode                = ER::EOS_Success;
        info.ClientData                = ClientData;
        info.RegisteredPlayers         = ids.empty() ? nullptr : ids.data();
        info.RegisteredPlayersCount    = (uint32_t)ids.size();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Sessions_UnregisterPlayers(EOS_HSessions Handle, const EOS_Sessions_UnregisterPlayersOptions* Options, void* ClientData, const EOS_Sessions_OnUnregisterPlayersCallback CompletionDelegate) {
    std::vector<EOS_ProductUserId> ids;
    if (Options && Options->PlayersToUnregister)
        for (uint32_t i = 0; i < Options->PlayersToUnregisterCount; i++)
            ids.push_back(Options->PlayersToUnregister[i]);

    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, ids]() mutable {
        EOS_Sessions_UnregisterPlayersCallbackInfo info{};
        info.ResultCode                 = ER::EOS_Success;
        info.ClientData                 = ClientData;
        info.UnregisteredPlayers        = ids.empty() ? nullptr : ids.data();
        info.UnregisteredPlayersCount   = (uint32_t)ids.size();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Sessions_IsUserInSession(EOS_HSessions Handle, const EOS_Sessions_IsUserInSessionOptions* Options) {
    if (!Options || !Options->SessionName) return ER::EOS_InvalidParameters;
    LocalSession session;
    if (!LookupByName(Options->SessionName, session)) return ER::EOS_NotFound;
    LobbyRecord record;
    if (!LobbyDirectory::Get().Get(session.LobbyId, record)) return ER::EOS_NotFound;
    return record.FindMember(PuidString(Options->TargetUserId)) ? ER::EOS_Success : ER::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Sessions_CopyActiveSessionHandle(EOS_HSessions Handle, const EOS_Sessions_CopyActiveSessionHandleOptions* Options, EOS_HActiveSession* OutSessionHandle) {
    if (!Options || !OutSessionHandle || !Options->SessionName) return ER::EOS_InvalidParameters;
    *OutSessionHandle = nullptr;
    LocalSession session;
    if (!LookupByName(Options->SessionName, session)) return ER::EOS_NotFound;
    LobbyRecord record;
    if (!LobbyDirectory::Get().Get(session.LobbyId, record)) return ER::EOS_NotFound;

    auto* active = new EOS_ActiveSessionHandle();
    active->Magic       = kMagicActiveSession;
    active->SessionName = session.Name;
    active->Record      = std::move(record);
    active->State       = session.State;
    *OutSessionHandle = (EOS_HActiveSession)active;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Sessions_CreateSessionSearch(EOS_HSessions Handle, const EOS_Sessions_CreateSessionSearchOptions* Options, EOS_HSessionSearch* OutSessionSearchHandle) {
    if (!OutSessionSearchHandle) return ER::EOS_InvalidParameters;
    auto* search = new EOS_SessionSearchHandle();
    search->Magic      = kMagicSessionSearch;
    search->MaxResults = (Options && Options->MaxSearchResults) ? Options->MaxSearchResults : 50;
    *OutSessionSearchHandle = (EOS_HSessionSearch)search;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Sessions_SendInvite(EOS_HSessions Handle, const EOS_Sessions_SendInviteOptions* Options, void* ClientData, const EOS_Sessions_OnSendInviteCallback CompletionDelegate) {
    ER result = ER::EOS_NotFound;
    if (Options && Options->SessionName) {
        LocalSession session;
        if (LookupByName(Options->SessionName, session))
            result = LobbyDirectory::Get().SendInvite(session.LobbyId, PuidString(Options->TargetUserId))
                         ? ER::EOS_Success : ER::EOS_NotFound;
    }
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
        EOS_Sessions_SendInviteCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

#define REFIX_SESSION_NOTIFY(FnName, OptionsType, CallbackType, Kind)                                \
    EOS_DECLARE_FUNC(EOS_NotificationId) FnName(EOS_HSessions Handle, const OptionsType* Options,    \
                                                void* ClientData, const CallbackType Handler) {      \
        return Dispatcher::Get().Register(NotifyKind::Kind, (void*)Handler, ClientData);             \
    }

REFIX_SESSION_NOTIFY(EOS_Sessions_AddNotifySessionInviteReceived, EOS_Sessions_AddNotifySessionInviteReceivedOptions, EOS_Sessions_OnSessionInviteReceivedCallback, SessionInviteReceived)
REFIX_SESSION_NOTIFY(EOS_Sessions_AddNotifySessionInviteAccepted, EOS_Sessions_AddNotifySessionInviteAcceptedOptions, EOS_Sessions_OnSessionInviteAcceptedCallback, SessionInviteAccepted)
REFIX_SESSION_NOTIFY(EOS_Sessions_AddNotifyJoinSessionAccepted,   EOS_Sessions_AddNotifyJoinSessionAcceptedOptions,   EOS_Sessions_OnJoinSessionAcceptedCallback,   JoinSessionAccepted)

EOS_DECLARE_FUNC(void) EOS_Sessions_RemoveNotifySessionInviteReceived(EOS_HSessions Handle, EOS_NotificationId InId) { Dispatcher::Get().Unregister(InId); }
EOS_DECLARE_FUNC(void) EOS_Sessions_RemoveNotifySessionInviteAccepted(EOS_HSessions Handle, EOS_NotificationId InId) { Dispatcher::Get().Unregister(InId); }
EOS_DECLARE_FUNC(void) EOS_Sessions_RemoveNotifyJoinSessionAccepted(EOS_HSessions Handle, EOS_NotificationId InId)   { Dispatcher::Get().Unregister(InId); }

// ===========================================================================
// EOS_SessionModification_*
// ===========================================================================
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetBucketId(EOS_HSessionModification Handle, const EOS_SessionModification_SetBucketIdOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options || !Options->BucketId) return ER::EOS_InvalidParameters;
    m->SetBucket = true; m->Bucket = Options->BucketId;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetMaxPlayers(EOS_HSessionModification Handle, const EOS_SessionModification_SetMaxPlayersOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options || Options->MaxPlayers == 0) return ER::EOS_InvalidParameters;
    m->SetMaxPlayers = true; m->MaxPlayers = Options->MaxPlayers;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetPermissionLevel(EOS_HSessionModification Handle, const EOS_SessionModification_SetPermissionLevelOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options) return ER::EOS_InvalidParameters;
    m->SetPermission = true; m->Permission = Options->PermissionLevel;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetJoinInProgressAllowed(EOS_HSessionModification Handle, const EOS_SessionModification_SetJoinInProgressAllowedOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options) return ER::EOS_InvalidParameters;
    m->SetJoinInProgress = true; m->JoinInProgress = Options->bAllowJoinInProgress != EOS_FALSE;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetInvitesAllowed(EOS_HSessionModification Handle, const EOS_SessionModification_SetInvitesAllowedOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options) return ER::EOS_InvalidParameters;
    m->SetInvitesAllowed = true; m->InvitesAllowed = Options->bInvitesAllowed != EOS_FALSE;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_SetHostAddress(EOS_HSessionModification Handle, const EOS_SessionModification_SetHostAddressOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options || !Options->HostAddress) return ER::EOS_InvalidParameters;
    m->SetHostAddress = true; m->HostAddress = Options->HostAddress;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_AddAttribute(EOS_HSessionModification Handle, const EOS_SessionModification_AddAttributeOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options || !Options->SessionAttribute || !Options->SessionAttribute->Key) return ER::EOS_InvalidParameters;
    if (IsReserved(Options->SessionAttribute->Key)) return ER::EOS_InvalidParameters;
    Attribute a;
    a.Key   = Options->SessionAttribute->Key;
    a.Value = FromSdk(*Options->SessionAttribute);
    a.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
    m->AddAttributes.push_back(std::move(a));
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionModification_RemoveAttribute(EOS_HSessionModification Handle, const EOS_SessionModification_RemoveAttributeOptions* Options) {
    auto* m = AsModification(Handle);
    if (!m || !Options || !Options->Key) return ER::EOS_InvalidParameters;
    m->RemoveAttributes.push_back(Options->Key);
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(void) EOS_SessionModification_Release(EOS_HSessionModification SessionModificationHandle) {
    auto* m = AsModification(SessionModificationHandle);
    if (!m) return;
    m->Magic = 0;
    delete m;
}

// ===========================================================================
// EOS_SessionDetails_*
// ===========================================================================
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionDetails_CopyInfo(EOS_HSessionDetails Handle, const EOS_SessionDetails_CopyInfoOptions* Options, EOS_SessionDetails_Info** OutSessionInfo) {
    auto* d = AsDetails(Handle);
    if (!d || !OutSessionInfo) return ER::EOS_InvalidParameters;
    *OutSessionInfo = MakeSessionInfo(d->Record);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_SessionDetails_Info_Release(EOS_SessionDetails_Info* SessionInfo) {
    if (!SessionInfo) return;
    SessionInfoBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_infoMutex);
        auto it = g_infoBlocks.find(SessionInfo);
        if (it == g_infoBlocks.end()) return;
        block = it->second;
        g_infoBlocks.erase(it);
    }
    delete block;
}

EOS_DECLARE_FUNC(uint32_t) EOS_SessionDetails_GetSessionAttributeCount(EOS_HSessionDetails Handle, const EOS_SessionDetails_GetSessionAttributeCountOptions* Options) {
    auto* d = AsDetails(Handle);
    return d ? (uint32_t)VisibleAttributes(d->Record).size() : 0u;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionDetails_CopySessionAttributeByIndex(EOS_HSessionDetails Handle, const EOS_SessionDetails_CopySessionAttributeByIndexOptions* Options, EOS_SessionDetails_Attribute** OutSessionAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutSessionAttribute) return ER::EOS_InvalidParameters;
    *OutSessionAttribute = nullptr;
    auto visible = VisibleAttributes(d->Record);
    if (Options->AttrIndex >= visible.size()) return ER::EOS_NotFound;
    *OutSessionAttribute = MakeSessionAttribute(visible[Options->AttrIndex]);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionDetails_CopySessionAttributeByKey(EOS_HSessionDetails Handle, const EOS_SessionDetails_CopySessionAttributeByKeyOptions* Options, EOS_SessionDetails_Attribute** OutSessionAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutSessionAttribute || !Options->AttrKey) return ER::EOS_InvalidParameters;
    *OutSessionAttribute = nullptr;
    if (IsReserved(Options->AttrKey)) return ER::EOS_NotFound;
    const Attribute* a = d->Record.FindAttribute(Options->AttrKey);
    if (!a) return ER::EOS_NotFound;
    *OutSessionAttribute = MakeSessionAttribute(*a);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_SessionDetails_Attribute_Release(EOS_SessionDetails_Attribute* SessionAttribute) {
    if (!SessionAttribute) return;
    SessionAttributeBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_attrMutex);
        auto it = g_attrBlocks.find(SessionAttribute);
        if (it == g_attrBlocks.end()) return;
        block = it->second;
        g_attrBlocks.erase(it);
    }
    delete block;
}

EOS_DECLARE_FUNC(void) EOS_SessionDetails_Release(EOS_HSessionDetails SessionHandle) {
    auto* d = AsDetails(SessionHandle);
    if (!d) return;
    d->Magic = 0;
    delete d;
}

// ===========================================================================
// EOS_SessionSearch_*
// ===========================================================================
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_SetSessionId(EOS_HSessionSearch Handle, const EOS_SessionSearch_SetSessionIdOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->SessionId) return ER::EOS_InvalidParameters;
    s->SessionIdFilter = Options->SessionId;
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_SetTargetUserId(EOS_HSessionSearch Handle, const EOS_SessionSearch_SetTargetUserIdOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options) return ER::EOS_InvalidParameters;
    s->TargetUserFilter = PuidString(Options->TargetUserId);
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_SetParameter(EOS_HSessionSearch Handle, const EOS_SessionSearch_SetParameterOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->Parameter || !Options->Parameter->Key) return ER::EOS_InvalidParameters;
    SearchParameter p;
    p.Key   = Options->Parameter->Key;
    p.Value = FromSdk(*Options->Parameter);
    p.Op    = Options->ComparisonOp;
    s->Parameters.push_back(std::move(p));
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_RemoveParameter(EOS_HSessionSearch Handle, const EOS_SessionSearch_RemoveParameterOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->Key) return ER::EOS_InvalidParameters;
    for (size_t i = 0; i < s->Parameters.size(); ) {
        if (s->Parameters[i].Key == Options->Key) s->Parameters.erase(s->Parameters.begin() + i);
        else ++i;
    }
    return ER::EOS_Success;
}
EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_SetMaxResults(EOS_HSessionSearch Handle, const EOS_SessionSearch_SetMaxResultsOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || Options->MaxSearchResults == 0) return ER::EOS_InvalidParameters;
    s->MaxResults = Options->MaxSearchResults;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_SessionSearch_Find(EOS_HSessionSearch Handle, const EOS_SessionSearch_FindOptions* Options, void* ClientData, const EOS_SessionSearch_OnFindCallback CompletionDelegate) {
    auto* s = AsSearch(Handle);
    if (!s || !StartOnlineSubsystem()) {
        if (CompletionDelegate)
            Dispatcher::Get().PostGenericCompletion((void*)CompletionDelegate, ClientData,
                                                    s ? ER::EOS_NoConnection : ER::EOS_InvalidParameters);
        return;
    }
    s->LocalPuid = Options ? PuidString(Options->LocalUserId) : "";
    s->Results.clear();
    LobbyDirectory::Get().RequestSearch();

    const uint32_t waitMs = (uint32_t)Config::Get().GetInt("Network", "SearchWaitMs", 900);
    Dispatcher::Get().PostAfter(waitMs, [s, ClientData, CompletionDelegate]() {
        if (s->Magic != kMagicSessionSearch) return;
        for (const auto& record : LobbyDirectory::Get().Snapshot()) {
            if (s->Results.size() >= s->MaxResults) break;
            if (!IsSessionRecord(record)) continue;
            if (!s->SessionIdFilter.empty() &&
                ReservedString(record, REFIX_SESSION_ID_KEY, record.LobbyId) != s->SessionIdFilter) continue;
            if (!s->TargetUserFilter.empty() && !record.FindMember(s->TargetUserFilter)) continue;
            if (record.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY) continue;
            bool matches = true;
            for (const auto& p : s->Parameters) if (!p.Matches(record)) { matches = false; break; }
            if (matches) s->Results.push_back(record);
        }
        RFLOG(Session, "EOS_SessionSearch_Find: %zu session(s) matched", s->Results.size());
        if (!CompletionDelegate) return;
        EOS_SessionSearch_FindCallbackInfo info{};
        info.ResultCode = ER::EOS_Success;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(uint32_t) EOS_SessionSearch_GetSearchResultCount(EOS_HSessionSearch Handle, const EOS_SessionSearch_GetSearchResultCountOptions* Options) {
    auto* s = AsSearch(Handle);
    return s ? (uint32_t)s->Results.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_SessionSearch_CopySearchResultByIndex(EOS_HSessionSearch Handle, const EOS_SessionSearch_CopySearchResultByIndexOptions* Options, EOS_HSessionDetails* OutSessionHandle) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !OutSessionHandle) return ER::EOS_InvalidParameters;
    *OutSessionHandle = nullptr;
    if (Options->SessionIndex >= s->Results.size()) return ER::EOS_NotFound;
    auto* d = new EOS_SessionDetailsHandle();
    d->Magic  = kMagicSessionDetails;
    d->Record = s->Results[Options->SessionIndex];
    *OutSessionHandle = (EOS_HSessionDetails)d;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_SessionSearch_Release(EOS_HSessionSearch SessionSearchHandle) {
    auto* s = AsSearch(SessionSearchHandle);
    if (!s) return;
    s->Magic = 0;
    delete s;
}

// ===========================================================================
// EOS_ActiveSession_*
// ===========================================================================
namespace {
struct ActiveInfoBlock {
    EOS_ActiveSession_Info   Info;
    std::string              SessionName;
    EOS_SessionDetails_Info* Details = nullptr;
};
std::mutex                                        g_activeMutex;
std::map<EOS_ActiveSession_Info*, ActiveInfoBlock*> g_activeBlocks;
} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_ActiveSession_CopyInfo(EOS_HActiveSession Handle, const EOS_ActiveSession_CopyInfoOptions* Options, EOS_ActiveSession_Info** OutActiveSessionInfo) {
    auto* a = AsActive(Handle);
    if (!a || !OutActiveSessionInfo) return ER::EOS_InvalidParameters;
    auto* block = new ActiveInfoBlock();
    block->SessionName = a->SessionName;
    block->Details     = MakeSessionInfo(a->Record);
    std::memset(&block->Info, 0, sizeof(block->Info));
    block->Info.ApiVersion     = EOS_ACTIVESESSION_COPYINFO_API_LATEST;
    block->Info.SessionName    = block->SessionName.c_str();
    block->Info.LocalUserId    = Identity::Get().LocalPuid();
    block->Info.State          = a->State;
    block->Info.SessionDetails = block->Details;
    {
        std::lock_guard<std::mutex> lock(g_activeMutex);
        g_activeBlocks[&block->Info] = block;
    }
    *OutActiveSessionInfo = &block->Info;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_ActiveSession_Info_Release(EOS_ActiveSession_Info* ActiveSessionInfo) {
    if (!ActiveSessionInfo) return;
    ActiveInfoBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_activeMutex);
        auto it = g_activeBlocks.find(ActiveSessionInfo);
        if (it == g_activeBlocks.end()) return;
        block = it->second;
        g_activeBlocks.erase(it);
    }
    EOS_SessionDetails_Info_Release(block->Details);
    delete block;
}

EOS_DECLARE_FUNC(uint32_t) EOS_ActiveSession_GetRegisteredPlayerCount(EOS_HActiveSession Handle, const EOS_ActiveSession_GetRegisteredPlayerCountOptions* Options) {
    auto* a = AsActive(Handle);
    return a ? (uint32_t)a->Record.Members.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_ActiveSession_GetRegisteredPlayerByIndex(EOS_HActiveSession Handle, const EOS_ActiveSession_GetRegisteredPlayerByIndexOptions* Options) {
    auto* a = AsActive(Handle);
    if (!a || !Options || Options->PlayerIndex >= a->Record.Members.size()) return nullptr;
    return IdRegistry::Get().Puid(a->Record.Members[Options->PlayerIndex].Puid);
}

EOS_DECLARE_FUNC(void) EOS_ActiveSession_Release(EOS_HActiveSession ActiveSessionHandle) {
    auto* a = AsActive(ActiveSessionHandle);
    if (!a) return;
    a->Magic = 0;
    delete a;
}

namespace refix {

void RegisterSessionsApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_Sessions_CreateSessionModification);
    REFIX_BIND(reg, EOS_Sessions_UpdateSessionModification);
    REFIX_BIND(reg, EOS_Sessions_UpdateSession);
    REFIX_BIND(reg, EOS_Sessions_DestroySession);
    REFIX_BIND(reg, EOS_Sessions_JoinSession);
    REFIX_BIND(reg, EOS_Sessions_StartSession);
    REFIX_BIND(reg, EOS_Sessions_EndSession);
    REFIX_BIND(reg, EOS_Sessions_RegisterPlayers);
    REFIX_BIND(reg, EOS_Sessions_UnregisterPlayers);
    REFIX_BIND(reg, EOS_Sessions_IsUserInSession);
    REFIX_BIND(reg, EOS_Sessions_CopyActiveSessionHandle);
    REFIX_BIND(reg, EOS_Sessions_CreateSessionSearch);
    REFIX_BIND(reg, EOS_Sessions_SendInvite);
    REFIX_BIND(reg, EOS_Sessions_AddNotifySessionInviteReceived);
    REFIX_BIND(reg, EOS_Sessions_AddNotifySessionInviteAccepted);
    REFIX_BIND(reg, EOS_Sessions_AddNotifyJoinSessionAccepted);
    REFIX_BIND(reg, EOS_Sessions_RemoveNotifySessionInviteReceived);
    REFIX_BIND(reg, EOS_Sessions_RemoveNotifySessionInviteAccepted);
    REFIX_BIND(reg, EOS_Sessions_RemoveNotifyJoinSessionAccepted);

    REFIX_BIND(reg, EOS_SessionModification_SetBucketId);
    REFIX_BIND(reg, EOS_SessionModification_SetMaxPlayers);
    REFIX_BIND(reg, EOS_SessionModification_SetPermissionLevel);
    REFIX_BIND(reg, EOS_SessionModification_SetJoinInProgressAllowed);
    REFIX_BIND(reg, EOS_SessionModification_SetInvitesAllowed);
    REFIX_BIND(reg, EOS_SessionModification_SetHostAddress);
    REFIX_BIND(reg, EOS_SessionModification_AddAttribute);
    REFIX_BIND(reg, EOS_SessionModification_RemoveAttribute);
    REFIX_BIND(reg, EOS_SessionModification_Release);

    REFIX_BIND(reg, EOS_SessionDetails_CopyInfo);
    REFIX_BIND(reg, EOS_SessionDetails_Info_Release);
    REFIX_BIND(reg, EOS_SessionDetails_GetSessionAttributeCount);
    REFIX_BIND(reg, EOS_SessionDetails_CopySessionAttributeByIndex);
    REFIX_BIND(reg, EOS_SessionDetails_CopySessionAttributeByKey);
    REFIX_BIND(reg, EOS_SessionDetails_Attribute_Release);
    REFIX_BIND(reg, EOS_SessionDetails_Release);

    REFIX_BIND(reg, EOS_SessionSearch_SetSessionId);
    REFIX_BIND(reg, EOS_SessionSearch_SetTargetUserId);
    REFIX_BIND(reg, EOS_SessionSearch_SetParameter);
    REFIX_BIND(reg, EOS_SessionSearch_RemoveParameter);
    REFIX_BIND(reg, EOS_SessionSearch_SetMaxResults);
    REFIX_BIND(reg, EOS_SessionSearch_Find);
    REFIX_BIND(reg, EOS_SessionSearch_GetSearchResultCount);
    REFIX_BIND(reg, EOS_SessionSearch_CopySearchResultByIndex);
    REFIX_BIND(reg, EOS_SessionSearch_Release);

    REFIX_BIND(reg, EOS_ActiveSession_CopyInfo);
    REFIX_BIND(reg, EOS_ActiveSession_Info_Release);
    REFIX_BIND(reg, EOS_ActiveSession_GetRegisteredPlayerCount);
    REFIX_BIND(reg, EOS_ActiveSession_GetRegisteredPlayerByIndex);
    REFIX_BIND(reg, EOS_ActiveSession_Release);
}

} // namespace refix
