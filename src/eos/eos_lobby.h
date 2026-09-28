// =============================================================================
// ReFix EOS Online v2 - EOS Lobby Creation, Search, Join & Management Layer
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

// Create Lobby Options & Callback Info
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

#define EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST 1
struct EOS_Lobby_UpdateLobbyModificationOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    const char* LobbyId;
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

// Update Lobby
#define EOS_LOBBY_UPDATELOBBY_API_LATEST 1
struct EOS_Lobby_UpdateLobbyOptions {
    int32_t ApiVersion;
    EOS_HLobbyModification LobbyModificationHandle;
};

struct EOS_Lobby_UpdateLobbyCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    const char* LobbyId;
};

// Destroy Lobby
#define EOS_LOBBY_DESTROYLOBBY_API_LATEST 1
struct EOS_Lobby_DestroyLobbyOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    const char* LobbyId;
};

struct EOS_Lobby_DestroyLobbyCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    const char* LobbyId;
};

// Lobby Details Handles & Options
typedef void* EOS_HLobbyDetails;

#define EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST 1
struct EOS_Lobby_CopyLobbyDetailsHandleOptions {
    int32_t ApiVersion;
    const char* LobbyId;
    EOS_ProductUserId LocalUserId;
};

#define EOS_LOBBY_COPYLOBBYDETAILSHANDLEBYINVITEID_API_LATEST 1
struct EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions {
    int32_t ApiVersion;
    const char* InviteId;
};

#define EOS_LOBBY_COPYLOBBYDETAILSHANDLEBYUIEVENTID_API_LATEST 1
struct EOS_Lobby_CopyLobbyDetailsHandleByUiEventIdOptions {
    int32_t ApiVersion;
    uint64_t UiEventId;
};

#define EOS_LOBBYDETAILS_COPYINFO_API_LATEST 1
struct EOS_LobbyDetails_CopyInfoOptions {
    int32_t ApiVersion;
};

#define EOS_LOBBYDETAILS_INFO_API_LATEST 2
struct EOS_LobbyDetails_Info {
    int32_t ApiVersion;
    const char* LobbyId;
    EOS_ProductUserId LobbyOwnerUserId;
    EOS_ELobbyPermissionLevel PermissionLevel;
    uint32_t AvailableSlots;
    uint32_t MaxMembers;
    EOS_Bool bAllowInvites;
    const char* BucketId;
    EOS_Bool bAllowHostMigration;
    EOS_Bool bRTCOptionsEnabled;
    EOS_Bool bAllowJoinById;
    EOS_Bool bRejoinAfterKickRequiresInvite;
    EOS_Bool bPresenceEnabled;
    uint32_t AllowedPlatformIdsCount;
    const uint32_t* AllowedPlatformIds;
};

#define EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST 1
struct EOS_LobbyDetails_GetAttributeCountOptions {
    int32_t ApiVersion;
};

#define EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST 1
struct EOS_LobbyDetails_CopyAttributeByIndexOptions {
    int32_t ApiVersion;
    uint32_t AttrIndex;
};

#define EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST 1
struct EOS_LobbyDetails_CopyAttributeByKeyOptions {
    int32_t ApiVersion;
    const char* AttrKey;
};

#define EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST 1
struct EOS_LobbyDetails_GetMemberCountOptions {
    int32_t ApiVersion;
};

#define EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST 1
struct EOS_LobbyDetails_GetMemberByIndexOptions {
    int32_t ApiVersion;
    uint32_t MemberIndex;
};

#define EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST 1
struct EOS_LobbyDetails_GetLobbyOwnerOptions {
    int32_t ApiVersion;
};

// Lobby Search Handles & Options
typedef void* EOS_HLobbySearch;

#define EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST 1
struct EOS_Lobby_CreateLobbySearchOptions {
    int32_t ApiVersion;
    uint32_t MaxResults;
};

#define EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST 1
struct EOS_LobbySearch_SetParameterOptions {
    int32_t ApiVersion;
    const EOS_Lobby_AttributeData* Parameter;
    int32_t ComparisonOp;
};

#define EOS_LOBBYSEARCH_FIND_API_LATEST 1
struct EOS_LobbySearch_FindOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
};

struct EOS_LobbySearch_FindCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
};

#define EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST 1
struct EOS_LobbySearch_GetSearchResultCountOptions {
    int32_t ApiVersion;
};

#define EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST 1
struct EOS_LobbySearch_CopySearchResultByIndexOptions {
    int32_t ApiVersion;
    uint32_t LobbyIndex;
};

// Join & Leave Lobby Options & Callback Info
#define EOS_LOBBY_JOINLOBBY_API_LATEST 3
struct EOS_Lobby_JoinLobbyOptions {
    int32_t ApiVersion;
    EOS_HLobbyDetails LobbyDetailsHandle;
    EOS_ProductUserId LocalUserId;
    EOS_Bool bPresenceEnabled;
    EOS_Bool bCrossplayOptOut;
};

struct EOS_Lobby_JoinLobbyCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    const char* LobbyId;
};

#define EOS_LOBBY_LEAVELOBBY_API_LATEST 1
struct EOS_Lobby_LeaveLobbyOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    const char* LobbyId;
};

struct EOS_Lobby_LeaveLobbyCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    const char* LobbyId;
};

// Notifications
enum EOS_ELobbyMemberStatus {
    EOS_LMS_JOINED = 0,
    EOS_LMS_LEFT = 1,
    EOS_LMS_DISCONNECTED = 2,
    EOS_LMS_KICKED = 3,
    EOS_LMS_PROMOTED = 4,
    EOS_LMS_CLOSED = 5
};

