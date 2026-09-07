// ReFix EOS v3 - EOS_Lobby_*, EOS_LobbyModification_*, EOS_LobbyDetails_*,
// EOS_LobbySearch_*.
//
// This is the interface a title uses to host and find a game. Three principles
// run through it:
//
//   * the lobby is described by what the title configured, never by defaults -
//     max members, bucket, permission level and every attribute round-trip
//     exactly as they were set;
//   * the host is the only authority on membership, so the member list a client
//     reads is the host's list, not a local guess;
//   * handles are typed and snapshot their data, so a LobbyDetails obtained
//     before a change keeps describing what it described, and a stale handle is
//     rejected rather than dereferenced.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/refix_config.h"
#include "../core/eos_handles.h"
#include "../core/lobby_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../core/eos_online.h"
#include "../net/lobby_directory.h"
#include "../eos_module.h"

using namespace refix;

namespace {

// --- attribute blocks handed to the title -------------------------------
// EOS_Lobby_Attribute points at an EOS_Lobby_AttributeData which points at
// strings. All three live in one block so EOS_Lobby_Attribute_Release frees
// everything with a single delete.
struct AttributeBlock {
    EOS_Lobby_Attribute     Attribute;
    EOS_Lobby_AttributeData Data;
    std::string             Key;
    std::string             Utf8;
};

std::mutex                                        g_attrMutex;
std::map<EOS_Lobby_Attribute*, AttributeBlock*>   g_attrBlocks;

EOS_Lobby_Attribute* MakeAttribute(const Attribute& src) {
    auto* block = new AttributeBlock();
    block->Key  = src.Key;
    block->Utf8 = src.Value.AsUtf8;

    std::memset(&block->Data, 0, sizeof(block->Data));
    block->Data.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    block->Data.Key        = block->Key.c_str();
    block->Data.ValueType  = src.Value.Type;
    switch (src.Value.Type) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:   block->Data.Value.AsInt64  = src.Value.AsInt64; break;
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:  block->Data.Value.AsDouble = src.Value.AsDouble; break;
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN: block->Data.Value.AsBool   = src.Value.AsBool ? EOS_TRUE : EOS_FALSE; break;
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:                                      block->Data.Value.AsUtf8   = block->Utf8.c_str(); break;
    }

    std::memset(&block->Attribute, 0, sizeof(block->Attribute));
    block->Attribute.ApiVersion = EOS_LOBBY_ATTRIBUTE_API_LATEST;
    block->Attribute.Data       = &block->Data;
    block->Attribute.Visibility = src.Visibility;

    std::lock_guard<std::mutex> lock(g_attrMutex);
    g_attrBlocks[&block->Attribute] = block;
    return &block->Attribute;
}

// --- info blocks ---------------------------------------------------------
struct InfoBlock {
    EOS_LobbyDetails_Info Info;
    std::string           LobbyId;
    std::string           BucketId;
};
std::mutex                                     g_infoMutex;
std::map<EOS_LobbyDetails_Info*, InfoBlock*>   g_infoBlocks;

std::mutex                                              g_memberInfoMutex;
std::map<EOS_LobbyDetails_MemberInfo*, EOS_LobbyDetails_MemberInfo*> g_memberInfoBlocks;

// --- conversions ---------------------------------------------------------
AttributeValue FromSdk(const EOS_Lobby_AttributeData& data) {
    AttributeValue v;
    v.Type = data.ValueType;
    switch (data.ValueType) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:   v.AsInt64  = data.Value.AsInt64; break;
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:  v.AsDouble = data.Value.AsDouble; break;
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN: v.AsBool   = data.Value.AsBool != EOS_FALSE; break;
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:                                      v.AsUtf8   = data.Value.AsUtf8 ? data.Value.AsUtf8 : ""; break;
    }
    return v;
}

std::string PuidString(EOS_ProductUserId id) {
    const char* s = IdRegistry::ToString(id);
    return s ? std::string(s) : std::string();
}

LobbyMember LocalMember() {
    const UserRecord& user = Identity::Get().LocalUser();
    LobbyMember m;
    m.Puid        = user.Puid;
    m.DisplayName = user.DisplayName;
    m.Address     = LobbyDirectory::Get().LocalEndpoint();
    return m;
}

// --- pending joins -------------------------------------------------------
// EOS_Lobby_JoinLobby completes when the host answers, so the delegate is
// parked here until the directory reports the outcome (or the attempt times
// out - a completion the title never receives is worse than a failure).
struct PendingJoin {
    std::string                          LobbyId;
    void*                                ClientData = nullptr;
    EOS_Lobby_OnJoinLobbyCallback        Delegate = nullptr;
    EOS_ProductUserId                    LocalUserId = nullptr;
    uint64_t                             DeadlineMs = 0;
    bool                                 Done = false;
};

std::mutex                 g_joinMutex;
std::vector<PendingJoin>   g_pendingJoins;

void CompleteJoin(const std::string& lobbyId, ER result) {
    std::vector<PendingJoin> ready;
    {
        std::lock_guard<std::mutex> lock(g_joinMutex);
        for (auto& p : g_pendingJoins) {
            if (p.Done || p.LobbyId != lobbyId) continue;
            p.Done = true;
            ready.push_back(p);
        }
        for (size_t i = 0; i < g_pendingJoins.size(); ) {
            if (g_pendingJoins[i].Done) g_pendingJoins.erase(g_pendingJoins.begin() + i);
            else ++i;
        }
    }
    for (const auto& p : ready) {
        if (!p.Delegate) continue;
        auto delegate = p.Delegate;
        void* clientData = p.ClientData;
        std::string id = p.LobbyId;
        Dispatcher::Get().Post([delegate, clientData, id, result]() {
            EOS_Lobby_JoinLobbyCallbackInfo info{};
            info.ResultCode = result;
            info.ClientData = clientData;
            info.LobbyId    = id.c_str();
            delegate(&info);
        });
    }
}

