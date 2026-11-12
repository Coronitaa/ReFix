// ReFix EOS v3 - Dynamic Passthrough Architecture to genuine EOSSDK_original.dll
#include "../core/refix_common.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"
#include "../net/steam_backend.h"
#include "eos_api_passthrough.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

extern HMODULE g_hDllModule;
extern "C" void* g_eosProcs[679];

namespace refix {

namespace {

using EOS_Connect_Login_Fn = void (EOS_CALL*)(EOS_HConnect, const EOS_Connect_LoginOptions*, void*, const EOS_Connect_OnLoginCallback);
using EOS_Connect_CreateDeviceId_Fn = void (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CreateDeviceIdOptions*, void*, const EOS_Connect_OnCreateDeviceIdCallback);
using EOS_Connect_CreateUser_Fn = void (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CreateUserOptions*, void*, const EOS_Connect_OnCreateUserCallback);
using EOS_Connect_GetProductUserExternalAccountCount_Fn = uint32_t (EOS_CALL*)(EOS_HConnect, const EOS_Connect_GetProductUserExternalAccountCountOptions*);
using EOS_Connect_CopyProductUserExternalAccountByAccountType_Fn = EOS_EResult (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions*, EOS_Connect_ExternalAccountInfo**);
using EOS_Connect_CopyProductUserExternalAccountByIndex_Fn = EOS_EResult (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CopyProductUserExternalAccountByIndexOptions*, EOS_Connect_ExternalAccountInfo**);
using EOS_Connect_CopyProductUserExternalAccountByAccountId_Fn = EOS_EResult (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions*, EOS_Connect_ExternalAccountInfo**);
using EOS_Connect_CopyProductUserInfo_Fn = EOS_EResult (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CopyProductUserInfoOptions*, EOS_Connect_ExternalAccountInfo**);
using EOS_Connect_CopyUserInfo_Fn = EOS_EResult (EOS_CALL*)(EOS_HConnect, const EOS_Connect_CopyProductUserInfoOptions*, EOS_Connect_ExternalAccountInfo**);
using EOS_Connect_ExternalAccountInfo_Release_Fn = void (EOS_CALL*)(EOS_Connect_ExternalAccountInfo*);
using EOS_Connect_VerifyIdToken_Fn = void (EOS_CALL*)(EOS_HConnect, const EOS_Connect_VerifyIdTokenOptions*, void*, const EOS_Connect_OnVerifyIdTokenCallback);

static HMODULE g_hGenuineSdk = nullptr;
static EOS_Connect_Login_Fn g_orig_EOS_Connect_Login = nullptr;
static EOS_Connect_CreateDeviceId_Fn g_orig_EOS_Connect_CreateDeviceId = nullptr;
static EOS_Connect_CreateUser_Fn g_orig_EOS_Connect_CreateUser = nullptr;
static EOS_Connect_GetProductUserExternalAccountCount_Fn g_orig_EOS_Connect_GetProductUserExternalAccountCount = nullptr;
static EOS_Connect_CopyProductUserExternalAccountByAccountType_Fn g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountType = nullptr;
static EOS_Connect_CopyProductUserExternalAccountByIndex_Fn g_orig_EOS_Connect_CopyProductUserExternalAccountByIndex = nullptr;
static EOS_Connect_CopyProductUserExternalAccountByAccountId_Fn g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountId = nullptr;
static EOS_Connect_CopyProductUserInfo_Fn g_orig_EOS_Connect_CopyProductUserInfo = nullptr;
static EOS_Connect_ExternalAccountInfo_Release_Fn g_orig_EOS_Connect_ExternalAccountInfo_Release = nullptr;
static EOS_Connect_VerifyIdToken_Fn g_orig_EOS_Connect_VerifyIdToken = nullptr;
static EOS_ProductUserId g_localProductUserId = nullptr;
static std::mutex g_puidMutex;
static std::map<EOS_ProductUserId, std::string> g_puidToSteamId;

std::string DllDirectory() {
    char buf[MAX_PATH] = { 0 };
    if (g_hDllModule) {
        GetModuleFileNameA(g_hDllModule, buf, MAX_PATH);
        std::string p(buf);
        size_t pos = p.find_last_of("\\/");
        if (pos != std::string::npos) return p.substr(0, pos + 1);
    }
    return ".\\";
}

bool FileExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string FindOriginalSdkCandidate() {
    std::vector<std::string> candidates = {
        DllDirectory() + "EOSSDK_original.dll",
        DllDirectory() + "EOSSDK-Win64-Shipping_original.dll",
        GameDirectory() + "RedpointEOS\\EOSSDK_original.dll",
        GameDirectory() + "RedpointEOS\\EOSSDK-Win64-Shipping.dll",
        GameDirectory() + "EOSSDK_original.dll",
        GameDirectory() + "EOSSDK-Win64-Shipping_original.dll",
        ".\\EOSSDK_original.dll",
        ".\\EOSSDK-Win64-Shipping_original.dll"
    };

    for (const auto& path : candidates) {
        if (!FileExists(path)) continue;

        // Verify the candidate is not ReFix itself
        HMODULE hTest = LoadLibraryExA(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
        if (hTest) {
            // Check exported name table without executing DllMain
            FreeLibrary(hTest);
        }

        HMODULE hCheck = LoadLibraryA(path.c_str());
        if (hCheck) {
            if (GetProcAddress(hCheck, "ReFix") != nullptr) {
                RFLOG(Core, "[Passthrough] Found '%s', but it contains ReFix signature; skipping", path.c_str());
                FreeLibrary(hCheck);
                continue;
            }
            FreeLibrary(hCheck);
            return path;
        }
    }
    return "";
}

std::string GetPlayerDisplayName() {
    std::string name = SteamBackend::Get().LocalPersonaName();
    if (!name.empty()) return name;

    name = Config::Get().GetString("User", "Name", "");
    if (!name.empty()) return name;

    char envBuf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0]) {
        return envBuf;
    }
    return "Player";
}

std::string GetPlayerSteamId() {
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (hSteam) {
        typedef uint64_t (*fn_GetSteamID)();
        fn_GetSteamID pfn = (fn_GetSteamID)GetProcAddress(hSteam, "ReFix_GetLocalSteamID");
        if (pfn) {
            uint64_t sid = pfn();
            if (sid != 0) return std::to_string(sid);
        }
    }

    uint64_t id = SteamBackend::Get().LocalSteamId();
    if (id != 0) return std::to_string(id);

    char envBuf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_USER_ID", envBuf, sizeof(envBuf)) > 0 && envBuf[0]) {
        return envBuf;
    }
    if (GetEnvironmentVariableA("REFIX_ACTIVE_PROFILE", envBuf, sizeof(envBuf)) > 0 && envBuf[0]) {
        uint64_t profId = _strtoui64(envBuf, nullptr, 10);
        if (profId > 70000000000000000ULL) {
            return std::to_string(profId);
        }
    }

