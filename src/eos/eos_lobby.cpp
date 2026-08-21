// =============================================================================
// ReFix EOS Online v2 - EOS Lobby Creation Implementation
// =============================================================================
#include "eos_lobby.h"
#include "eos_connect.h"
#include <cstdio>
#include <sstream>
#include <iomanip>

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

// In-Memory LobbyModification Model for Pending Modifications
struct OpaqueLobbyModification {
    uint32_t magic; // 0x4C4D4F44 ('LMOD')
    std::string lobbyId;
    EOS_ProductUserId localUserId;
    uint32_t maxMembers;
    EOS_ELobbyPermissionLevel permissionLevel;
    std::string bucketId;
    bool invitesAllowed;
    std::unordered_map<std::string, std::string> attributes;
};

static constexpr uint32_t LMOD_MAGIC = 0x4C4D4F44;

} // namespace ReFixEOS

extern "C" {

void EOS_Lobby_CreateLobby(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, void* CompletionDelegate) {
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
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = nullptr;
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    // 2. Validate LocalUserId (Must be valid registered opaque handle)
    if (!Options->LocalUserId || !idMgr.IsValidProductUserId(Options->LocalUserId)) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Invalid LocalUserId handle (%p)", Options->LocalUserId);
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidUser;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = nullptr;
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    // 3. Validate Capacity
    uint32_t maxMembers = Options->MaxLobbyMembers;
    if (maxMembers == 0 || maxMembers > ReFixOnline::MAX_LOBBY_MEMBERS) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Invalid MaxLobbyMembers (%u) -> EOS_InvalidParameters", maxMembers);
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = nullptr;
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    // 4. Validate BucketId if present
    if (Options->BucketId && strlen(Options->BucketId) > ReFixOnline::MAX_ATTRIBUTE_VAL_LEN) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: BucketId length exceeded limit");
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = nullptr;
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
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

    // 6. Asynchronous Request to Authoritative Backend State (REQUESTED -> BACKEND_PENDING -> CREATED)
    ReFixOnline::LobbyData outLobby;
    auto res = roomBridge.GetServerState().CreateLobby(puidStr, maxMembers, attributes, outLobby);

    if (res == ReFixOnline::SUCCESS) {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Backend SUCCESS -> LobbyId=%s, Owner=%s",
            outLobby.lobbyId.c_str(), outLobby.ownerUserId.c_str());

        // Allocate persistent string buffer for callback payload
        char* lobbyIdBuf = _strdup(outLobby.lobbyId.c_str());

        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = lobbyIdBuf;

        // Callback is strictly queued and executed during EOS_Platform_Tick()
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
    } else {
        ReFixEOS::LogDiagnostic("EOS_Lobby_CreateLobby: Backend failed with error code %d", (int)res);
        EOS_Lobby_CreateLobbyCallbackInfo cbInfo = {};
        cbInfo.ResultCode = (res == ReFixOnline::NOT_AUTHENTICATED) ? EOS_NoConnection : EOS_LimitExceeded;
        cbInfo.ClientData = ClientData;
        cbInfo.LobbyId = nullptr;
        cbMgr.QueueCallback(CompletionDelegate, cbInfo);
    }
}

EOS_EResult EOS_Lobby_CreateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle) {
    if (!Options || !OutLobbyModificationHandle) return EOS_InvalidParameters;

    auto* mod = new ReFixEOS::OpaqueLobbyModification();
    mod->magic = ReFixEOS::LMOD_MAGIC;
    mod->lobbyId = Options->LobbyId ? Options->LobbyId : "";
    mod->localUserId = Options->LocalUserId;
    mod->maxMembers = 4;
    mod->permissionLevel = EOS_LPL_PUBLICADVERTISED;
    mod->invitesAllowed = true;

    *OutLobbyModificationHandle = (EOS_HLobbyModification)mod;
    return EOS_Success;
}

EOS_EResult EOS_Lobby_UpdateLobbyModification(EOS_HLobby Handle, void* Options, EOS_HLobbyModification* OutLobbyModificationHandle) {
    if (!OutLobbyModificationHandle) return EOS_InvalidParameters;
    return EOS_Lobby_CreateLobbyModification(Handle, nullptr, OutLobbyModificationHandle);
}

EOS_EResult EOS_LobbyModification_SetPermissionLevel(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options) {
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->permissionLevel = Options->PermissionLevel;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetMaxMembers(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options) {
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->maxMembers = Options->MaxMembers;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetBucketId(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options) {
    if (!Handle || !Options || !Options->BucketId) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->bucketId = Options->BucketId;
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_SetInvitesAllowed(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetInvitesAllowedOptions* Options) {
    if (!Handle || !Options) return EOS_InvalidParameters;
    auto* mod = (ReFixEOS::OpaqueLobbyModification*)Handle;
    if (mod->magic != ReFixEOS::LMOD_MAGIC) return EOS_InvalidParameters;
    mod->invitesAllowed = (Options->bInvitesAllowed != 0);
    return EOS_Success;
}

EOS_EResult EOS_LobbyModification_AddAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddAttributeOptions* Options) {
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
    if (LobbyAttribute) {
        if (LobbyAttribute->Data) {
            free((void*)LobbyAttribute->Data->Key);
            free(LobbyAttribute->Data);
        }
        free(LobbyAttribute);
    }
}

} // extern "C"