// --- notifications -------------------------------------------------------
void RaiseLobbyUpdate(const std::string& lobbyId) {
    Dispatcher::Get().Broadcast<EOS_Lobby_LobbyUpdateReceivedCallbackInfo>(
        NotifyKind::LobbyUpdateReceived, [lobbyId](void* clientData) {
            EOS_Lobby_LobbyUpdateReceivedCallbackInfo info{};
            info.ClientData = clientData;
            info.LobbyId    = lobbyId.c_str();
            return info;
        });
}

void RaiseMemberStatus(const std::string& lobbyId, const std::string& puid,
                       EOS_ELobbyMemberStatus status) {
    EOS_ProductUserId target = IdRegistry::Get().Puid(puid);
    Dispatcher::Get().Broadcast<EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo>(
        NotifyKind::LobbyMemberStatusReceived, [lobbyId, target, status](void* clientData) {
            EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo info{};
            info.ClientData         = clientData;
            info.LobbyId            = lobbyId.c_str();
            info.TargetUserId       = target;
            info.CurrentStatus      = status;
            return info;
        });
}

void RaiseMemberUpdate(const std::string& lobbyId, const std::string& puid) {
    EOS_ProductUserId target = IdRegistry::Get().Puid(puid);
    Dispatcher::Get().Broadcast<EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo>(
        NotifyKind::LobbyMemberUpdateReceived, [lobbyId, target](void* clientData) {
            EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo info{};
            info.ClientData   = clientData;
            info.LobbyId      = lobbyId.c_str();
            info.TargetUserId = target;
            return info;
        });
}

// Wire the directory's events into EOS notifications exactly once.
void InstallDirectoryEvents() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    DirectoryEvents ev;
    ev.OnLobbyUpdated = [](const LobbyRecord& record) {
        CompleteJoin(record.LobbyId, ER::EOS_Success);
        std::string id = record.LobbyId;
        Dispatcher::Get().Post([id]() { RaiseLobbyUpdate(id); });
    };
    ev.OnMemberChanged = [](const LobbyRecord& record, const std::string& puid, MemberChange change) {
        std::string id = record.LobbyId;
        EOS_ELobbyMemberStatus status;
        switch (change) {
            case MemberChange::Joined:       status = EOS_ELobbyMemberStatus::EOS_LMS_JOINED; break;
            case MemberChange::Left:         status = EOS_ELobbyMemberStatus::EOS_LMS_LEFT; break;
            case MemberChange::Disconnected: status = EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED; break;
            case MemberChange::Kicked:       status = EOS_ELobbyMemberStatus::EOS_LMS_KICKED; break;
            case MemberChange::Promoted:     status = EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED; break;
            case MemberChange::Closed:
            default:                         status = EOS_ELobbyMemberStatus::EOS_LMS_CLOSED; break;
        }
        Dispatcher::Get().Post([id, puid, status]() { RaiseMemberStatus(id, puid, status); });
    };
    ev.OnInviteReceived = [](const std::string& lobbyId, const std::string& fromPuid,
                             const std::string& inviteId) {
        EOS_ProductUserId local = Identity::Get().LocalPuid();
        EOS_ProductUserId from  = IdRegistry::Get().Puid(fromPuid);
        Dispatcher::Get().Broadcast<EOS_Lobby_LobbyInviteReceivedCallbackInfo>(
            NotifyKind::LobbyInviteReceived, [inviteId, local, from](void* clientData) {
                EOS_Lobby_LobbyInviteReceivedCallbackInfo info{};
                info.ClientData   = clientData;
                info.InviteId     = inviteId.c_str();
                info.LocalUserId  = local;
                info.TargetUserId = from;
                return info;
            });
    };
    LobbyDirectory::Get().SetEvents(std::move(ev));
}

} // namespace

namespace refix {

bool StartOnlineSubsystem() {
    InstallDirectoryEvents();
    return EnsureOnline();
}

} // namespace refix

namespace {
inline bool StartOnline() { return refix::StartOnlineSubsystem(); }
} // namespace

// ===========================================================================
// EOS_Lobby_*
// ===========================================================================
EOS_DECLARE_FUNC(void) EOS_Lobby_CreateLobby(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnCreateLobbyCallback CompletionDelegate) {
    auto answer = [&](ER result, const std::string& lobbyId) {
        if (!CompletionDelegate) return;
        Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
            EOS_Lobby_CreateLobbyCallbackInfo info{};
            info.ResultCode = result;
            info.ClientData = ClientData;
            info.LobbyId    = lobbyId.empty() ? nullptr : lobbyId.c_str();
            CompletionDelegate(&info);
        });
    };

    if (!Options) { answer(ER::EOS_InvalidParameters, ""); return; }
    if (!IdRegistry::IsValidPuid(Options->LocalUserId)) {
        RFLOG(Lobby, "CreateLobby: invalid LocalUserId");
        answer(ER::EOS_InvalidUser, "");
        return;
    }
    if (Options->MaxLobbyMembers == 0) {
        RFLOG(Lobby, "CreateLobby: MaxLobbyMembers is 0");
        answer(ER::EOS_InvalidParameters, "");
        return;
    }
    if (!StartOnline()) { answer(ER::EOS_NoConnection, ""); return; }

    LobbyRecord record;
    record.LobbyId    = (Options->LobbyId && *Options->LobbyId) ? Options->LobbyId : "";
    record.OwnerPuid  = PuidString(Options->LocalUserId);
    record.BucketId   = Options->BucketId ? Options->BucketId : "";
    record.MaxMembers = Options->MaxLobbyMembers;
    record.Permission = Options->PermissionLevel;
    record.AllowInvites                  = Options->bAllowInvites != EOS_FALSE;
    record.AllowHostMigration            = Options->bDisableHostMigration == EOS_FALSE;
    record.RtcRoomEnabled                = Options->bEnableRTCRoom != EOS_FALSE;
    record.AllowJoinById                 = Options->bEnableJoinById != EOS_FALSE;
    record.RejoinAfterKickRequiresInvite = Options->bRejoinAfterKickRequiresInvite != EOS_FALSE;
    record.PresenceEnabled               = Options->bPresenceEnabled != EOS_FALSE;

    LobbyMember owner = LocalMember();
    owner.Puid    = record.OwnerPuid;
    owner.IsOwner = true;
    record.Members.push_back(owner);

    std::string lobbyId = LobbyDirectory::Get().CreateLobby(std::move(record));
    RFLOG(Lobby, "EOS_Lobby_CreateLobby -> %s (max=%u bucket='%s' permission=%d)",
          lobbyId.c_str(), Options->MaxLobbyMembers, Options->BucketId ? Options->BucketId : "",
          (int)Options->PermissionLevel);
    answer(ER::EOS_Success, lobbyId);
}