    std::string sid = Config::Get().GetString("User", "SteamID", "");
    if (!sid.empty() && sid != "76561197960287930" && sid != "76561198000000001") return sid;

    int slot = 0;
    char slotBuf[16] = { 0 };
    if (GetEnvironmentVariableA("REFIX_MULTI_SLOT", slotBuf, sizeof(slotBuf)) > 0) {
        slot = atoi(slotBuf);
    } else if (hSteam) {
        typedef int (*fn_GetSlot)();
        fn_GetSlot pfnSlot = (fn_GetSlot)GetProcAddress(hSteam, "ReFix_GetInstanceSlot");
        if (pfnSlot) slot = pfnSlot();
    }

    uint64_t fallback = 0x0110000100000000ULL | (0x10000ULL + (uint64_t)(slot * 1000 + (GetCurrentProcessId() & 0x3FF)));
    return std::to_string(fallback);
}

// ---------------------------------------------------------------------------
// 3-Stage DeviceIdAuth State Machine
// ---------------------------------------------------------------------------
struct DeviceIdAuthContext {
    EOS_HConnect Handle = nullptr;
    void* OriginalClientData = nullptr;
    EOS_Connect_OnLoginCallback OriginalCompletionDelegate = nullptr;
    std::string DisplayName;
    EOS_Connect_Credentials DeviceCreds{};
    EOS_Connect_UserLoginInfo UserLoginInfo{};
    EOS_Connect_LoginOptions LoginOptions{};
    EOS_Connect_CreateDeviceIdOptions CreateDeviceOptions{};
    EOS_Connect_CreateUserOptions CreateUserOptions{};
    EOS_ContinuanceToken ContinuanceToken = nullptr;
};

