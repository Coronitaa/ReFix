// =============================================================================
// ReFix EOS Online v2 - EOS Lobby Creation, Search, Join & Management Layer
// =============================================================================
#include "eos_lobby.h"
#include "eos_connect.h"
#include <windows.h>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace ReFixEOS {

std::string NormalizeAttributeValue(int32_t valueType, const ::EOS_Lobby_AttributeDataValue& val) {
    switch (valueType) {
    case EOS_AT_BOOLEAN:
        return val.AsBool ? "b:1" : "b:0";
    case EOS_AT_INT64:
        return "i:" + std::to_string(val.AsInt64);
    case EOS_AT_DOUBLE: {
        std::ostringstream ss;
        ss << "d:" << std::setprecision(8) << val.AsDouble;
        return ss.str();
    }
    case EOS_AT_STRING:
    default:
        return val.AsUtf8 ? ("s:" + std::string(val.AsUtf8)) : "s:";
    }
}

// In-Memory LobbyModification Model
static constexpr uint32_t LMOD_MAGIC = 0x4C4D4F44; // 'LMOD'
struct OpaqueLobbyModification {
    uint32_t magic = LMOD_MAGIC;
    std::string lobbyId;
    EOS_ProductUserId localUserId = nullptr;
    uint32_t maxMembers = 4;
    EOS_ELobbyPermissionLevel permissionLevel = EOS_LPL_PUBLICADVERTISED;
    std::string bucketId;
    bool invitesAllowed = true;
    std::unordered_map<std::string, std::string> attributes;
};

// In-Memory LobbyDetails Model
static constexpr uint32_t LDET_MAGIC = 0x4C444554; // 'LDET'
struct OpaqueLobbyDetails {
    uint32_t magic = LDET_MAGIC;
    ReFixOnline::LobbyData data;
    std::vector<std::pair<std::string, std::string>> attributeList;
    EOS_ProductUserId ownerPuid = nullptr;
    std::vector<EOS_ProductUserId> memberPuids;

    void Populate(const ReFixOnline::LobbyData& lob) {
        data = lob;
        attributeList.clear();
        for (const auto& kv : data.attributes) {
            attributeList.push_back(kv);
        }
        ownerPuid = IdentityManager::Get().ProductUserIdFromString(data.ownerUserId.c_str());
        memberPuids.clear();
        for (const auto& m : data.members) {
            EOS_ProductUserId puid = IdentityManager::Get().ProductUserIdFromString(m.userId.c_str());
            memberPuids.push_back(puid);
        }
    }
};

// In-Memory LobbySearch Model
static constexpr uint32_t LSRC_MAGIC = 0x4C535243; // 'LSRC'
struct OpaqueLobbySearch {
    uint32_t magic = LSRC_MAGIC;
    uint32_t maxResults = 50;
    std::unordered_map<std::string, std::string> filters;
    std::vector<ReFixOnline::LobbyData> searchResults;
};

// Callback Closures
struct CreateLobbyClosure {
    void* completionDelegate;
    void* clientData;
    char* lobbyId;
    EOS_EResult resultCode;
};