struct EOS_Lobby_LobbyUpdateReceivedCallbackInfo {
    void* ClientData;
    const char* LobbyId;
};

struct EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo {
    void* ClientData;
    const char* LobbyId;
    EOS_ProductUserId TargetUserId;
};

struct EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo {
    void* ClientData;
    const char* LobbyId;
    EOS_ProductUserId TargetUserId;
    EOS_ELobbyMemberStatus CurrentStatus;
};

struct EOS_Lobby_JoinLobbyAcceptedCallbackInfo {
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    uint64_t UiEventId;
};

struct EOS_Lobby_LeaveLobbyRequestedCallbackInfo {
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    const char* LobbyId;
};

struct EOS_Lobby_LobbyInviteReceivedCallbackInfo {
    void* ClientData;
    const char* InviteId;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
};

struct EOS_Lobby_LobbyInviteAcceptedCallbackInfo {
    void* ClientData;
    const char* InviteId;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
    const char* LobbyId;
};

struct EOS_Lobby_LobbyInviteRejectedCallbackInfo {
    void* ClientData;
    const char* InviteId;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
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
    // Lobby Lifecycle
    void EOS_Lobby_CreateLobby(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Lobby_UpdateLobby(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Lobby_DestroyLobby(EOS_HLobby Handle, const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Lobby_JoinLobby(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Lobby_LeaveLobby(EOS_HLobby Handle, const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, void* CompletionDelegate);

    // Lobby Modification
    EOS_EResult EOS_Lobby_CreateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
    EOS_EResult EOS_Lobby_UpdateLobbyModification(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
    EOS_EResult EOS_LobbyModification_SetPermissionLevel(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options);
    EOS_EResult EOS_LobbyModification_SetMaxMembers(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options);
    EOS_EResult EOS_LobbyModification_SetBucketId(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options);
    EOS_EResult EOS_LobbyModification_SetInvitesAllowed(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetInvitesAllowedOptions* Options);
    EOS_EResult EOS_LobbyModification_AddAttribute(EOS_HLobbyModification Handle, const EOS_LobbyModification_AddAttributeOptions* Options);
    void EOS_LobbyModification_Release(EOS_HLobbyModification LobbyModificationHandle);
    void EOS_Lobby_Attribute_Release(EOS_Lobby_Attribute* LobbyAttribute);

    // Lobby Details
    EOS_EResult EOS_Lobby_CopyLobbyDetailsHandle(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    EOS_EResult EOS_Lobby_CopyLobbyDetailsHandleByInviteId(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    EOS_EResult EOS_Lobby_CopyLobbyDetailsHandleByUiEventId(EOS_HLobby Handle, const EOS_Lobby_CopyLobbyDetailsHandleByUiEventIdOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    EOS_EResult EOS_LobbyDetails_CopyInfo(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyInfoOptions* Options, EOS_LobbyDetails_Info** OutLobbyDetailsInfo);
    void EOS_LobbyDetails_Info_Release(EOS_LobbyDetails_Info* LobbyDetailsInfo);
    void EOS_LobbyDetails_Release(EOS_HLobbyDetails LobbyDetailsHandle);
    uint32_t EOS_LobbyDetails_GetAttributeCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetAttributeCountOptions* Options);
    EOS_EResult EOS_LobbyDetails_CopyAttributeByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByIndexOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    EOS_EResult EOS_LobbyDetails_CopyAttributeByKey(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_CopyAttributeByKeyOptions* Options, EOS_Lobby_Attribute** OutAttribute);
    uint32_t EOS_LobbyDetails_GetMemberCount(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberCountOptions* Options);
    EOS_ProductUserId EOS_LobbyDetails_GetMemberByIndex(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetMemberByIndexOptions* Options);
    EOS_ProductUserId EOS_LobbyDetails_GetLobbyOwner(EOS_HLobbyDetails Handle, const EOS_LobbyDetails_GetLobbyOwnerOptions* Options);

    // Lobby Search
    EOS_EResult EOS_Lobby_CreateLobbySearch(EOS_HLobby Handle, const EOS_Lobby_CreateLobbySearchOptions* Options, EOS_HLobbySearch* OutLobbySearchHandle);
    EOS_EResult EOS_LobbySearch_SetParameter(EOS_HLobbySearch Handle, const EOS_LobbySearch_SetParameterOptions* Options);
    void EOS_LobbySearch_Find(EOS_HLobbySearch Handle, const EOS_LobbySearch_FindOptions* Options, void* ClientData, void* CompletionDelegate);
    uint32_t EOS_LobbySearch_GetSearchResultCount(EOS_HLobbySearch Handle, const EOS_LobbySearch_GetSearchResultCountOptions* Options);
    EOS_EResult EOS_LobbySearch_CopySearchResultByIndex(EOS_HLobbySearch Handle, const EOS_LobbySearch_CopySearchResultByIndexOptions* Options, EOS_HLobbyDetails* OutLobbyDetailsHandle);
    void EOS_LobbySearch_Release(EOS_HLobbySearch LobbySearchHandle);

    // Notifications
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyUpdateReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyUpdateReceived(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyMemberStatusReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyJoinLobbyAccepted(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyJoinLobbyAccepted(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLeaveLobbyRequested(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLeaveLobbyRequested(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteReceived(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyInviteReceived(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteAccepted(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyInviteAccepted(EOS_HLobby Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Lobby_AddNotifyLobbyInviteRejected(EOS_HLobby Handle, const void* Options, void* ClientData, void* NotificationFn);
    void EOS_Lobby_RemoveNotifyLobbyInviteRejected(EOS_HLobby Handle, EOS_NotificationId InId);
}
