// =============================================================================
// ReFix EOS Online v2 - EOS Connect & Authentication Layer
// =============================================================================
#pragma once

#include "eos_types.h"
#include "eos_identity.h"
#include "eos_callbacks.h"

#pragma pack(push, 8)

// Credentials struct
#define EOS_CONNECT_CREDENTIALS_API_LATEST 1
struct EOS_Connect_Credentials {
    int32_t ApiVersion;
    const char* Token;
    int32_t Type;
};

// Login Options & Callback Info
#define EOS_CONNECT_LOGIN_API_LATEST 2
struct EOS_Connect_LoginOptions {
    int32_t ApiVersion;
    const EOS_Connect_Credentials* Credentials;
    void* UserLoginInfo;
};

struct EOS_Connect_LoginCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ContinuanceToken ContinuanceToken;
};

// CreateUser Options & Callback Info
#define EOS_CONNECT_CREATEUSER_API_LATEST 1
struct EOS_Connect_CreateUserOptions {
    int32_t ApiVersion;
    EOS_ContinuanceToken ContinuanceToken;
};

struct EOS_Connect_CreateUserCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

// LinkAccount Options & Callback Info
#define EOS_CONNECT_LINKACCOUNT_API_LATEST 1
struct EOS_Connect_LinkAccountOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    EOS_ContinuanceToken ContinuanceToken;
};

struct EOS_Connect_LinkAccountCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

// DeviceId Options & Callback Info
#define EOS_CONNECT_CREATEDEVICEID_API_LATEST 1
struct EOS_Connect_CreateDeviceIdOptions {
    int32_t ApiVersion;
    const char* DeviceModel;
};

struct EOS_Connect_CreateDeviceIdCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
};

struct EOS_Connect_DeleteDeviceIdCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
};

// External Account Mappings Options & Callback Info
#define EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST 1
struct EOS_Connect_QueryExternalAccountMappingsOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    int32_t AccountIdType;
    const char** ExternalAccountIds;
    uint32_t ExternalAccountIdCount;
};

struct EOS_Connect_QueryExternalAccountMappingsCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

#define EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST 1
struct EOS_Connect_GetExternalAccountMappingsOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    int32_t AccountIdType;
    const char* TargetExternalAccountId;
};

// ProductUserId Mappings Options & Callback Info
#define EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST 2
struct EOS_Connect_QueryProductUserIdMappingsOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId* ProductUserIds;
    uint32_t ProductUserIdCount;
};

struct EOS_Connect_QueryProductUserIdMappingsCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

#define EOS_CONNECT_GETPRODUCTUSERIDMAPPING_API_LATEST 1
struct EOS_Connect_GetProductUserIdMappingOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    int32_t AccountIdType;
    EOS_ProductUserId TargetProductUserId;
};

// Copy Product User Info Options
#define EOS_CONNECT_COPYPRODUCTUSERINFO_API_LATEST 1
struct EOS_Connect_CopyProductUserInfoOptions {
    int32_t ApiVersion;
    EOS_ProductUserId TargetUserId;
};

#define EOS_CONNECT_COPYPRODUCTUSEREXTERNALACCOUNTBYINDEX_API_LATEST 1
struct EOS_Connect_CopyProductUserExternalAccountByIndexOptions {
    int32_t ApiVersion;
    EOS_ProductUserId TargetUserId;
    uint32_t ExternalAccountInfoIndex;
};

#define EOS_CONNECT_COPYPRODUCTUSEREXTERNALACCOUNTBYACCOUNTTYPE_API_LATEST 1
struct EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions {
    int32_t ApiVersion;
    EOS_ProductUserId TargetUserId;
    int32_t AccountIdType;
};

#define EOS_CONNECT_COPYPRODUCTUSEREXTERNALACCOUNTBYACCOUNTID_API_LATEST 1
struct EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions {
    int32_t ApiVersion;
    EOS_ProductUserId TargetUserId;
    const char* AccountId;
};