static void ForwardCreateLobbyCallback(const void* data) {
    const auto* closure = (const CreateLobbyClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;
        cbInfo.LobbyId = closure->lobbyId;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush CreateLobby callback (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE CALLBACK DISPATCHED (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");

        typedef void (*EOS_Lobby_OnCreateLobbyCallback)(const EOS_Lobby_CreateLobbyCallbackInfo* Data);
        auto fn = (EOS_Lobby_OnCreateLobbyCallback)closure->completionDelegate;
        fn(&cbInfo);

        if (closure->lobbyId) free((void*)closure->lobbyId);
    }
}

struct UpdateLobbyClosure {
    void* completionDelegate;
    void* clientData;
    char* lobbyId;
    EOS_EResult resultCode;
};

static void ForwardUpdateLobbyCallback(const void* data) {
    const auto* closure = (const UpdateLobbyClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_Lobby_UpdateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;
        cbInfo.LobbyId = closure->lobbyId;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush UpdateLobby callback (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] UPDATE LOBBY CALLBACK DISPATCHED (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");

        typedef void (*EOS_Lobby_OnUpdateLobbyCallback)(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);
        auto fn = (EOS_Lobby_OnUpdateLobbyCallback)closure->completionDelegate;
        fn(&cbInfo);

        if (closure->lobbyId) free((void*)closure->lobbyId);
    }
}

struct DestroyLobbyClosure {
    void* completionDelegate;
    void* clientData;
    char* lobbyId;
    EOS_EResult resultCode;
};

static void ForwardDestroyLobbyCallback(const void* data) {
    const auto* closure = (const DestroyLobbyClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_Lobby_DestroyLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;
        cbInfo.LobbyId = closure->lobbyId;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush DestroyLobby callback (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] DESTROY LOBBY CALLBACK DISPATCHED (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");

        typedef void (*EOS_Lobby_OnDestroyLobbyCallback)(const EOS_Lobby_DestroyLobbyCallbackInfo* Data);
        auto fn = (EOS_Lobby_OnDestroyLobbyCallback)closure->completionDelegate;
        fn(&cbInfo);

        if (closure->lobbyId) free((void*)closure->lobbyId);
    }
}

struct JoinLobbyClosure {
    void* completionDelegate;
    void* clientData;
    char* lobbyId;
    EOS_EResult resultCode;
};

static void ForwardJoinLobbyCallback(const void* data) {
    const auto* closure = (const JoinLobbyClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_Lobby_JoinLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;
        cbInfo.LobbyId = closure->lobbyId;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush JoinLobby callback (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] JOIN LOBBY CALLBACK DISPATCHED (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");

        typedef void (*EOS_Lobby_OnJoinLobbyCallback)(const EOS_Lobby_JoinLobbyCallbackInfo* Data);
        auto fn = (EOS_Lobby_OnJoinLobbyCallback)closure->completionDelegate;
        fn(&cbInfo);

        if (closure->lobbyId) free((void*)closure->lobbyId);
    }
}

struct LeaveLobbyClosure {
    void* completionDelegate;
    void* clientData;
    char* lobbyId;
    EOS_EResult resultCode;
};

static void ForwardLeaveLobbyCallback(const void* data) {
    const auto* closure = (const LeaveLobbyClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_Lobby_LeaveLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;
        cbInfo.LobbyId = closure->lobbyId;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush LeaveLobby callback (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] LEAVE LOBBY CALLBACK DISPATCHED (Result=%d, LobbyId=%s)",
            (int)cbInfo.ResultCode, cbInfo.LobbyId ? cbInfo.LobbyId : "null");

        typedef void (*EOS_Lobby_OnLeaveLobbyCallback)(const EOS_Lobby_LeaveLobbyCallbackInfo* Data);
        auto fn = (EOS_Lobby_OnLeaveLobbyCallback)closure->completionDelegate;
        fn(&cbInfo);

        if (closure->lobbyId) free((void*)closure->lobbyId);
    }
}

struct FindLobbiesClosure {
    void* completionDelegate;
    void* clientData;
    EOS_EResult resultCode;
};

static void ForwardFindLobbiesCallback(const void* data) {
    const auto* closure = (const FindLobbiesClosure*)data;
    if (closure && closure->completionDelegate) {
        EOS_LobbySearch_FindCallbackInfo cbInfo = {};
        cbInfo.ResultCode = closure->resultCode;
        cbInfo.ClientData = closure->clientData;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Flush FindLobbies callback (Result=%d)", (int)cbInfo.ResultCode);
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] FIND LOBBIES CALLBACK DISPATCHED (Result=%d)", (int)cbInfo.ResultCode);

        typedef void (*EOS_LobbySearch_OnFindCallback)(const EOS_LobbySearch_FindCallbackInfo* Data);
        auto fn = (EOS_LobbySearch_OnFindCallback)closure->completionDelegate;
        fn(&cbInfo);
    }
}

} // namespace ReFixEOS

extern "C" {

// =============================================================================
// Lobby Lifecycle
// =============================================================================

void EOS_Lobby_CreateLobby(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_CreateLobby ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] LocalUserId=%p", Options ? Options->LocalUserId : nullptr);
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] MaxLobbyMembers=%u", Options ? Options->MaxLobbyMembers : 0);
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] PermissionLevel=%d", Options ? (int)Options->PermissionLevel : 0);
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] PresenceEnabled=%d", Options ? (int)Options->bPresenceEnabled : 0);
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] BucketId=%s", (Options && Options->BucketId) ? Options->BucketId : "null");

    if (!CompletionDelegate) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: CompletionDelegate is NULL, aborting");
        return;
    }

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();

    // 1. Parameter Validation
    if (!Options || Options->ApiVersion <= 0) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Invalid parameters (Options=%p)", Options);
        ReFixEOS::CreateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE CALLBACK QUEUED");
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardCreateLobbyCallback, closure);
        return;
    }

    // 2. Validate LocalUserId (Must be valid registered opaque handle)
    if (!Options->LocalUserId || !idMgr.IsValidProductUserId(Options->LocalUserId)) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Invalid LocalUserId handle (%p)", Options->LocalUserId);
        ReFixEOS::CreateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidUser;
        closure.lobbyId = nullptr;
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE CALLBACK QUEUED");
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardCreateLobbyCallback, closure);
        return;
    }

    // 3. Validate Capacity
    uint32_t maxMembers = Options->MaxLobbyMembers;
    if (maxMembers == 0 || maxMembers > ReFixOnline::MAX_LOBBY_MEMBERS) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Invalid MaxLobbyMembers (%u) -> EOS_InvalidParameters", maxMembers);
        ReFixEOS::CreateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE CALLBACK QUEUED");
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardCreateLobbyCallback, closure);
        return;
    }

    // 4. Validate BucketId if present
    if (Options->BucketId && strlen(Options->BucketId) > ReFixOnline::MAX_ATTRIBUTE_VAL_LEN) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: BucketId length exceeded limit");
        ReFixEOS::CreateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE CALLBACK QUEUED");
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardCreateLobbyCallback, closure);
        return;
    }

    std::string puidStr = idMgr.ProductUserIdToString(Options->LocalUserId);
    ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Requested by PUID=%s, MaxMembers=%u",
        puidStr.c_str(), maxMembers);

    // 5. Prepare Attributes map
    std::unordered_map<std::string, std::string> attributes;
    if (Options->BucketId) {
        attributes["bucket_id"] = "s:" + std::string(Options->BucketId);
    }
    attributes["permission_level"] = "i:" + std::to_string(Options->PermissionLevel);
    attributes["presence_enabled"] = Options->bPresenceEnabled ? "b:1" : "b:0";
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] AttributeCount=%u", (uint32_t)attributes.size());

    // 6. Asynchronous Request via RoomManagerBridge -> BackendClient -> Protocol -> Transport
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] RoomManager CreateLobby ENTER");
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] PUID=%s", puidStr.c_str());
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] MaxMembers=%u", maxMembers);

    roomBridge.CreateLobby(maxMembers, attributes, [CompletionDelegate, ClientData](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        ReFixEOS::LogDiagnostic("[RFIX_BACKEND] BackendClient Callback ENTER Result=%d LobbyId=%s", (int)res, lob.lobbyId.c_str());

        ReFixEOS::CreateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;

        if (res == ReFixOnline::SUCCESS) {
            ReFixEOS::LogDiagnostic("[EOS_RUNTIME] BACKEND CREATE SUCCESS LobbyId=%s", lob.lobbyId.c_str());
            closure.resultCode = EOS_Success;
            closure.lobbyId = _strdup(lob.lobbyId.c_str());
        } else if (res == ReFixOnline::NOT_AUTHENTICATED) {
            ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE FAILED Result=NOT_AUTHENTICATED -> EOS_NoConnection");
            closure.resultCode = EOS_NoConnection;
            closure.lobbyId = nullptr;
        } else {
            ReFixEOS::LogDiagnostic("[EOS_RUNTIME] CREATE FAILED Result=%d -> EOS_LimitExceeded", (int)res);
            closure.resultCode = EOS_LimitExceeded;
            closure.lobbyId = nullptr;
        }

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue CreateLobby callback");
        ReFixEOS::CallbackManager::Get().QueueCallback((void*)ReFixEOS::ForwardCreateLobbyCallback, closure);
    });
}