EOS_DECLARE_FUNC(void) EOS_Lobby_DestroyLobby(EOS_HLobby Handle, const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnDestroyLobbyCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    ER result = ER::EOS_NotFound;
    if (!lobbyId.empty()) {
        if (LobbyDirectory::Get().DestroyLobby(lobbyId)) result = ER::EOS_Success;
        else if (LobbyDirectory::Get().Leave(lobbyId, Identity::Get().LocalUser().Puid)) result = ER::EOS_Success;
    }
    RFLOG(Lobby, "EOS_Lobby_DestroyLobby(%s) -> %d", lobbyId.c_str(), (int)result);
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_DestroyLobbyCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Lobby_JoinLobby(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnJoinLobbyCallback CompletionDelegate) {
    auto fail = [&](ER result, const char* why) {
        RFLOG(Lobby, "EOS_Lobby_JoinLobby failed: %s", why);
        if (!CompletionDelegate) return;
        Dispatcher::Get().Post([CompletionDelegate, ClientData, result]() {
            EOS_Lobby_JoinLobbyCallbackInfo info{};
            info.ResultCode = result;
            info.ClientData = ClientData;
            info.LobbyId    = nullptr;
            CompletionDelegate(&info);
        });
    };

    if (!Options || !Options->LobbyDetailsHandle) { fail(ER::EOS_InvalidParameters, "no details handle"); return; }
    auto* details = (EOS_LobbyDetailsHandle*)Options->LobbyDetailsHandle;
    if (details->Magic != kMagicLobbyDetails) { fail(ER::EOS_InvalidParameters, "stale details handle"); return; }
    if (!StartOnline()) { fail(ER::EOS_NoConnection, "transport unavailable"); return; }

    LobbyMember self = LocalMember();
    if (!LobbyDirectory::Get().Join(details->Record, self)) {
        fail(ER::EOS_NoConnection, "join request could not be sent");
        return;
    }

    PendingJoin pending;
    pending.LobbyId     = details->Record.LobbyId;
    pending.ClientData  = ClientData;
    pending.Delegate    = CompletionDelegate;
    pending.LocalUserId = Options->LocalUserId;
    {
        std::lock_guard<std::mutex> lock(g_joinMutex);
        g_pendingJoins.push_back(pending);
    }

    // A host that never answers must not leave the title waiting forever.
    std::string lobbyId = pending.LobbyId;
    Dispatcher::Get().PostAfter(8000, [lobbyId]() { CompleteJoin(lobbyId, ER::EOS_TimedOut); });
}

EOS_DECLARE_FUNC(void) EOS_Lobby_JoinLobbyById(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyByIdOptions* Options, void* ClientData, const EOS_Lobby_OnJoinLobbyByIdCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    auto answer = [&](ER result) {
        if (!CompletionDelegate) return;
        Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
            EOS_Lobby_JoinLobbyByIdCallbackInfo info{};
            info.ResultCode = result;
            info.ClientData = ClientData;
            info.LobbyId    = lobbyId.c_str();
            CompletionDelegate(&info);
        });
    };

    if (lobbyId.empty() || !StartOnline()) { answer(ER::EOS_InvalidParameters); return; }
    LobbyRecord record;
    if (!LobbyDirectory::Get().Get(lobbyId, record)) {
        // Ask the network; the id may belong to a lobby we have not heard of yet.
        LobbyDirectory::Get().RequestSearch();
        Dispatcher::Get().PostAfter(1200, [lobbyId, ClientData, CompletionDelegate]() {
            LobbyRecord found;
            ER result = ER::EOS_NotFound;
            if (LobbyDirectory::Get().Get(lobbyId, found) &&
                LobbyDirectory::Get().Join(found, LocalMember())) {
                result = ER::EOS_Success;
            }
            if (!CompletionDelegate) return;
            EOS_Lobby_JoinLobbyByIdCallbackInfo info{};
            info.ResultCode = result;
            info.ClientData = ClientData;
            info.LobbyId    = lobbyId.c_str();
            CompletionDelegate(&info);
        });
        return;
    }
    answer(LobbyDirectory::Get().Join(record, LocalMember()) ? ER::EOS_Success : ER::EOS_NoConnection);
}

