// =============================================================================
// ReFix EOS Online v2 - EOS Connect & Authentication Implementation
// =============================================================================
#include "eos_connect.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <sstream>

namespace ReFixEOS {

constexpr uint32_t CTOK_HANDLE_MAGIC = 0x43544F4B; // 'CTOK'

bool IsDebugLoggingEnabled() {
    static int s_cached = -1;
    if (s_cached == -1) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string iniPath(exePath);
        size_t pos = iniPath.find_last_of("\\/");
        if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";
        else iniPath = "ReFix.ini";

        char buf[32] = { 0 };
        GetPrivateProfileStringA("EOS", "DebugLogging", "true", buf, sizeof(buf), iniPath.c_str());
        s_cached = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0) ? 1 : 0;
    }
    return (s_cached == 1);
}

void LogDiagnostic(const char* format, ...) {
    if (!IsDebugLoggingEnabled()) return;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char finalMsg[1200];
    sprintf_s(finalMsg, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [TID:0x%04X] [EOS:CONNECT] %s\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), buffer);

    OutputDebugStringA(finalMsg);
    printf("%s", finalMsg);

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        logPath = logPath.substr(0, pos + 1) + "ReFix.log";
    } else {
        logPath = "ReFix.log";
    }

    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (f) {
        fprintf(f, "%s", finalMsg);
        fclose(f);
    }
}

static std::string RedactToken(const char* token) {
    if (!token) return "[NULL]";
    size_t len = strlen(token);
    char buf[64];
    sprintf_s(buf, "[REDACTED:len=%zu]", len);
    return std::string(buf);
}

} // namespace ReFixEOS

extern "C" {

void EOS_Connect_Login(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) {
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: CompletionDelegate is NULL, aborting");
        return;
    }

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.RefreshFromEnvironment();

    // 1. Validation of Options & Credentials
    if (!Options || Options->ApiVersion <= 0 || !Options->Credentials) {
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: Invalid parameters (Options=%p)", Options);
        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    const auto* creds = Options->Credentials;
    if (creds->ApiVersion <= 0 || !creds->Token) {
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: Invalid credentials struct (Token=NULL)");
        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    ReFixEOS::LogDiagnostic("EOS_Connect_Login: CredentialType=%d, Token=%s",
        creds->Type, ReFixEOS::RedactToken(creds->Token).c_str());

    // 2. Explicit State Machine by Credential Type
    if (creds->Type == EOS_ECT_STEAM_SESSION_TICKET || creds->Type == EOS_ECT_STEAM_APP_TICKET || creds->Type == 18 /* EOS_ECT_EXTERNAL_ACCOUNT */) {
        // Steam session ticket / external auth authentication
        if (strlen(creds->Token) < 4) {
            ReFixEOS::LogDiagnostic("EOS_Connect_Login: Steam ticket token too short -> EOS_InvalidAuth");
            EOS_Connect_LoginCallbackInfo cbInfo = {};
            cbInfo.ResultCode = EOS_InvalidAuth;
            cbInfo.ClientData = ClientData;
            cbInfo.LocalUserId = nullptr;
            cbInfo.ContinuanceToken = nullptr;
            ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
            return;
        }

        EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: Steam login success -> PUID=%s",
            idMgr.GetLocalProductUserIdString().c_str());

        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = localPuid;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);

        // Dispatch Login Status Notification
        EOS_Connect_LoginStatusChangedCallbackInfo notif = {};
        notif.ClientData = nullptr;
        notif.LocalUserId = localPuid;
        notif.PreviousStatus = EOS_LS_NotLoggedIn;
        notif.CurrentStatus = EOS_LS_LoggedIn;
        ReFixEOS::CallbackManager::Get().DispatchNotification(1, notif);
        return;
    }
    else if (creds->Type == EOS_ECT_DEVICEID_ACCESS_TOKEN) {
        // Device ID login (standalone / fallback)
        EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: DeviceId login success -> PUID=%s",
            idMgr.GetLocalProductUserIdString().c_str());

        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = localPuid;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);

        EOS_Connect_LoginStatusChangedCallbackInfo notif = {};
        notif.ClientData = nullptr;
        notif.LocalUserId = localPuid;
        notif.PreviousStatus = EOS_LS_NotLoggedIn;
        notif.CurrentStatus = EOS_LS_LoggedIn;
        ReFixEOS::CallbackManager::Get().DispatchNotification(1, notif);
        return;
    }
    else if (creds->Type == EOS_ECT_EPIC_ID_TOKEN) {
        // Epic ID token login
        EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: Epic ID token login success -> PUID=%s",
            idMgr.GetLocalProductUserIdString().c_str());

        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = localPuid;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);

        // Dispatch Login Status Notification to registered listeners (e.g. RedpointEOS)
        EOS_Connect_LoginStatusChangedCallbackInfo notif = {};
        notif.ClientData = nullptr;
        notif.LocalUserId = localPuid;
        notif.PreviousStatus = EOS_LS_NotLoggedIn;
        notif.CurrentStatus = EOS_LS_LoggedIn;
        ReFixEOS::CallbackManager::Get().DispatchNotification(1, notif);
        return;
    }
    else {
        // Unsupported credential type
        ReFixEOS::LogDiagnostic("EOS_Connect_Login: Unsupported CredentialType=%d -> EOS_InvalidAuth", creds->Type);
        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidAuth;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        cbInfo.ContinuanceToken = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }
}