static void EOS_CALL OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
static void DoDeviceIdLogin(DeviceIdAuthContext* ctx);
static void EOS_CALL OnDeviceIdLoginCallback(const EOS_Connect_LoginCallbackInfo* Data);
static void EOS_CALL OnCreateUserCallback(const EOS_Connect_CreateUserCallbackInfo* Data);

EOS_DECLARE_FUNC(void) Passthrough_EOS_Connect_Login(
    EOS_HConnect Handle,
    const EOS_Connect_LoginOptions* Options,
    void* ClientData,
    const EOS_Connect_OnLoginCallback CompletionDelegate
) {
    if (!g_orig_EOS_Connect_Login) return;

    bool deviceIdAuth = Config::Get().GetBool("EOS", "DeviceIdAuth", true);
    if (!deviceIdAuth || !Options || !Options->Credentials) {
        g_orig_EOS_Connect_Login(Handle, Options, ClientData, CompletionDelegate);
        return;
    }

    int credType = (int)Options->Credentials->Type;
    // 1 = STEAM_APP_TICKET, 18 = STEAM_SESSION_TICKET
    if (credType != 1 && credType != 18) {
        RFLOG(Auth, "[Passthrough] EOS_Connect_Login with credType=%d; forwarding directly to genuine SDK", credType);
        g_orig_EOS_Connect_Login(Handle, Options, ClientData, CompletionDelegate);
        return;
    }

    RFLOG(Auth, "[Passthrough] EOS_Connect_Login intercepted Steam ticket (type %d). Initiating 3-stage DeviceIdAuth state machine...", credType);

    auto* ctx = new DeviceIdAuthContext();
    ctx->Handle = Handle;
    ctx->OriginalClientData = ClientData;
    ctx->OriginalCompletionDelegate = CompletionDelegate;
    ctx->DisplayName = GetPlayerDisplayName();
    if (ctx->DisplayName.empty()) ctx->DisplayName = "Player";
    if (ctx->DisplayName.size() > 32) ctx->DisplayName.resize(32);

    if (g_orig_EOS_Connect_CreateDeviceId) {
        ctx->CreateDeviceOptions = {};
        ctx->CreateDeviceOptions.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
        ctx->CreateDeviceOptions.DeviceModel = "PC Windows";
        g_orig_EOS_Connect_CreateDeviceId(Handle, &ctx->CreateDeviceOptions, ctx, OnCreateDeviceIdCallback);
    } else {
        DoDeviceIdLogin(ctx);
    }
}

static void EOS_CALL OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data) {
    if (!Data || !Data->ClientData) return;
    auto* ctx = static_cast<DeviceIdAuthContext*>(Data->ClientData);

    int res = (int)Data->ResultCode;
    RFLOG(Auth, "[Passthrough] Stage 1 EOS_Connect_CreateDeviceId result = %d", res);

    // EOS_Success (0) or EOS_DuplicateNotAllowed (24)
    if (res == (int)EOS_EResult::EOS_Success || res == (int)EOS_EResult::EOS_DuplicateNotAllowed || res == 24) {
        DoDeviceIdLogin(ctx);
    } else {
        RFLOG(Auth, "[Passthrough] EOS_Connect_CreateDeviceId failed with error %d", res);
        if (ctx->OriginalCompletionDelegate) {
            EOS_Connect_LoginCallbackInfo failInfo{};
            failInfo.ResultCode = Data->ResultCode;
            failInfo.ClientData = ctx->OriginalClientData;
            ctx->OriginalCompletionDelegate(&failInfo);
        }
        delete ctx;
    }
}

