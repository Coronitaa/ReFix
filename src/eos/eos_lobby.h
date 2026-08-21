// =============================================================================
// ReFix EOS Online v2 - EOS Lobby Creation & Management Layer
// =============================================================================
#pragma once

#include "eos_types.h"
#include "eos_identity.h"
#include "eos_callbacks.h"
#include "../eos_core/eos_room_manager.h"

#pragma pack(push, 8)

// Attribute Data Value Union / Struct
union EOS_Lobby_AttributeDataValue {
    int64_t AsInt64;
    double AsDouble;
    EOS_Bool AsBool;
    const char* AsUtf8;
};

#define EOS_LOBBY_ATTRIBUTEDATA_API_LATEST 1
struct EOS_Lobby_AttributeData {
    int32_t ApiVersion;
    const char* Key;
    union {
        int64_t AsInt64;
        double AsDouble;
        EOS_Bool AsBool;
        const char* AsUtf8;
    } Value;
    int32_t ValueType; // EOS_AT_BOOLEAN, EOS_AT_INT64, EOS_AT_DOUBLE, EOS_AT_STRING
};

#define EOS_LOBBY_ATTRIBUTE_API_LATEST 1
struct EOS_Lobby_Attribute {
    int32_t ApiVersion;
    EOS_Lobby_AttributeData* Data;
    int32_t Visibility;
};

// Create Lobby Options
#define EOS_LOBBY_CREATELOBBY_API_LATEST 8
struct EOS_Lobby_CreateLobbyOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    uint32_t MaxLobbyMembers;
    EOS_ELobbyPermissionLevel PermissionLevel;
    EOS_Bool bPresenceEnabled;
    const char* Attributes;
    const char* BucketId;
    EOS_Bool bDisableHostMigration;
    EOS_Bool bEnableRTCOptions;
    void* Reserved;
    uint32_t AllowedPlatformIdsCount;
    const uint32_t* AllowedPlatformIds;
    EOS_Bool bCrossplayOptOut;
};

// Create Lobby Callback Info
struct EOS_Lobby_CreateLobbyCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    const char* LobbyId;
};

// Lobby Modification Handle & Options
typedef void* EOS_HLobbyModification;

#define EOS_LOBBY_CREATELOBBYMODIFICATION_API_LATEST 1
struct EOS_Lobby_CreateLobbyModificationOptions {
    int32_t ApiVersion;
    const char* LobbyId;
    EOS_ProductUserId LocalUserId;
};

#define EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST 2
struct EOS_LobbyModification_AddAttributeOptions {
    int32_t ApiVersion;
    const EOS_Lobby_AttributeData* Attribute;
    int32_t Visibility;
};

#define EOS_LOBBYMODIFICATION_SETMAXMEMBERS_API_LATEST 1
struct EOS_LobbyModification_SetMaxMembersOptions {
    int32_t ApiVersion;
    uint32_t MaxMembers;
};

#define EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST 1
struct EOS_LobbyModification_SetPermissionLevelOptions {
    int32_t ApiVersion;
    EOS_ELobbyPermissionLevel PermissionLevel;
};

#define EOS_LOBBYMODIFICATION_SETBUCKETID_API_LATEST 1
struct EOS_LobbyModification_SetBucketIdOptions {
    int32_t ApiVersion;
    const char* BucketId;
};

#define EOS_LOBBYMODIFICATION_SETINVITESALLOWED_API_LATEST 1
struct EOS_LobbyModification_SetInvitesAllowedOptions {
    int32_t ApiVersion;
    EOS_Bool bInvitesAllowed;
};

#pragma pack(pop)

namespace ReFixEOS {

enum class ECreateLobbyState {
    REQUESTED          = 0,
    BACKEND_PENDING    = 1,
    CREATED            = 2,
    FAILED             = 3,
    CALLBACK_QUEUED    = 4,
    CALLBACK_DISPATCHED= 5
};

// Normalizes EOS_Lobby_AttributeData to string key/value
std::string NormalizeAttributeValue(int32_t valueType, const ::EOS_Lobby_AttributeDataValue& val);

} // namespace ReFixEOS

extern "C" {
    void EOS_Lobby_CreateLobby(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, void* CompletionDelegate);
    EOS_EResult EOS_Lobby_CreateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
    EOS_EResult EOS_Lobby_UpdateLobbyModification(EOS_HLobby Handle, void* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
    EOS_EResult EOS_LobbyModification_SetPermissionLevel(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options);
    EOS_EResult EOS_LobbyModification_SetMaxMembers(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options);
    EOS_EResult EOS_LobbyModification_SetBucketId(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options);
    EOS_EResult EOS_LobbyModification_SetInvitesAllowed(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetInvitesAllowedOptions* Options);
    EOS_EResult EOS_LobbyModification_AddAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddAttributeOptions* Options);
    void EOS_LobbyModification_Release(EOS_HLobbyModification LobbyModificationHandle);
    void EOS_Lobby_Attribute_Release(EOS_Lobby_Attribute* LobbyAttribute);
}