EOS_DECLARE_FUNC(void) EOS_Lobby_LeaveLobby(EOS_HLobby Handle, const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnLeaveLobbyCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    ER result = ER::EOS_NotFound;
    if (!lobbyId.empty() &&
        LobbyDirectory::Get().Leave(lobbyId, Identity::Get().LocalUser().Puid)) {
        result = ER::EOS_Success;
    }
    RFLOG(Lobby, "EOS_Lobby_LeaveLobby(%s) -> %d", lobbyId.c_str(), (int)result);
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_LeaveLobbyCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Lobby_UpdateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle) {
    if (!Options || !OutLobbyModificationHandle) return ER::EOS_InvalidParameters;
    *OutLobbyModificationHandle = nullptr;
    if (!Options->LobbyId) return ER::EOS_InvalidParameters;

    LobbyRecord record;
    if (!LobbyDirectory::Get().Get(Options->LobbyId, record)) return ER::EOS_NotFound;

    auto* mod = new EOS_LobbyModificationHandle();
    mod->Magic     = kMagicLobbyModification;
    mod->LobbyId   = Options->LobbyId;
    mod->LocalPuid = PuidString(Options->LocalUserId);
    *OutLobbyModificationHandle = (EOS_HLobbyModification)mod;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Lobby_UpdateLobby(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, const EOS_Lobby_OnUpdateLobbyCallback CompletionDelegate) {
    std::string lobbyId;
    ER result = ER::EOS_InvalidParameters;

    if (Options && Options->LobbyModificationHandle) {
        auto* mod = (EOS_LobbyModificationHandle*)Options->LobbyModificationHandle;
        if (mod->Magic == kMagicLobbyModification) {
            lobbyId = mod->LobbyId;
            LobbyRecord record;
            if (!LobbyDirectory::Get().Get(lobbyId, record)) {
                result = ER::EOS_NotFound;
            } else if (!LobbyDirectory::Get().IsHosting(lobbyId)) {
                // Only the host may rewrite lobby-wide state. Member attributes
                // set by a client are applied to that client's own entry.
                LobbyMember* self = record.FindMember(mod->LocalPuid);
                if (!self) {
                    result = ER::EOS_Lobby_NotOwner;
                } else {
                    for (const auto& a : mod->AddMemberAttributes) {
                        bool replaced = false;
                        for (auto& existing : self->Attributes)
                            if (existing.Key == a.Key) { existing = a; replaced = true; break; }
                        if (!replaced) self->Attributes.push_back(a);
                    }
                    result = ER::EOS_Success;
                }
            } else {
                if (mod->SetBucket)         record.BucketId     = mod->Bucket;
                if (mod->SetPermission)     record.Permission   = mod->Permission;
                if (mod->SetMaxMembers)     record.MaxMembers   = mod->MaxMembers;
                if (mod->SetInvitesAllowed) record.AllowInvites = mod->InvitesAllowed;

                for (const auto& key : mod->RemoveAttributes) {
                    for (size_t i = 0; i < record.Attributes.size(); ) {
                        if (record.Attributes[i].Key == key) record.Attributes.erase(record.Attributes.begin() + i);
                        else ++i;
                    }
                }
                for (const auto& a : mod->AddAttributes) {
                    bool replaced = false;
                    for (auto& existing : record.Attributes)
                        if (existing.Key == a.Key) { existing = a; replaced = true; break; }
                    if (!replaced) record.Attributes.push_back(a);
                }
                if (LobbyMember* self = record.FindMember(mod->LocalPuid)) {
                    for (const auto& key : mod->RemoveMemberAttributes) {
                        for (size_t i = 0; i < self->Attributes.size(); ) {
                            if (self->Attributes[i].Key == key) self->Attributes.erase(self->Attributes.begin() + i);
                            else ++i;
                        }
                    }
                    for (const auto& a : mod->AddMemberAttributes) {
                        bool replaced = false;
                        for (auto& existing : self->Attributes)
                            if (existing.Key == a.Key) { existing = a; replaced = true; break; }
                        if (!replaced) self->Attributes.push_back(a);
                    }
                }
                result = LobbyDirectory::Get().UpdateLobby(record) ? ER::EOS_Success : ER::EOS_NotFound;
                RFLOG(Lobby, "EOS_Lobby_UpdateLobby(%s): bucket='%s' max=%u attributes=%zu -> %d",
                      lobbyId.c_str(), record.BucketId.c_str(), record.MaxMembers,
                      record.Attributes.size(), (int)result);
            }
        }
    }

    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_UpdateLobbyCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.empty() ? nullptr : lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Lobby_PromoteMember(EOS_HLobby Handle, const EOS_Lobby_PromoteMemberOptions* Options, void* ClientData, const EOS_Lobby_OnPromoteMemberCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    std::string target  = Options ? PuidString(Options->TargetUserId) : "";
    ER result = LobbyDirectory::Get().PromoteMember(lobbyId, target) ? ER::EOS_Success : ER::EOS_Lobby_NotOwner;
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_PromoteMemberCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Lobby_KickMember(EOS_HLobby Handle, const EOS_Lobby_KickMemberOptions* Options, void* ClientData, const EOS_Lobby_OnKickMemberCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    std::string target  = Options ? PuidString(Options->TargetUserId) : "";
    ER result = LobbyDirectory::Get().KickMember(lobbyId, target) ? ER::EOS_Success : ER::EOS_Lobby_NotOwner;
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_KickMemberCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Lobby_SendInvite(EOS_HLobby Handle, const EOS_Lobby_SendInviteOptions* Options, void* ClientData, const EOS_Lobby_OnSendInviteCallback CompletionDelegate) {
    std::string lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    std::string target  = Options ? PuidString(Options->TargetUserId) : "";
    ER result = LobbyDirectory::Get().SendInvite(lobbyId, target) ? ER::EOS_Success : ER::EOS_NotFound;
    RFLOG(Lobby, "EOS_Lobby_SendInvite(%s -> %s) -> %d", lobbyId.c_str(), target.c_str(), (int)result);
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, result, lobbyId]() {
        EOS_Lobby_SendInviteCallbackInfo info{};
        info.ResultCode = result;
        info.ClientData = ClientData;
        info.LobbyId    = lobbyId.c_str();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Lobby_CopyLobbyDetailsHandle(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    if (!Options || !OutLobbyDetailsHandle || !Options->LobbyId) return ER::EOS_InvalidParameters;
    *OutLobbyDetailsHandle = nullptr;
    LobbyRecord record;
    if (!LobbyDirectory::Get().Get(Options->LobbyId, record)) return ER::EOS_NotFound;

    auto* details = new EOS_LobbyDetailsHandle();
    details->Magic     = kMagicLobbyDetails;
    details->Record    = std::move(record);
    details->LocalPuid = PuidString(Options->LocalUserId);
    *OutLobbyDetailsHandle = (EOS_HLobbyDetails)details;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Lobby_CreateLobbySearch(EOS_HLobby Handle, const EOS_Lobby_CreateLobbySearchOptions* Options, EOS_HLobbySearch* OutLobbySearchHandle) {
    if (!OutLobbySearchHandle) return ER::EOS_InvalidParameters;
    auto* search = new EOS_LobbySearchHandle();
    search->Magic      = kMagicLobbySearch;
    search->MaxResults = Options && Options->MaxResults ? Options->MaxResults : 50;
    *OutLobbySearchHandle = (EOS_HLobbySearch)search;
    return ER::EOS_Success;
}

// --- notification registration ------------------------------------------
#define REFIX_LOBBY_NOTIFY(FnName, OptionsType, CallbackType, Kind)                              \
    EOS_DECLARE_FUNC(EOS_NotificationId) FnName(EOS_HLobby Handle, const OptionsType* Options,   \
                                                void* ClientData, const CallbackType Handler) {  \
        StartOnline();                                                                           \
        return Dispatcher::Get().Register(NotifyKind::Kind, (void*)Handler, ClientData);         \
    }

REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyUpdateReceived,       EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions,       EOS_Lobby_OnLobbyUpdateReceivedCallback,       LobbyUpdateReceived)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyMemberUpdateReceived, EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions, EOS_Lobby_OnLobbyMemberUpdateReceivedCallback, LobbyMemberUpdateReceived)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyMemberStatusReceived, EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions, EOS_Lobby_OnLobbyMemberStatusReceivedCallback, LobbyMemberStatusReceived)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyInviteReceived,       EOS_Lobby_AddNotifyLobbyInviteReceivedOptions,       EOS_Lobby_OnLobbyInviteReceivedCallback,       LobbyInviteReceived)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyInviteAccepted,       EOS_Lobby_AddNotifyLobbyInviteAcceptedOptions,       EOS_Lobby_OnLobbyInviteAcceptedCallback,       LobbyInviteAccepted)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLobbyInviteRejected,       EOS_Lobby_AddNotifyLobbyInviteRejectedOptions,       EOS_Lobby_OnLobbyInviteRejectedCallback,       LobbyInviteRejected)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyJoinLobbyAccepted,         EOS_Lobby_AddNotifyJoinLobbyAcceptedOptions,         EOS_Lobby_OnJoinLobbyAcceptedCallback,         JoinLobbyAccepted)
REFIX_LOBBY_NOTIFY(EOS_Lobby_AddNotifyLeaveLobbyRequested,       EOS_Lobby_AddNotifyLeaveLobbyRequestedOptions,       EOS_Lobby_OnLeaveLobbyRequestedCallback,       LeaveLobbyRequested)