static void DoDeviceIdLogin(DeviceIdAuthContext* ctx) {
    RFLOG(Auth, "[Passthrough] Stage 2: Logging in with DeviceId ('%s')", ctx->DisplayName.c_str());

    ctx->DeviceCreds = {};
    ctx->DeviceCreds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    ctx->DeviceCreds.Type = (EOS_EExternalCredentialType)10; // EOS_ECT_DEVICEID_ACCESS_TOKEN
    ctx->DeviceCreds.Token = nullptr;

    ctx->UserLoginInfo = {};
    ctx->UserLoginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
    ctx->UserLoginInfo.DisplayName = ctx->DisplayName.c_str();
    ctx->UserLoginInfo.NsaIdToken = nullptr;

    ctx->LoginOptions = {};
    ctx->LoginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
    ctx->LoginOptions.Credentials = &ctx->DeviceCreds;
    ctx->LoginOptions.UserLoginInfo = &ctx->UserLoginInfo;

    g_orig_EOS_Connect_Login(ctx->Handle, &ctx->LoginOptions, ctx, OnDeviceIdLoginCallback);
}

static void EOS_CALL OnDeviceIdLoginCallback(const EOS_Connect_LoginCallbackInfo* Data) {
    if (!Data || !Data->ClientData) return;
    auto* ctx = static_cast<DeviceIdAuthContext*>(Data->ClientData);

    int res = (int)Data->ResultCode;
    RFLOG(Auth, "[Passthrough] Stage 2 EOS_Connect_Login result = %d", res);

    if (res == (int)EOS_EResult::EOS_Success) {
        RFLOG(Auth, "[Passthrough] EOS_Connect_Login succeeded via DeviceId! LocalUserId = %p", Data->LocalUserId);
        g_localProductUserId = Data->LocalUserId;
        {
            std::lock_guard<std::mutex> lg(g_puidMutex);
            g_puidToSteamId[g_localProductUserId] = GetPlayerSteamId();
        }
        if (ctx->OriginalCompletionDelegate) {
            EOS_Connect_LoginCallbackInfo cbInfo = *Data;
            cbInfo.ClientData = ctx->OriginalClientData;
            ctx->OriginalCompletionDelegate(&cbInfo);
        }
        delete ctx;
    } else if (res == (int)EOS_EResult::EOS_InvalidUser || res == 3) {
        RFLOG(Auth, "[Passthrough] User not registered yet (EOS_InvalidUser, 3). Advancing to Stage 3: EOS_Connect_CreateUser...");
        if (g_orig_EOS_Connect_CreateUser && Data->ContinuanceToken) {
            ctx->ContinuanceToken = Data->ContinuanceToken;
            ctx->CreateUserOptions = {};
            ctx->CreateUserOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
            ctx->CreateUserOptions.ContinuanceToken = ctx->ContinuanceToken;
            g_orig_EOS_Connect_CreateUser(ctx->Handle, &ctx->CreateUserOptions, ctx, OnCreateUserCallback);
            return;
        } else {
            RFLOG(Auth, "[Passthrough] Cannot create user: g_orig_EOS_Connect_CreateUser missing or null ContinuanceToken");
            if (ctx->OriginalCompletionDelegate) {
                EOS_Connect_LoginCallbackInfo failInfo = *Data;
                failInfo.ClientData = ctx->OriginalClientData;
                ctx->OriginalCompletionDelegate(&failInfo);
            }
            delete ctx;
        }
    } else {
        RFLOG(Auth, "[Passthrough] EOS_Connect_Login failed with result %d", res);
        if (ctx->OriginalCompletionDelegate) {
            EOS_Connect_LoginCallbackInfo failInfo = *Data;
            failInfo.ClientData = ctx->OriginalClientData;
            ctx->OriginalCompletionDelegate(&failInfo);
        }
        delete ctx;
    }
}