void EOS_Lobby_UpdateLobby(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_UpdateLobby ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);

    if (!CompletionDelegate) return;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();

    if (!Options || !Options->LobbyModificationHandle) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_UpdateLobby: Invalid Options or LobbyModificationHandle");
        ReFixEOS::UpdateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardUpdateLobbyCallback, closure);
        return;
    }

    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Options->LobbyModificationHandle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_UpdateLobby: Invalid LobbyModificationHandle magic");
        ReFixEOS::UpdateLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardUpdateLobbyCallback, closure);
        return;
    }

    ReFixEOS::LogDiagnostic("EOS_Lobby_UpdateLobby: Updating lobbyId='%s' with %u attributes",
        mod->lobbyId.c_str(), (uint32_t)mod->attributes.size());

    std::string puidStr = mod->localUserId ? ReFixEOS::IdentityManager::Get().ProductUserIdToString(mod->localUserId) : ReFixEOS::IdentityManager::Get().GetLocalProductUserIdString();
    auto& backend = ReFixOnline::BackendServerState::Get();
    ReFixOnline::LobbyData updatedLobby;
    ReFixOnline::EBackendResult res = backend.UpdateLobby(puidStr, mod->lobbyId, mod->attributes, mod->maxMembers, mod->bucketId, (int32_t)mod->permissionLevel, mod->invitesAllowed, updatedLobby);

    ReFixEOS::UpdateLobbyClosure closure = {};
    closure.completionDelegate = CompletionDelegate;
    closure.clientData = ClientData;
    closure.resultCode = (res == ReFixOnline::SUCCESS) ? EOS_Success : EOS_NotFound;
    closure.lobbyId = _strdup(mod->lobbyId.c_str());

    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue UpdateLobby callback");
    cbMgr.QueueCallback((void*)ReFixEOS::ForwardUpdateLobbyCallback, closure);
}