void EOS_Connect_CreateUser(EOS_HConnect Handle, const EOS_Connect_CreateUserOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    if (!Options || !Options->ContinuanceToken) {
        ReFixEOS::LogDiagnostic("EOS_Connect_CreateUser: Invalid parameters or null ContinuanceToken");
        EOS_Connect_CreateUserCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    auto* ctok = (ReFixEOS::OpaqueContinuanceToken*)Options->ContinuanceToken;
    if (ctok->magic != ReFixEOS::CTOK_HANDLE_MAGIC) {
        ReFixEOS::LogDiagnostic("EOS_Connect_CreateUser: Invalid ContinuanceToken handle magic");
        EOS_Connect_CreateUserCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidAuth;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    delete ctok;

    EOS_ProductUserId localPuid = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
    ReFixEOS::LogDiagnostic("EOS_Connect_CreateUser: User created -> PUID=%s",
        ReFixEOS::IdentityManager::Get().GetLocalProductUserIdString().c_str());

    EOS_Connect_CreateUserCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    cbInfo.LocalUserId = localPuid;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

void EOS_Connect_LinkAccount(EOS_HConnect Handle, const EOS_Connect_LinkAccountOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    if (!Options || !Options->ContinuanceToken || !Options->LocalUserId) {
        EOS_Connect_LinkAccountCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    auto* ctok = (ReFixEOS::OpaqueContinuanceToken*)Options->ContinuanceToken;
    if (ctok->magic == ReFixEOS::CTOK_HANDLE_MAGIC) {
        delete ctok;
    }

    EOS_Connect_LinkAccountCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    cbInfo.LocalUserId = Options->LocalUserId;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

void EOS_Connect_CreateDeviceId(EOS_HConnect Handle, const EOS_Connect_CreateDeviceIdOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    ReFixEOS::LogDiagnostic("EOS_Connect_CreateDeviceId: Created virtual DeviceId");
    EOS_Connect_CreateDeviceIdCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

void EOS_Connect_DeleteDeviceId(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    EOS_Connect_DeleteDeviceIdCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

void EOS_Connect_Logout(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    ReFixEOS::LogDiagnostic("EOS_Connect_Logout: Logged out");
    EOS_Connect_LoginCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    cbInfo.LocalUserId = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
    cbInfo.ContinuanceToken = nullptr;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

void EOS_Connect_QueryExternalAccountMappings(EOS_HConnect Handle, const EOS_Connect_QueryExternalAccountMappingsOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    if (!Options || !Options->LocalUserId) {
        EOS_Connect_QueryExternalAccountMappingsCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_InvalidParameters;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = nullptr;
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
        return;
    }

    EOS_Connect_QueryExternalAccountMappingsCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    cbInfo.LocalUserId = Options->LocalUserId;
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

EOS_ProductUserId EOS_Connect_GetExternalAccountMapping(EOS_HConnect Handle, const EOS_Connect_GetExternalAccountMappingsOptions* Options) {
    if (!Options || !Options->TargetExternalAccountId) {
        return nullptr;
    }
    return ReFixEOS::IdentityManager::Get().GetOrCreateProductUserIdFromExternal(
        Options->AccountIdType, Options->TargetExternalAccountId);
}

void EOS_Connect_QueryProductUserIdMappings(EOS_HConnect Handle, const EOS_Connect_QueryProductUserIdMappingsOptions* Options, void* ClientData, void* CompletionDelegate) {
    if (!CompletionDelegate) return;

    EOS_Connect_QueryProductUserIdMappingsCallbackInfo cbInfo = {};
    cbInfo.ResultCode = EOS_Success;
    cbInfo.ClientData = ClientData;
    cbInfo.LocalUserId = Options ? Options->LocalUserId : ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
    ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
}

EOS_EResult EOS_Connect_GetProductUserIdMapping(EOS_HConnect Handle, const EOS_Connect_GetProductUserIdMappingOptions* Options, char* OutBuffer, int32_t* InOutBufferLength) {
    if (!InOutBufferLength) return EOS_InvalidParameters;
    auto puid = Options ? Options->TargetProductUserId : ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(puid)) return EOS_InvalidUser;

    std::string str = ReFixEOS::IdentityManager::Get().ProductUserIdToString(puid);
    if (str.empty()) return EOS_InvalidUser;

    int32_t needed = (int32_t)str.length() + 1;
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_LimitExceeded;
    }
    strcpy_s(OutBuffer, *InOutBufferLength, str.c_str());
    *InOutBufferLength = needed - 1;
    return EOS_Success;
}

uint32_t EOS_Connect_GetProductUserExternalAccountCount(EOS_HConnect Handle, void* Options) {
    return 2; // Epic + Steam
}

EOS_EResult EOS_Connect_CopyProductUserInfo(EOS_HConnect Handle, const EOS_Connect_CopyProductUserInfoOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!OutExternalAccountInfo || !Options) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(Options->TargetUserId)) return EOS_InvalidUser;

    void* info = ReFixEOS::IdentityManager::Get().AllocateExternalAccountInfo(Options->TargetUserId, 0, EOS_EAT_STEAM);
    if (!info) return EOS_NotFound;

    *OutExternalAccountInfo = (EOS_Connect_ExternalAccountInfo*)info;
    return EOS_Success;
}

EOS_EResult EOS_Connect_CopyProductUserExternalAccountByIndex(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByIndexOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!OutExternalAccountInfo || !Options) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(Options->TargetUserId)) return EOS_InvalidUser;

    void* info = ReFixEOS::IdentityManager::Get().AllocateExternalAccountInfo(Options->TargetUserId, Options->ExternalAccountInfoIndex, -1);
    if (!info) return EOS_NotFound;

    *OutExternalAccountInfo = (EOS_Connect_ExternalAccountInfo*)info;
    return EOS_Success;
}

EOS_EResult EOS_Connect_CopyProductUserExternalAccountByAccountType(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!OutExternalAccountInfo || !Options) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(Options->TargetUserId)) return EOS_InvalidUser;

    void* info = ReFixEOS::IdentityManager::Get().AllocateExternalAccountInfo(Options->TargetUserId, -1, Options->AccountIdType);
    if (!info) return EOS_NotFound;

    *OutExternalAccountInfo = (EOS_Connect_ExternalAccountInfo*)info;
    return EOS_Success;
}

EOS_EResult EOS_Connect_CopyProductUserExternalAccountByAccountId(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!OutExternalAccountInfo || !Options || !Options->AccountId) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(Options->TargetUserId)) return EOS_InvalidUser;

    auto puid = ReFixEOS::IdentityManager::Get().GetOrCreateProductUserIdFromExternal(EOS_EAT_STEAM, Options->AccountId);
    void* info = ReFixEOS::IdentityManager::Get().AllocateExternalAccountInfo(puid, -1, EOS_EAT_STEAM);
    if (!info) return EOS_NotFound;

    *OutExternalAccountInfo = (EOS_Connect_ExternalAccountInfo*)info;
    return EOS_Success;
}