static void EOS_CALL OnCreateUserCallback(const EOS_Connect_CreateUserCallbackInfo* Data) {
    if (!Data || !Data->ClientData) return;
    auto* ctx = static_cast<DeviceIdAuthContext*>(Data->ClientData);

    int res = (int)Data->ResultCode;
    RFLOG(Auth, "[Passthrough] Stage 3 EOS_Connect_CreateUser result = %d", res);

    if (res == (int)EOS_EResult::EOS_Success) {
        RFLOG(Auth, "[Passthrough] User created successfully! Minted LocalUserId = %p", Data->LocalUserId);
        g_localProductUserId = Data->LocalUserId;
        {
            std::lock_guard<std::mutex> lg(g_puidMutex);
            g_puidToSteamId[g_localProductUserId] = GetPlayerSteamId();
        }
        if (ctx->OriginalCompletionDelegate) {
            EOS_Connect_LoginCallbackInfo loginInfo{};
            loginInfo.ResultCode = EOS_EResult::EOS_Success;
            loginInfo.ClientData = ctx->OriginalClientData;
            loginInfo.LocalUserId = Data->LocalUserId;
            loginInfo.ContinuanceToken = nullptr;
            ctx->OriginalCompletionDelegate(&loginInfo);
        }
        delete ctx;
    } else {
        RFLOG(Auth, "[Passthrough] EOS_Connect_CreateUser failed with result %d", res);
        if (ctx->OriginalCompletionDelegate) {
            EOS_Connect_LoginCallbackInfo failInfo{};
            failInfo.ResultCode = Data->ResultCode;
            failInfo.ClientData = ctx->OriginalClientData;
            ctx->OriginalCompletionDelegate(&failInfo);
        }
        delete ctx;
    }
}

// ---------------------------------------------------------------------------
// External Account Synthesis (OnlineSubsystemEOS)
// ---------------------------------------------------------------------------
struct SynthAccountBlock {
    EOS_Connect_ExternalAccountInfo Info;
    std::string DisplayName;
    std::string AccountId;
};

static std::mutex g_synthMutex;
static std::map<EOS_Connect_ExternalAccountInfo*, SynthAccountBlock*> g_synthBlocks;

EOS_DECLARE_FUNC(uint32_t) Passthrough_EOS_Connect_GetProductUserExternalAccountCount(
    EOS_HConnect Handle,
    const EOS_Connect_GetProductUserExternalAccountCountOptions* Options
) {
    uint32_t realCount = 0;
    if (g_orig_EOS_Connect_GetProductUserExternalAccountCount) {
        realCount = g_orig_EOS_Connect_GetProductUserExternalAccountCount(Handle, Options);
    }
    if (realCount == 0 && Options && Options->TargetUserId) {
        if (g_localProductUserId && Options->TargetUserId != g_localProductUserId) {
            return 0u;
        }
        return 1u;
    }
    return realCount;
}