void EOS_Lobby_DestroyLobby(EOS_HLobby Handle, const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_DestroyLobby ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);

    if (!CompletionDelegate) return;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    if (!Options || !Options->LobbyId) {
        ReFixEOS::DestroyLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardDestroyLobbyCallback, closure);
        return;
    }

    std::string lobbyId = Options->LobbyId;
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.DestroyLobby(lobbyId, [CompletionDelegate, ClientData, lobbyId](ReFixOnline::EBackendResult res, const std::string& id) {
        ReFixEOS::DestroyLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = (res == ReFixOnline::SUCCESS) ? EOS_Success : EOS_NotFound;
        closure.lobbyId = _strdup(lobbyId.c_str());

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue DestroyLobby callback");
        ReFixEOS::CallbackManager::Get().QueueCallback((void*)ReFixEOS::ForwardDestroyLobbyCallback, closure);
    });
}

void EOS_Lobby_JoinLobby(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_JoinLobby ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);

    if (!CompletionDelegate) return;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    if (!Options || !Options->LobbyDetailsHandle) {
        ReFixEOS::JoinLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardJoinLobbyCallback, closure);
        return;
    }

    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Options->LobbyDetailsHandle;
    if (details->magic != ReFixEOS::LDET_MAGIC) {
        ReFixEOS::JoinLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardJoinLobbyCallback, closure);
        return;
    }

    std::string lobbyId = details->data.lobbyId;
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.JoinLobby(lobbyId, [CompletionDelegate, ClientData, lobbyId](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        ReFixEOS::JoinLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;

        if (res == ReFixOnline::SUCCESS || res == ReFixOnline::ALREADY_MEMBER) {
            closure.resultCode = EOS_Success;
            closure.lobbyId = _strdup(lobbyId.c_str());
        } else if (res == ReFixOnline::LOBBY_FULL) {
            closure.resultCode = EOS_LimitExceeded;
            closure.lobbyId = nullptr;
        } else {
            closure.resultCode = EOS_NotFound;
            closure.lobbyId = nullptr;
        }

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue JoinLobby callback");
        ReFixEOS::CallbackManager::Get().QueueCallback((void*)ReFixEOS::ForwardJoinLobbyCallback, closure);
    });
}

void EOS_Lobby_LeaveLobby(EOS_HLobby Handle, const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_LeaveLobby ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);

    if (!CompletionDelegate) return;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    if (!Options || !Options->LobbyId) {
        ReFixEOS::LeaveLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        closure.lobbyId = nullptr;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardLeaveLobbyCallback, closure);
        return;
    }

    std::string lobbyId = Options->LobbyId;
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.LeaveLobby(lobbyId, [CompletionDelegate, ClientData, lobbyId](ReFixOnline::EBackendResult res, const std::string& id) {
        ReFixEOS::LeaveLobbyClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = (res == ReFixOnline::SUCCESS) ? EOS_Success : EOS_NotFound;
        closure.lobbyId = _strdup(lobbyId.c_str());

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue LeaveLobby callback");
        ReFixEOS::CallbackManager::Get().QueueCallback((void*)ReFixEOS::ForwardLeaveLobbyCallback, closure);
    });
}

// =============================================================================
// Lobby Modification
// =============================================================================