void EOS_Connect_ExternalAccountInfo_Release(EOS_Connect_ExternalAccountInfo* ExternalAccountInfo) {
    ReFixEOS::IdentityManager::Get().FreeExternalAccountInfo(ExternalAccountInfo);
}

EOS_ProductUserId EOS_Connect_GetLoggedInUserByIndex(EOS_HConnect Handle, int32_t Index) {
    return (Index == 0) ? ReFixEOS::IdentityManager::Get().GetLocalProductUserId() : nullptr;
}

int32_t EOS_Connect_GetLoggedInUsersCount(EOS_HConnect Handle) {
    return 1;
}

EOS_ELoginStatus EOS_Connect_GetLoginStatus(EOS_HConnect Handle, EOS_ProductUserId LocalUserId) {
    if (!LocalUserId) return EOS_LS_NotLoggedIn;
    return ReFixEOS::IdentityManager::Get().IsValidProductUserId(LocalUserId) ? EOS_LS_LoggedIn : EOS_LS_NotLoggedIn;
}

EOS_NotificationId EOS_Connect_AddNotifyLoginStatusChanged(EOS_HConnect Handle, void* Options, void* ClientData, void* NotificationFn) {
    return ReFixEOS::CallbackManager::Get().AddNotification(1, ClientData, NotificationFn);
}

void EOS_Connect_RemoveNotifyLoginStatusChanged(EOS_HConnect Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_NotificationId EOS_Connect_AddNotifyAuthExpiration(EOS_HConnect Handle, void* Options, void* ClientData, void* NotificationFn) {
    return ReFixEOS::CallbackManager::Get().AddNotification(2, ClientData, NotificationFn);
}

void EOS_Connect_RemoveNotifyAuthExpiration(EOS_HConnect Handle, EOS_NotificationId InId) {
    ReFixEOS::CallbackManager::Get().RemoveNotification(InId);
}

EOS_EResult EOS_Connect_VerifyIdToken(EOS_HConnect Handle, void* Options, void* ClientData, void* CompletionDelegate) {
    if (CompletionDelegate) {
        EOS_Connect_LoginCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = ClientData;
        cbInfo.LocalUserId = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
        ReFixEOS::CallbackManager::Get().QueueCallback(CompletionDelegate, cbInfo);
    }
    return EOS_Success;
}

EOS_EResult EOS_Connect_CopyIdToken(EOS_HConnect Handle, void* Options, void** OutIdToken) {
    return EOS_NotFound;
}

} // extern "C"