EOS_DECLARE_FUNC(EOS_EResult) Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountType(
    EOS_HConnect Handle,
    const EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions* Options,
    EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo
) {
    if (!Options || !OutExternalAccountInfo) return EOS_EResult::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;

    if (g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountType) {
        EOS_EResult res = g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountType(Handle, Options, OutExternalAccountInfo);
        if (res == EOS_EResult::EOS_Success && *OutExternalAccountInfo != nullptr) {
            return EOS_EResult::EOS_Success;
        }
    }

    if (g_localProductUserId && Options->TargetUserId != g_localProductUserId) {
        return EOS_EResult::EOS_NotFound;
    }

    if (Options->AccountIdType == EAT::EOS_EAT_STEAM || (int)Options->AccountIdType == 1) {
        auto* block = new SynthAccountBlock();
        block->DisplayName = GetPlayerDisplayName();
        block->AccountId   = GetPlayerSteamId();

        std::memset(&block->Info, 0, sizeof(block->Info));
        block->Info.ApiVersion    = EOS_CONNECT_EXTERNALACCOUNTINFO_API_LATEST;
        block->Info.ProductUserId = Options->TargetUserId;
        block->Info.DisplayName   = block->DisplayName.c_str();
        block->Info.AccountId     = block->AccountId.c_str();
        block->Info.AccountIdType = EAT::EOS_EAT_STEAM;
        block->Info.LastLoginTime = UnixSeconds();

        {
            std::lock_guard<std::mutex> lock(g_synthMutex);
            g_synthBlocks[&block->Info] = block;
        }
        *OutExternalAccountInfo = &block->Info;
        RFLOG(Auth, "[Passthrough] Synthesized EOS_EAT_STEAM account for PUID %p: Name='%s', SteamID=%s",
              Options->TargetUserId, block->DisplayName.c_str(), block->AccountId.c_str());
        return EOS_EResult::EOS_Success;
    }

    return EOS_EResult::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) Passthrough_EOS_Connect_CopyProductUserExternalAccountByIndex(
    EOS_HConnect Handle,
    const EOS_Connect_CopyProductUserExternalAccountByIndexOptions* Options,
    EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo
) {
    if (!Options || !OutExternalAccountInfo) return EOS_EResult::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;

    if (g_orig_EOS_Connect_CopyProductUserExternalAccountByIndex) {
        EOS_EResult res = g_orig_EOS_Connect_CopyProductUserExternalAccountByIndex(Handle, Options, OutExternalAccountInfo);
        if (res == EOS_EResult::EOS_Success && *OutExternalAccountInfo != nullptr) {
            return EOS_EResult::EOS_Success;
        }
    }

    if (Options->ExternalAccountInfoIndex == 0) {
        EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions typeOpts{};
        typeOpts.ApiVersion = 1;
        typeOpts.TargetUserId = Options->TargetUserId;
        typeOpts.AccountIdType = EAT::EOS_EAT_STEAM;
        return Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountType(Handle, &typeOpts, OutExternalAccountInfo);
    }
    return EOS_EResult::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountId(
    EOS_HConnect Handle,
    const EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions* Options,
    EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo
) {
    if (!Options || !OutExternalAccountInfo) return EOS_EResult::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;

    if (g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountId) {
        EOS_EResult res = g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountId(Handle, Options, OutExternalAccountInfo);
        if (res == EOS_EResult::EOS_Success && *OutExternalAccountInfo != nullptr) {
            return EOS_EResult::EOS_Success;
        }
    }

    if (Options->AccountId && (std::string(Options->AccountId) == GetPlayerSteamId())) {
        EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions typeOpts{};
        typeOpts.ApiVersion = 1;
        typeOpts.TargetUserId = Options->TargetUserId;
        typeOpts.AccountIdType = EAT::EOS_EAT_STEAM;
        return Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountType(Handle, &typeOpts, OutExternalAccountInfo);
    }
    return EOS_EResult::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) Passthrough_EOS_Connect_CopyProductUserInfo(
    EOS_HConnect Handle,
    const EOS_Connect_CopyProductUserInfoOptions* Options,
    EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo
) {
    if (!Options || !OutExternalAccountInfo) return EOS_EResult::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;

    if (g_orig_EOS_Connect_CopyProductUserInfo) {
        EOS_EResult res = g_orig_EOS_Connect_CopyProductUserInfo(Handle, Options, OutExternalAccountInfo);
        if (res == EOS_EResult::EOS_Success && *OutExternalAccountInfo != nullptr) {
            return EOS_EResult::EOS_Success;
        }
    }

    EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions typeOpts{};
    typeOpts.ApiVersion = 1;
    typeOpts.TargetUserId = Options->TargetUserId;
    typeOpts.AccountIdType = EAT::EOS_EAT_STEAM;
    return Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountType(Handle, &typeOpts, OutExternalAccountInfo);
}

EOS_DECLARE_FUNC(void) Passthrough_EOS_Connect_ExternalAccountInfo_Release(EOS_Connect_ExternalAccountInfo* ExternalAccountInfo) {
    if (!ExternalAccountInfo) return;
    {
        std::lock_guard<std::mutex> lock(g_synthMutex);
        auto it = g_synthBlocks.find(ExternalAccountInfo);
        if (it != g_synthBlocks.end()) {
            delete it->second;
            g_synthBlocks.erase(it);
            return;
        }
    }
    if (g_orig_EOS_Connect_ExternalAccountInfo_Release) {
        g_orig_EOS_Connect_ExternalAccountInfo_Release(ExternalAccountInfo);
    }
}

// ---------------------------------------------------------------------------
// VerifyIdToken Interception & External Account Synthesis (7 Days to Die / Unity EOS)
// ---------------------------------------------------------------------------
struct VerifyIdTokenContext {
    void* OriginalClientData = nullptr;
    EOS_Connect_OnVerifyIdTokenCallback OriginalCompletionDelegate = nullptr;
    EOS_ProductUserId TargetPuid = nullptr;
    std::string SteamId;
    std::string Platform;
    std::string DeviceType;
};

static void EOS_CALL OnVerifyIdTokenCallback(const EOS_Connect_VerifyIdTokenCallbackInfo* Data) {
    if (!Data || !Data->ClientData) return;
    auto* ctx = static_cast<VerifyIdTokenContext*>(Data->ClientData);

    EOS_Connect_VerifyIdTokenCallbackInfo info = *Data;
    info.ClientData = ctx->OriginalClientData;

    if (info.ProductUserId && !ctx->SteamId.empty()) {
        std::lock_guard<std::mutex> lg(g_puidMutex);
        g_puidToSteamId[info.ProductUserId] = ctx->SteamId;
    }

    // Check if account info is missing or operation failed.
    // When DeviceIdAuth is used (or if genuine EOS SDK returned bIsAccountInfoPresent == EOS_FALSE or ResultCode != Success),
    // synthesize the external Steam account info so that Unity / EOS games (e.g. 7 Days to Die) can complete platform user mapping.
    if (!info.bIsAccountInfoPresent || !info.AccountId || !info.AccountId[0] || info.ResultCode != EOS_EResult::EOS_Success) {
        RFLOG(Auth, "[Passthrough] Synthesizing external Steam account info in VerifyIdToken callback (ResultCode was %d, bIsAccountInfoPresent was %d)",
              (int)info.ResultCode, (int)info.bIsAccountInfoPresent);
        info.ResultCode = EOS_EResult::EOS_Success;
        info.bIsAccountInfoPresent = EOS_TRUE;
        info.AccountIdType = EAT::EOS_EAT_STEAM;
        info.AccountId = ctx->SteamId.c_str();
        info.Platform = ctx->Platform.c_str();
        info.DeviceType = ctx->DeviceType.c_str();
        if (ctx->TargetPuid) {
            info.ProductUserId = ctx->TargetPuid;
        } else if (!info.ProductUserId && g_localProductUserId) {
            info.ProductUserId = g_localProductUserId;
        }
    }

    if (ctx->OriginalCompletionDelegate) {
        ctx->OriginalCompletionDelegate(&info);
    }
    delete ctx;
}

EOS_DECLARE_FUNC(void) Passthrough_EOS_Connect_VerifyIdToken(
    EOS_HConnect Handle,
    const EOS_Connect_VerifyIdTokenOptions* Options,
    void* ClientData,
    const EOS_Connect_OnVerifyIdTokenCallback CompletionDelegate
) {
    if (!CompletionDelegate) return;

    auto* ctx = new VerifyIdTokenContext();
    ctx->OriginalClientData = ClientData;
    ctx->OriginalCompletionDelegate = CompletionDelegate;
    ctx->TargetPuid = (Options && Options->IdToken) ? Options->IdToken->ProductUserId : g_localProductUserId;
    if (!ctx->TargetPuid) ctx->TargetPuid = g_localProductUserId;

    std::string sid = GetPlayerSteamId();
    if (ctx->TargetPuid) {
        std::lock_guard<std::mutex> lg(g_puidMutex);
        auto it = g_puidToSteamId.find(ctx->TargetPuid);
        if (it != g_puidToSteamId.end()) {
            sid = it->second;
        } else {
            g_puidToSteamId[ctx->TargetPuid] = sid;
        }
    }
    ctx->SteamId = sid;
    ctx->Platform = "STEAM";
    ctx->DeviceType = "PC";

    RFLOG(Auth, "[Passthrough] EOS_Connect_VerifyIdToken called for PUID %p (SteamID=%s)",
          ctx->TargetPuid, ctx->SteamId.c_str());

    if (g_orig_EOS_Connect_VerifyIdToken) {
        g_orig_EOS_Connect_VerifyIdToken(Handle, Options, ctx, OnVerifyIdTokenCallback);
    } else {
        EOS_Connect_VerifyIdTokenCallbackInfo synthInfo{};
        synthInfo.ResultCode = EOS_EResult::EOS_Success;
        synthInfo.ClientData = ctx;
        synthInfo.ProductUserId = ctx->TargetPuid;
        synthInfo.bIsAccountInfoPresent = EOS_TRUE;
        synthInfo.AccountIdType = EAT::EOS_EAT_STEAM;
        synthInfo.AccountId = ctx->SteamId.c_str();
        synthInfo.Platform = ctx->Platform.c_str();
        synthInfo.DeviceType = ctx->DeviceType.c_str();
        OnVerifyIdTokenCallback(&synthInfo);
    }
}

} // namespace