EOS_EResult EOS_Lobby_CreateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_CreateLobbyModification called (TID=0x%lx, Handle=%p, Options=%p, LocalUserId=%p, LobbyId=%s)",
        tid, Handle, Options, Options ? Options->LocalUserId : nullptr, (Options && Options->LobbyId) ? Options->LobbyId : "null");
    if (!OutLobbyModificationHandle) return EOS_InvalidParameters;

    auto* mod = new ReFixEOS::OpaqueLobbyModification();
    mod->magic = ReFixEOS::LMOD_MAGIC;
    mod->lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    mod->localUserId = Options ? Options->LocalUserId : nullptr;
    mod->maxMembers = 4;
    mod->permissionLevel = EOS_LPL_PUBLICADVERTISED;
    mod->invitesAllowed = true;

    *OutLobbyModificationHandle = (EOS_HLobbyModification)mod;
    return EOS_Success;
}

EOS_EResult EOS_Lobby_UpdateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_UpdateLobbyModification called (TID=0x%lx, Handle=%p, Options=%p, LobbyId=%s)",
        tid, Handle, Options, (Options && Options->LobbyId) ? Options->LobbyId : "null");
    if (!OutLobbyModificationHandle) return EOS_InvalidParameters;

    auto* mod = new ReFixEOS::OpaqueLobbyModification();
    mod->magic = ReFixEOS::LMOD_MAGIC;
    mod->lobbyId = (Options && Options->LobbyId) ? Options->LobbyId : "";
    mod->localUserId = (Options && Options->LocalUserId) ? Options->LocalUserId : nullptr;
    mod->maxMembers = 4;
    mod->permissionLevel = EOS_LPL_PUBLICADVERTISED;
    mod->invitesAllowed = true;

    *OutLobbyModificationHandle = (EOS_HLobbyModification)mod;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetPermissionLevel(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbyModification_SetPermissionLevel (TID=0x%lx, Handle=%p, Level=%d)",
        tid, Handle, Options ? (int)Options->PermissionLevel : 0);
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->permissionLevel = Options->PermissionLevel;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetMaxMembers(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbyModification_SetMaxMembers (TID=0x%lx, Handle=%p, MaxMembers=%u)",
        tid, Handle, Options ? Options->MaxMembers : 0);
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->maxMembers = Options->MaxMembers;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetBucketId(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbyModification_SetBucketId (TID=0x%lx, Handle=%p, BucketId=%s)",
        tid, Handle, (Options && Options->BucketId) ? Options->BucketId : "null");
    if (!Handle || !Options || !Options->BucketId) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->bucketId = Options->BucketId;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetInvitesAllowed(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetInvitesAllowedOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbyModification_SetInvitesAllowed (TID=0x%lx, Handle=%p, Allowed=%d)",
        tid, Handle, Options ? (int)Options->bInvitesAllowed : 0);
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->invitesAllowed = (Options->bInvitesAllowed != 0);
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_AddAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddAttributeOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbyModification_AddAttribute (TID=0x%lx, Handle=%p, Key=%s)",
        tid, Handle, (Options && Options->Attribute && Options->Attribute->Key) ? Options->Attribute->Key : "null");
    if (!Handle || !Options || !Options->Attribute || !Options->Attribute->Key) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;

    const auto* attr = Options->Attribute;
    std::string key = attr->Key;
    ::EOS_Lobby_AttributeDataValue val;
    val.AsInt64 = attr->Value.AsInt64;

    std::string normVal = ReFixEOS::NormalizeAttributeValue(attr->ValueType, val);
    mod->attributes[key] = normVal;
    return EOS_Success;
}

void EOS_LobbyModification_Release(EOS_HLobbyModification LobbyModificationHandle) {
    if (!LobbyModificationHandle) return;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)LobbyModificationHandle;
    if (mod->magic == ReFixEOS::LMOD_MAGIC) {
        delete mod;
    }
}

void EOS_Lobby_Attribute_Release(EOS_Lobby_Attribute* LobbyAttribute) {
    if (!LobbyAttribute) return;
    if (LobbyAttribute->Data) {
        if (LobbyAttribute->Data->Key) free((void*)LobbyAttribute->Data->Key);
        if (LobbyAttribute->Data->ValueType == EOS_AT_STRING && LobbyAttribute->Data->Value.AsUtf8) {
            free((void*)LobbyAttribute->Data->Value.AsUtf8);
        }
        free(LobbyAttribute->Data);
    }
    free(LobbyAttribute);
}

// =============================================================================
// Lobby Details
// =============================================================================