#define REFIX_LOBBY_UNNOTIFY(FnName)                                              \
    EOS_DECLARE_FUNC(void) FnName(EOS_HLobby Handle, EOS_NotificationId InId) {   \
        Dispatcher::Get().Unregister(InId);                                       \
    }

REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyUpdateReceived)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyInviteReceived)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyInviteAccepted)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLobbyInviteRejected)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyJoinLobbyAccepted)
REFIX_LOBBY_UNNOTIFY(EOS_Lobby_RemoveNotifyLeaveLobbyRequested)

// ===========================================================================
// EOS_LobbyModification_*
// ===========================================================================
namespace {
EOS_LobbyModificationHandle* AsModification(EOS_HLobbyModification h) {
    auto* mod = (EOS_LobbyModificationHandle*)h;
    return (mod && mod->Magic == kMagicLobbyModification) ? mod : nullptr;
}
} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_SetBucketId(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || !Options->BucketId) return ER::EOS_InvalidParameters;
    mod->SetBucket = true;
    mod->Bucket    = Options->BucketId;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_SetPermissionLevel(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options) return ER::EOS_InvalidParameters;
    mod->SetPermission = true;
    mod->Permission    = Options->PermissionLevel;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_SetMaxMembers(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || Options->MaxMembers == 0) return ER::EOS_InvalidParameters;
    mod->SetMaxMembers = true;
    mod->MaxMembers    = Options->MaxMembers;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_SetInvitesAllowed(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetInvitesAllowedOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options) return ER::EOS_InvalidParameters;
    mod->SetInvitesAllowed = true;
    mod->InvitesAllowed    = Options->bInvitesAllowed != EOS_FALSE;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_AddAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddAttributeOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || !Options->Attribute || !Options->Attribute->Key) return ER::EOS_InvalidParameters;
    Attribute a;
    a.Key        = Options->Attribute->Key;
    a.Value      = FromSdk(*Options->Attribute);
    a.Visibility = Options->Visibility;
    mod->AddAttributes.push_back(std::move(a));
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_RemoveAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_RemoveAttributeOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || !Options->Key) return ER::EOS_InvalidParameters;
    mod->RemoveAttributes.push_back(Options->Key);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_AddMemberAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddMemberAttributeOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || !Options->Attribute || !Options->Attribute->Key) return ER::EOS_InvalidParameters;
    Attribute a;
    a.Key        = Options->Attribute->Key;
    a.Value      = FromSdk(*Options->Attribute);
    a.Visibility = Options->Visibility;
    mod->AddMemberAttributes.push_back(std::move(a));
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyModification_RemoveMemberAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_RemoveMemberAttributeOptions* Options) {
    auto* mod = AsModification(Handle);
    if (!mod || !Options || !Options->Key) return ER::EOS_InvalidParameters;
    mod->RemoveMemberAttributes.push_back(Options->Key);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_LobbyModification_Release(EOS_HLobbyModification LobbyModificationHandle) {
    auto* mod = AsModification(LobbyModificationHandle);
    if (!mod) return;
    mod->Magic = 0;
    delete mod;
}

// ===========================================================================
// EOS_LobbyDetails_*
// ===========================================================================
namespace {
EOS_LobbyDetailsHandle* AsDetails(EOS_HLobbyDetails h) {
    auto* d = (EOS_LobbyDetailsHandle*)h;
    return (d && d->Magic == kMagicLobbyDetails) ? d : nullptr;
}
} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyInfo(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyInfoOptions* Options, EOS_LobbyDetails_Info** OutLobbyDetailsInfo) {
    auto* d = AsDetails(Handle);
    if (!d || !OutLobbyDetailsInfo) return ER::EOS_InvalidParameters;
    *OutLobbyDetailsInfo = nullptr;

    auto* block = new InfoBlock();
    block->LobbyId  = d->Record.LobbyId;
    block->BucketId = d->Record.BucketId;
    std::memset(&block->Info, 0, sizeof(block->Info));
    block->Info.ApiVersion       = EOS_LOBBYDETAILS_INFO_API_LATEST;
    block->Info.LobbyId          = block->LobbyId.c_str();
    block->Info.LobbyOwnerUserId = IdRegistry::Get().Puid(d->Record.OwnerPuid);
    block->Info.PermissionLevel  = d->Record.Permission;
    block->Info.AvailableSlots   = d->Record.AvailableSlots();
    block->Info.MaxMembers       = d->Record.MaxMembers;
    block->Info.bAllowInvites    = d->Record.AllowInvites ? EOS_TRUE : EOS_FALSE;
    block->Info.BucketId         = block->BucketId.c_str();
    block->Info.bAllowHostMigration = d->Record.AllowHostMigration ? EOS_TRUE : EOS_FALSE;
    block->Info.bRTCRoomEnabled     = d->Record.RtcRoomEnabled ? EOS_TRUE : EOS_FALSE;
    block->Info.bAllowJoinById      = d->Record.AllowJoinById ? EOS_TRUE : EOS_FALSE;
    block->Info.bRejoinAfterKickRequiresInvite = d->Record.RejoinAfterKickRequiresInvite ? EOS_TRUE : EOS_FALSE;
    block->Info.bPresenceEnabled    = d->Record.PresenceEnabled ? EOS_TRUE : EOS_FALSE;
    block->Info.AllowedPlatformIds  = nullptr;   // unrestricted
    block->Info.AllowedPlatformIdsCount = 0;

    {
        std::lock_guard<std::mutex> lock(g_infoMutex);
        g_infoBlocks[&block->Info] = block;
    }
    *OutLobbyDetailsInfo = &block->Info;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_LobbyDetails_Info_Release(EOS_LobbyDetails_Info* LobbyDetailsInfo) {
    if (!LobbyDetailsInfo) return;
    InfoBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_infoMutex);
        auto it = g_infoBlocks.find(LobbyDetailsInfo);
        if (it == g_infoBlocks.end()) return;
        block = it->second;
        g_infoBlocks.erase(it);
    }
    delete block;
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_LobbyDetails_GetLobbyOwner(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetLobbyOwnerOptions* Options) {
    auto* d = AsDetails(Handle);
    if (!d) return nullptr;
    return IdRegistry::Get().Puid(d->Record.OwnerPuid);
}

EOS_DECLARE_FUNC(uint32_t) EOS_LobbyDetails_GetMemberCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberCountOptions* Options) {
    auto* d = AsDetails(Handle);
    return d ? (uint32_t)d->Record.Members.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_LobbyDetails_GetMemberByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberByIndexOptions* Options) {
    auto* d = AsDetails(Handle);
    if (!d || !Options) return nullptr;
    if (Options->MemberIndex >= d->Record.Members.size()) return nullptr;
    return IdRegistry::Get().Puid(d->Record.Members[Options->MemberIndex].Puid);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyMemberInfo(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyMemberInfoOptions* Options, EOS_LobbyDetails_MemberInfo** OutMemberInfo) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutMemberInfo) return ER::EOS_InvalidParameters;
    *OutMemberInfo = nullptr;
    std::string puid = PuidString(Options->TargetUserId);
    const LobbyMember* member = d->Record.FindMember(puid);
    if (!member) return ER::EOS_NotFound;

    auto* info = new EOS_LobbyDetails_MemberInfo();
    std::memset(info, 0, sizeof(*info));
    info->ApiVersion       = EOS_LOBBYDETAILS_MEMBERINFO_API_LATEST;
    info->UserId           = IdRegistry::Get().Puid(member->Puid);
    info->Platform         = 0;                 // unspecified platform
    info->bAllowsCrossplay = EOS_TRUE;
    {
        std::lock_guard<std::mutex> lock(g_memberInfoMutex);
        g_memberInfoBlocks[info] = info;
    }
    *OutMemberInfo = info;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_LobbyDetails_MemberInfo_Release(EOS_LobbyDetails_MemberInfo* MemberInfo) {
    if (!MemberInfo) return;
    std::lock_guard<std::mutex> lock(g_memberInfoMutex);
    auto it = g_memberInfoBlocks.find(MemberInfo);
    if (it == g_memberInfoBlocks.end()) return;
    g_memberInfoBlocks.erase(it);
    delete MemberInfo;
}