#define EOS_CONNECT_EXTERNALACCOUNTINFO_API_LATEST 1
struct EOS_Connect_ExternalAccountInfo {
    int32_t ApiVersion;
    EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    int32_t AccountIdType;
    const char* AccountId;
    int64_t LastLoginTime;
};

// Login Status Callback Info
struct EOS_Connect_LoginStatusChangedCallbackInfo {
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ELoginStatus PreviousStatus;
    EOS_ELoginStatus CurrentStatus;
};

// Auth Expiration Callback Info
struct EOS_Connect_AuthExpirationCallbackInfo {
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

#pragma pack(pop)

namespace ReFixEOS {

// Opaque handle structure for ContinuanceToken
struct OpaqueContinuanceToken {
    uint32_t magic; // 0x43544F4B ('CTOK')
    int32_t credentialType;
    std::string tokenData;
};

void LogDiagnostic(const char* format, ...);
bool IsDebugLoggingEnabled();

} // namespace ReFixEOS

// C ABI Exports
extern "C" {
    void EOS_Connect_Login(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Connect_CreateUser(EOS_HConnect Handle, const EOS_Connect_CreateUserOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Connect_LinkAccount(EOS_HConnect Handle, const EOS_Connect_LinkAccountOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Connect_CreateDeviceId(EOS_HConnect Handle, const EOS_Connect_CreateDeviceIdOptions* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Connect_DeleteDeviceId(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate);
    void EOS_Connect_Logout(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate);

    void EOS_Connect_QueryExternalAccountMappings(EOS_HConnect Handle, const EOS_Connect_QueryExternalAccountMappingsOptions* Options, void* ClientData, void* CompletionDelegate);
    EOS_ProductUserId EOS_Connect_GetExternalAccountMapping(EOS_HConnect Handle, const EOS_Connect_GetExternalAccountMappingsOptions* Options);

    void EOS_Connect_QueryProductUserIdMappings(EOS_HConnect Handle, const EOS_Connect_QueryProductUserIdMappingsOptions* Options, void* ClientData, void* CompletionDelegate);
    EOS_EResult EOS_Connect_GetProductUserIdMapping(EOS_HConnect Handle, const EOS_Connect_GetProductUserIdMappingOptions* Options, char* OutBuffer, int32_t* InOutBufferLength);

    uint32_t EOS_Connect_GetProductUserExternalAccountCount(EOS_HConnect Handle, void* Options);
    EOS_EResult EOS_Connect_CopyProductUserInfo(EOS_HConnect Handle, const EOS_Connect_CopyProductUserInfoOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo);
    EOS_EResult EOS_Connect_CopyProductUserExternalAccountByIndex(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByIndexOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo);
    EOS_EResult EOS_Connect_CopyProductUserExternalAccountByAccountType(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo);
    EOS_EResult EOS_Connect_CopyProductUserExternalAccountByAccountId(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo);
    void EOS_Connect_ExternalAccountInfo_Release(EOS_Connect_ExternalAccountInfo* ExternalAccountInfo);

    EOS_ProductUserId EOS_Connect_GetLoggedInUserByIndex(EOS_HConnect Handle, int32_t Index);
    int32_t EOS_Connect_GetLoggedInUsersCount(EOS_HConnect Handle);
    EOS_ELoginStatus EOS_Connect_GetLoginStatus(EOS_HConnect Handle, EOS_ProductUserId LocalUserId);

    EOS_NotificationId EOS_Connect_AddNotifyLoginStatusChanged(EOS_HConnect Handle, void* Options, void* ClientData, void* NotificationFn);
    void EOS_Connect_RemoveNotifyLoginStatusChanged(EOS_HConnect Handle, EOS_NotificationId InId);
    EOS_NotificationId EOS_Connect_AddNotifyAuthExpiration(EOS_HConnect Handle, void* Options, void* ClientData, void* NotificationFn);
    void EOS_Connect_RemoveNotifyAuthExpiration(EOS_HConnect Handle, EOS_NotificationId InId);

    EOS_EResult EOS_Connect_VerifyIdToken(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate);
    EOS_EResult EOS_Connect_CopyIdToken(EOS_HConnect Handle, void* Options, void** OutIdToken);
}