EOS_EResult EOS_Lobby_CopyLobbyDetailsHandle(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_CopyLobbyDetailsHandle (TID=0x%lx, Handle=%p, Options=%p, LobbyId=%s)",
        tid, Handle, Options, (Options && Options->LobbyId) ? Options->LobbyId : "null");
    if (!Options || !Options->LobbyId || !OutLobbyDetailsHandle) return EOS_InvalidParameters;

    ReFixOnline::LobbyData lob;
    if (!ReFixEOS::RoomManagerBridge::Get().GetLobby(Options->LobbyId, lob)) {
        return EOS_NotFound;
    }

    auto* details = new ReFixEOS::OpaqueLobbyDetails();
    details->Populate(lob);
    *OutLobbyDetailsHandle = (EOS_HLobbyDetails)details;
    return EOS_Success;
}

EOS_EResult EOS_Lobby_CopyLobbyDetailsHandleByInviteId(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    if (!Options || !Options->InviteId || !OutLobbyDetailsHandle) return EOS_InvalidParameters;
    return EOS_NotFound;
}

EOS_EResult EOS_Lobby_CopyLobbyDetailsHandleByUiEventId(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleByUiEventIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    if (!Options || !OutLobbyDetailsHandle) return EOS_InvalidParameters;
    return EOS_NotFound;
}

EOS_EResult EOS_LobbyDetails_CopyInfo(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyInfoOptions* Options, EOS_LobbyDetails_Info** OutLobbyDetailsInfo) {
    if (!Handle || !OutLobbyDetailsInfo) return EOS_InvalidParameters;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return EOS_InvalidParameters;

    auto* info = (EOS_LobbyDetails_Info*)calloc(1, sizeof(EOS_LobbyDetails_Info));
    info->ApiVersion = EOS_LOBBYDETAILS_INFO_API_LATEST;
    info->LobbyId = _strdup(details->data.lobbyId.c_str());
    info->LobbyOwnerUserId = details->ownerPuid;
    info->MaxMembers = details->data.maxMembers;
    info->AvailableSlots = (details->data.maxMembers > details->data.currentMembers) ? (details->data.maxMembers - details->data.currentMembers) : 0;
    info->bAllowInvites = 1;
    info->bAllowHostMigration = 0;
    info->bAllowJoinById = 1;
    info->bPresenceEnabled = 1;

    auto bIt = details->data.attributes.find("bucket_id");
    if (bIt != details->data.attributes.end() && bIt->second.rfind("s:", 0) == 0) {
        info->BucketId = _strdup(bIt->second.substr(2).c_str());
    } else {
        info->BucketId = _strdup("");
    }

    auto pIt = details->data.attributes.find("permission_level");
    if (pIt != details->data.attributes.end() && pIt->second.rfind("i:", 0) == 0) {
        info->PermissionLevel = (EOS_ELobbyPermissionLevel)atoi(pIt->second.substr(2).c_str());
    } else {
        info->PermissionLevel = EOS_LPL_PUBLICADVERTISED;
    }

    *OutLobbyDetailsInfo = info;
    return EOS_Success;
}

void EOS_LobbyDetails_Info_Release(EOS_LobbyDetails_Info* LobbyDetailsInfo) {
    if (!LobbyDetailsInfo) return;
    if (LobbyDetailsInfo->LobbyId) free((void*)LobbyDetailsInfo->LobbyId);
    if (LobbyDetailsInfo->BucketId) free((void*)LobbyDetailsInfo->BucketId);
    free(LobbyDetailsInfo);
}

void EOS_LobbyDetails_Release(EOS_HLobbyDetails LobbyDetailsHandle) {
    if (!LobbyDetailsHandle) return;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)LobbyDetailsHandle;
    if (details->magic == ReFixEOS::LDET_MAGIC) {
        delete details;
    }
}

uint32_t EOS_LobbyDetails_GetAttributeCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetAttributeCountOptions* Options) {
    if (!Handle) return 0;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return 0;
    return (uint32_t)details->attributeList.size();
}