EOS_DECLARE_FUNC(uint32_t) EOS_LobbyDetails_GetAttributeCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetAttributeCountOptions* Options) {
    auto* d = AsDetails(Handle);
    return d ? (uint32_t)d->Record.Attributes.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyAttributeByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutAttribute) return ER::EOS_InvalidParameters;
    *OutAttribute = nullptr;
    if (Options->AttrIndex >= d->Record.Attributes.size()) return ER::EOS_NotFound;
    *OutAttribute = MakeAttribute(d->Record.Attributes[Options->AttrIndex]);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyAttributeByKey(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutAttribute || !Options->AttrKey) return ER::EOS_InvalidParameters;
    *OutAttribute = nullptr;
    const Attribute* attr = d->Record.FindAttribute(Options->AttrKey);
    if (!attr) return ER::EOS_NotFound;
    *OutAttribute = MakeAttribute(*attr);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(uint32_t) EOS_LobbyDetails_GetMemberAttributeCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberAttributeCountOptions* Options) {
    auto* d = AsDetails(Handle);
    if (!d || !Options) return 0;
    const LobbyMember* member = d->Record.FindMember(PuidString(Options->TargetUserId));
    return member ? (uint32_t)member->Attributes.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyMemberAttributeByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyMemberAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutAttribute) return ER::EOS_InvalidParameters;
    *OutAttribute = nullptr;
    const LobbyMember* member = d->Record.FindMember(PuidString(Options->TargetUserId));
    if (!member || Options->AttrIndex >= member->Attributes.size()) return ER::EOS_NotFound;
    *OutAttribute = MakeAttribute(member->Attributes[Options->AttrIndex]);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbyDetails_CopyMemberAttributeByKey(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyMemberAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    auto* d = AsDetails(Handle);
    if (!d || !Options || !OutAttribute || !Options->AttrKey) return ER::EOS_InvalidParameters;
    *OutAttribute = nullptr;
    const LobbyMember* member = d->Record.FindMember(PuidString(Options->TargetUserId));
    if (!member) return ER::EOS_NotFound;
    for (const auto& a : member->Attributes)
        if (a.Key == Options->AttrKey) { *OutAttribute = MakeAttribute(a); return ER::EOS_Success; }
    return ER::EOS_NotFound;
}