bool HasOriginalSdk() {
    return !FindOriginalSdkCandidate().empty();
}

bool InitialisePassthrough(Registrar& reg) {
    std::string sdkPath = FindOriginalSdkCandidate();
    if (sdkPath.empty()) {
        RFLOG(Core, "[Passthrough] Genuine EOSSDK_original.dll could not be found");
        return false;
    }

    g_hGenuineSdk = LoadLibraryA(sdkPath.c_str());
    if (!g_hGenuineSdk) {
        RFLOG(Core, "[Passthrough] Failed to load genuine SDK from '%s' (Win32 error %lu)",
              sdkPath.c_str(), GetLastError());
        return false;
    }

    int resolved = 0;
    for (int i = 0; i < ExportCount(); ++i) {
        const char* name = ExportName(i);
        FARPROC proc = GetProcAddress(g_hGenuineSdk, name);
        if (proc) {
            g_eosProcs[i] = (void*)proc;
            resolved++;
        }
    }

    RFLOG(Core, "[Passthrough] Genuine EOS SDK loaded from '%s': %d / %d exports forwarded",
          sdkPath.c_str(), resolved, ExportCount());

    g_orig_EOS_Connect_Login = (EOS_Connect_Login_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_Login");
    g_orig_EOS_Connect_CreateDeviceId = (EOS_Connect_CreateDeviceId_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CreateDeviceId");
    g_orig_EOS_Connect_CreateUser = (EOS_Connect_CreateUser_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CreateUser");
    g_orig_EOS_Connect_GetProductUserExternalAccountCount = (EOS_Connect_GetProductUserExternalAccountCount_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_GetProductUserExternalAccountCount");
    g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountType = (EOS_Connect_CopyProductUserExternalAccountByAccountType_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CopyProductUserExternalAccountByAccountType");
    g_orig_EOS_Connect_CopyProductUserExternalAccountByIndex = (EOS_Connect_CopyProductUserExternalAccountByIndex_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CopyProductUserExternalAccountByIndex");
    g_orig_EOS_Connect_CopyProductUserExternalAccountByAccountId = (EOS_Connect_CopyProductUserExternalAccountByAccountId_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CopyProductUserExternalAccountByAccountId");
    g_orig_EOS_Connect_CopyProductUserInfo = (EOS_Connect_CopyProductUserInfo_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_CopyProductUserInfo");
    g_orig_EOS_Connect_ExternalAccountInfo_Release = (EOS_Connect_ExternalAccountInfo_Release_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_ExternalAccountInfo_Release");
    g_orig_EOS_Connect_VerifyIdToken = (EOS_Connect_VerifyIdToken_Fn)GetProcAddress(g_hGenuineSdk, "EOS_Connect_VerifyIdToken");

    // Hook EOS_Connect_Login for DeviceIdAuth
    reg.Bind("EOS_Connect_Login", (void*)&Passthrough_EOS_Connect_Login);

    // Hook External Account synthesis for Unreal Engine OnlineSubsystemEOS
    reg.Bind("EOS_Connect_GetProductUserExternalAccountCount", (void*)&Passthrough_EOS_Connect_GetProductUserExternalAccountCount);
    reg.Bind("EOS_Connect_CopyProductUserExternalAccountByAccountType", (void*)&Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountType);
    reg.Bind("EOS_Connect_CopyProductUserExternalAccountByIndex", (void*)&Passthrough_EOS_Connect_CopyProductUserExternalAccountByIndex);
    reg.Bind("EOS_Connect_CopyProductUserExternalAccountByAccountId", (void*)&Passthrough_EOS_Connect_CopyProductUserExternalAccountByAccountId);
    reg.Bind("EOS_Connect_CopyProductUserInfo", (void*)&Passthrough_EOS_Connect_CopyProductUserInfo);
    reg.Bind("EOS_Connect_ExternalAccountInfo_Release", (void*)&Passthrough_EOS_Connect_ExternalAccountInfo_Release);

    // Hook VerifyIdToken for Unity / EOS platform user mapping (7 Days to Die, etc.)
    reg.Bind("EOS_Connect_VerifyIdToken", (void*)&Passthrough_EOS_Connect_VerifyIdToken);

    return true;
}

} // namespace refix