EOS_EResult EOS_LobbyDetails_CopyAttributeByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    if (!Handle || !Options || !OutAttribute) return EOS_InvalidParameters;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return EOS_InvalidParameters;

    if (Options->AttrIndex >= details->attributeList.size()) return EOS_NotFound;

    const auto& item = details->attributeList[Options->AttrIndex];
    std::string key = item.first;
    std::string normVal = item.second;

    auto* attr = (EOS_Lobby_Attribute*)calloc(1, sizeof(EOS_Lobby_Attribute));
    auto* data = (EOS_Lobby_AttributeData*)calloc(1, sizeof(EOS_Lobby_AttributeData));
    attr->ApiVersion = EOS_LOBBY_ATTRIBUTE_API_LATEST;
    attr->Data = data;
    attr->Visibility = 0; // Public

    data->ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    data->Key = _strdup(key.c_str());

    if (normVal.rfind("b:", 0) == 0) {
        data->ValueType = EOS_AT_BOOLEAN;
        data->Value.AsBool = (normVal.substr(2) == "1" ? 1 : 0);
    } else if (normVal.rfind("i:", 0) == 0) {
        data->ValueType = EOS_AT_INT64;
        data->Value.AsInt64 = _strtoi64(normVal.substr(2).c_str(), nullptr, 10);
    } else if (normVal.rfind("d:", 0) == 0) {
        data->ValueType = EOS_AT_DOUBLE;
        data->Value.AsDouble = atof(normVal.substr(2).c_str());
    } else if (normVal.rfind("s:", 0) == 0) {
        data->ValueType = EOS_AT_STRING;
        data->Value.AsUtf8 = _strdup(normVal.substr(2).c_str());
    } else {
        data->ValueType = EOS_AT_STRING;
        data->Value.AsUtf8 = _strdup(normVal.c_str());
    }

    *OutAttribute = attr;
    return EOS_Success;
}

EOS_EResult EOS_LobbyDetails_CopyAttributeByKey(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute) {
    if (!Handle || !Options || !Options->AttrKey || !OutAttribute) return EOS_InvalidParameters;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return EOS_InvalidParameters;

    for (uint32_t i = 0; i < details->attributeList.size(); ++i) {
        if (details->attributeList[i].first == Options->AttrKey) {
            EOS_LobbyDetails_CopyAttributeByIndexOptions idxOpts = {};
            idxOpts.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST;
            idxOpts.AttrIndex = i;
            return EOS_LobbyDetails_CopyAttributeByIndex(Handle, &idxOpts, OutAttribute);
        }
    }
    return EOS_NotFound;
}

uint32_t EOS_LobbyDetails_GetMemberCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberCountOptions* Options) {
    if (!Handle) return 0;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return 0;
    return (uint32_t)details->memberPuids.size();
}

EOS_ProductUserId EOS_LobbyDetails_GetMemberByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberByIndexOptions* Options) {
    if (!Handle || !Options) return nullptr;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return nullptr;
    if (Options->MemberIndex >= details->memberPuids.size()) return nullptr;
    return details->memberPuids[Options->MemberIndex];
}

EOS_ProductUserId EOS_LobbyDetails_GetLobbyOwner(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetLobbyOwnerOptions* Options) {
    if (!Handle) return nullptr;
    auto* details = (ReFixEOS::OpaqueLobbyDetails*)Handle;
    if (details->magic != ReFixEOS::LDET_MAGIC) return nullptr;
    return details->ownerPuid;
}

// =============================================================================
// Lobby Search
// =============================================================================

EOS_EResult EOS_Lobby_CreateLobbySearch(EOS_HLobby Handle, const EOS_Lobby_CreateLobbySearchOptions* Options, EOS_HLobbySearch* OutLobbySearchHandle) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_CreateLobbySearch (TID=0x%lx, Handle=%p, Options=%p)", tid, Handle, Options);
    if (!OutLobbySearchHandle) return EOS_InvalidParameters;

    auto* search = new ReFixEOS::OpaqueLobbySearch();
    search->magic = ReFixEOS::LSRC_MAGIC;
    search->maxResults = (Options && Options->MaxResults > 0) ? Options->MaxResults : 50;

    *OutLobbySearchHandle = (EOS_HLobbySearch)search;
    return EOS_Success;
}

EOS_EResult EOS_LobbySearch_SetParameter(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetParameterOptions* Options) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbySearch_SetParameter (TID=0x%lx, Handle=%p, Options=%p)", tid, Handle, Options);
    if (!Handle || !Options || !Options->Parameter || !Options->Parameter->Key) return EOS_InvalidParameters;

    auto* search = (ReFixEOS::OpaqueLobbySearch*)Handle;
    if (search->magic != ReFixEOS::LSRC_MAGIC) return EOS_InvalidParameters;

    const auto* attr = Options->Parameter;
    std::string key = attr->Key;
    ::EOS_Lobby_AttributeDataValue val;
    val.AsInt64 = attr->Value.AsInt64;

    std::string normVal = ReFixEOS::NormalizeAttributeValue(attr->ValueType, val);
    search->filters[key] = normVal;
    ReFixEOS::LogDiagnostic("EOS_LobbySearch_SetParameter: Added filter '%s' = '%s'", key.c_str(), normVal.c_str());
    return EOS_Success;
}