EOS_DECLARE_FUNC(void) EOS_LobbyDetails_Release(EOS_HLobbyDetails LobbyHandle) {
    auto* d = AsDetails(LobbyHandle);
    if (!d) return;
    d->Magic = 0;
    delete d;
}

EOS_DECLARE_FUNC(void) EOS_Lobby_Attribute_Release(EOS_Lobby_Attribute* LobbyAttribute) {
    if (!LobbyAttribute) return;
    AttributeBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_attrMutex);
        auto it = g_attrBlocks.find(LobbyAttribute);
        if (it == g_attrBlocks.end()) return;
        block = it->second;
        g_attrBlocks.erase(it);
    }
    delete block;
}

// ===========================================================================
// EOS_LobbySearch_*
// ===========================================================================
namespace {
EOS_LobbySearchHandle* AsSearch(EOS_HLobbySearch h) {
    auto* s = (EOS_LobbySearchHandle*)h;
    return (s && s->Magic == kMagicLobbySearch) ? s : nullptr;
}
} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_SetLobbyId(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetLobbyIdOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->LobbyId) return ER::EOS_InvalidParameters;
    s->LobbyIdFilter = Options->LobbyId;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_SetTargetUserId(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetTargetUserIdOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options) return ER::EOS_InvalidParameters;
    s->TargetUserFilter = PuidString(Options->TargetUserId);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_SetParameter(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetParameterOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->Parameter || !Options->Parameter->Key) return ER::EOS_InvalidParameters;
    SearchParameter p;
    p.Key   = Options->Parameter->Key;
    p.Value = FromSdk(*Options->Parameter);
    p.Op    = Options->ComparisonOp;
    s->Parameters.push_back(std::move(p));
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_RemoveParameter(EOS_HLobbySearch Handle, const EOS_LobbySearch_RemoveParameterOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !Options->Key) return ER::EOS_InvalidParameters;
    for (size_t i = 0; i < s->Parameters.size(); ) {
        if (s->Parameters[i].Key == Options->Key && s->Parameters[i].Op == Options->ComparisonOp)
            s->Parameters.erase(s->Parameters.begin() + i);
        else ++i;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_SetMaxResults(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetMaxResultsOptions* Options) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || Options->MaxResults == 0) return ER::EOS_InvalidParameters;
    s->MaxResults = Options->MaxResults;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_LobbySearch_Find(EOS_HLobbySearch Handle, const EOS_LobbySearch_FindOptions* Options, void* ClientData, const EOS_LobbySearch_OnFindCallback CompletionDelegate) {
    auto* s = AsSearch(Handle);
    if (!s) {
        if (CompletionDelegate)
            Dispatcher::Get().PostGenericCompletion((void*)CompletionDelegate, ClientData, ER::EOS_InvalidParameters);
        return;
    }
    if (!StartOnline()) {
        if (CompletionDelegate)
            Dispatcher::Get().PostGenericCompletion((void*)CompletionDelegate, ClientData, ER::EOS_NoConnection);
        return;
    }

    s->LocalPuid = Options ? PuidString(Options->LocalUserId) : "";
    s->Results.clear();
    LobbyDirectory::Get().RequestSearch();

    // Give hosts a moment to answer before reporting what we found. This is the
    // one place a delay is correct rather than a workaround: a search that
    // returns before any reply can arrive would always find nothing.
    const uint32_t waitMs = (uint32_t)Config::Get().GetInt("Network", "SearchWaitMs", 900);
    Dispatcher::Get().PostAfter(waitMs, [s, ClientData, CompletionDelegate]() {
        if (s->Magic != kMagicLobbySearch) return;
        std::vector<LobbyRecord> all = LobbyDirectory::Get().Snapshot();
        for (const auto& record : all) {
            if (s->Results.size() >= s->MaxResults) break;
            if (!s->LobbyIdFilter.empty() && record.LobbyId != s->LobbyIdFilter) continue;
            if (!s->TargetUserFilter.empty() && !record.FindMember(s->TargetUserFilter)) continue;
            if (record.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY) continue;
            bool matches = true;
            for (const auto& p : s->Parameters) {
                if (!p.Matches(record)) { matches = false; break; }
            }
            if (matches) s->Results.push_back(record);
        }
        RFLOG(Lobby, "EOS_LobbySearch_Find: %zu of %zu advertised lobbies matched %zu filter(s)",
              s->Results.size(), all.size(), s->Parameters.size());

        if (!CompletionDelegate) return;
        EOS_LobbySearch_FindCallbackInfo info{};
        info.ResultCode = ER::EOS_Success;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(uint32_t) EOS_LobbySearch_GetSearchResultCount(EOS_HLobbySearch Handle, const EOS_LobbySearch_GetSearchResultCountOptions* Options) {
    auto* s = AsSearch(Handle);
    return s ? (uint32_t)s->Results.size() : 0u;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_LobbySearch_CopySearchResultByIndex(EOS_HLobbySearch Handle, const EOS_LobbySearch_CopySearchResultByIndexOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    auto* s = AsSearch(Handle);
    if (!s || !Options || !OutLobbyDetailsHandle) return ER::EOS_InvalidParameters;
    *OutLobbyDetailsHandle = nullptr;
    if (Options->LobbyIndex >= s->Results.size()) return ER::EOS_NotFound;

    auto* details = new EOS_LobbyDetailsHandle();
    details->Magic     = kMagicLobbyDetails;
    details->Record    = s->Results[Options->LobbyIndex];
    details->LocalPuid = s->LocalPuid;
    *OutLobbyDetailsHandle = (EOS_HLobbyDetails)details;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_LobbySearch_Release(EOS_HLobbySearch LobbySearchHandle) {
    auto* s = AsSearch(LobbySearchHandle);
    if (!s) return;
    s->Magic = 0;
    delete s;
}

namespace refix {

void RegisterLobbyApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_Lobby_CreateLobby);
    REFIX_BIND(reg, EOS_Lobby_DestroyLobby);
    REFIX_BIND(reg, EOS_Lobby_JoinLobby);
    REFIX_BIND(reg, EOS_Lobby_JoinLobbyById);
    REFIX_BIND(reg, EOS_Lobby_LeaveLobby);
    REFIX_BIND(reg, EOS_Lobby_UpdateLobbyModification);
    REFIX_BIND(reg, EOS_Lobby_UpdateLobby);
    REFIX_BIND(reg, EOS_Lobby_PromoteMember);
    REFIX_BIND(reg, EOS_Lobby_KickMember);
    REFIX_BIND(reg, EOS_Lobby_SendInvite);
    REFIX_BIND(reg, EOS_Lobby_CopyLobbyDetailsHandle);
    REFIX_BIND(reg, EOS_Lobby_CreateLobbySearch);

    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyUpdateReceived);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyMemberUpdateReceived);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyMemberStatusReceived);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyInviteReceived);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyInviteAccepted);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLobbyInviteRejected);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyJoinLobbyAccepted);
    REFIX_BIND(reg, EOS_Lobby_AddNotifyLeaveLobbyRequested);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyUpdateReceived);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyInviteReceived);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyInviteAccepted);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLobbyInviteRejected);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyJoinLobbyAccepted);
    REFIX_BIND(reg, EOS_Lobby_RemoveNotifyLeaveLobbyRequested);

    REFIX_BIND(reg, EOS_LobbyModification_SetBucketId);
    REFIX_BIND(reg, EOS_LobbyModification_SetPermissionLevel);
    REFIX_BIND(reg, EOS_LobbyModification_SetMaxMembers);
    REFIX_BIND(reg, EOS_LobbyModification_SetInvitesAllowed);
    REFIX_BIND(reg, EOS_LobbyModification_AddAttribute);
    REFIX_BIND(reg, EOS_LobbyModification_RemoveAttribute);
    REFIX_BIND(reg, EOS_LobbyModification_AddMemberAttribute);
    REFIX_BIND(reg, EOS_LobbyModification_RemoveMemberAttribute);
    REFIX_BIND(reg, EOS_LobbyModification_Release);

    REFIX_BIND(reg, EOS_LobbyDetails_CopyInfo);
    REFIX_BIND(reg, EOS_LobbyDetails_Info_Release);
    REFIX_BIND(reg, EOS_LobbyDetails_GetLobbyOwner);
    REFIX_BIND(reg, EOS_LobbyDetails_GetMemberCount);
    REFIX_BIND(reg, EOS_LobbyDetails_GetMemberByIndex);
    REFIX_BIND(reg, EOS_LobbyDetails_CopyMemberInfo);
    REFIX_BIND(reg, EOS_LobbyDetails_MemberInfo_Release);
    REFIX_BIND(reg, EOS_LobbyDetails_GetAttributeCount);
    REFIX_BIND(reg, EOS_LobbyDetails_CopyAttributeByIndex);
    REFIX_BIND(reg, EOS_LobbyDetails_CopyAttributeByKey);
    REFIX_BIND(reg, EOS_LobbyDetails_GetMemberAttributeCount);
    REFIX_BIND(reg, EOS_LobbyDetails_CopyMemberAttributeByIndex);
    REFIX_BIND(reg, EOS_LobbyDetails_CopyMemberAttributeByKey);
    REFIX_BIND(reg, EOS_LobbyDetails_Release);
    REFIX_BIND(reg, EOS_Lobby_Attribute_Release);

    REFIX_BIND(reg, EOS_LobbySearch_SetLobbyId);
    REFIX_BIND(reg, EOS_LobbySearch_SetTargetUserId);
    REFIX_BIND(reg, EOS_LobbySearch_SetParameter);
    REFIX_BIND(reg, EOS_LobbySearch_RemoveParameter);
    REFIX_BIND(reg, EOS_LobbySearch_SetMaxResults);
    REFIX_BIND(reg, EOS_LobbySearch_Find);
    REFIX_BIND(reg, EOS_LobbySearch_GetSearchResultCount);
    REFIX_BIND(reg, EOS_LobbySearch_CopySearchResultByIndex);
    REFIX_BIND(reg, EOS_LobbySearch_Release);
}

} // namespace refix