void EOS_LobbySearch_Find(EOS_HLobbySearch Handle, const EOS_LobbySearch_FindOptions* Options, void* ClientData, void* CompletionDelegate) {
    DWORD tid = GetCurrentThreadId();
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbySearch_Find ENTER (TID=0x%lx, Handle=%p, Options=%p, Delegate=%p)",
        tid, Handle, Options, CompletionDelegate);

    if (!CompletionDelegate) return;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    if (!Handle) {
        ReFixEOS::FindLobbiesClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardFindLobbiesCallback, closure);
        return;
    }

    auto* search = (ReFixEOS::OpaqueLobbySearch*)Handle;
    if (search->magic != ReFixEOS::LSRC_MAGIC) {
        ReFixEOS::FindLobbiesClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = EOS_InvalidParameters;
        cbMgr.QueueCallback((void*)ReFixEOS::ForwardFindLobbiesCallback, closure);
        return;
    }

    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.FindLobbies(search->maxResults, search->filters, [CompletionDelegate, ClientData, search](ReFixOnline::EBackendResult res, const std::vector<ReFixOnline::LobbyData>& lobs) {
        search->searchResults = lobs;
        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_LobbySearch_Find returned %u lobbies", (uint32_t)lobs.size());

        ReFixEOS::FindLobbiesClosure closure = {};
        closure.completionDelegate = CompletionDelegate;
        closure.clientData = ClientData;
        closure.resultCode = (res == ReFixOnline::SUCCESS) ? EOS_Success : EOS_NotFound;

        ReFixEOS::LogDiagnostic("[EOS_RUNTIME] Queue FindLobbies callback");
        ReFixEOS::CallbackManager::Get().QueueCallback((void*)ReFixEOS::ForwardFindLobbiesCallback, closure);
    });
}

uint32_t EOS_LobbySearch_GetSearchResultCount(EOS_HLobbySearch Handle, const EOS_LobbySearch_GetSearchResultCountOptions* Options) {
    if (!Handle) return 0;
    auto* search = (ReFixEOS::OpaqueLobbySearch*)Handle;
    if (search->magic != ReFixEOS::LSRC_MAGIC) return 0;
    return (uint32_t)search->searchResults.size();
}

EOS_EResult EOS_LobbySearch_CopySearchResultByIndex(EOS_HLobbySearch Handle, const EOS_LobbySearch_CopySearchResultByIndexOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle) {
    if (!Handle || !Options || !OutLobbyDetailsHandle) return EOS_InvalidParameters;
    auto* search = (ReFixEOS::OpaqueLobbySearch*)Handle;
    if (search->magic != ReFixEOS::LSRC_MAGIC) return EOS_InvalidParameters;

    if (Options->LobbyIndex >= search->searchResults.size()) return EOS_NotFound;

    const auto& lob = search->searchResults[Options->LobbyIndex];
    auto* details = new ReFixEOS::OpaqueLobbyDetails();
    details->Populate(lob);

    *OutLobbyDetailsHandle = (EOS_HLobbyDetails)details;
    return EOS_Success;
}

void EOS_LobbySearch_Release(EOS_HLobbySearch LobbySearchHandle) {
    if (!LobbySearchHandle) return;
    auto* search = (ReFixEOS::OpaqueLobbySearch*)LobbySearchHandle;
    if (search->magic == ReFixEOS::LSRC_MAGIC) {
        delete search;
    }
}

// =============================================================================
// Notifications
// =============================================================================

EOS_NotificationId EOS_Lobby_AddNotifyLobbyUpdateReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyUpdateReceived registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(201, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyUpdateReceived(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyMemberUpdateReceived registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(202, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLobbyMemberStatusReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyMemberStatusReceived registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(203, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyJoinLobbyAccepted(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyJoinLobbyAccepted registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(204, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyJoinLobbyAccepted(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLeaveLobbyRequested(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLeaveLobbyRequested registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(205, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLeaveLobbyRequested(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyInviteReceived registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(206, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyInviteReceived(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteAccepted(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyInviteAccepted registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(207, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyInviteAccepted(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteRejected(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn) {
    ReFixEOS::LogDiagnostic("[EOS_RUNTIME] EOS_Lobby_AddNotifyLobbyInviteRejected registered");
    return ReFixEOS::CallbackManager::Get().AddNotification(208, ClientData, NotificationFn);
}

void EOS_Lobby_RemoveNotifyLobbyInviteRejected(EOS_HLobby Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

} // extern "C"
