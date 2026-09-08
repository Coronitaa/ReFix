// ReFix EOS v3 - EOS_Connect_*.
//
// Connect is where a title turns a platform credential (a Steam session ticket,
// a device id, ...) into a ProductUserId. Everything downstream - lobbies,
// sessions, P2P - is keyed on the id produced here, so this interface has two
// jobs and both matter:
//
//   * derive an id that is stable for a given player and identical on every
//     machine, so peers agree on who is who;
//   * describe that player's external account with byte-exact SDK structs.
//     Getting EOS_Connect_ExternalAccountInfo wrong is fatal: the caller walks
//     the struct and dereferences the pointers it finds.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"
#include "../net/lobby_directory.h"

using namespace refix;

namespace {

// Allocated per Copy* call and handed to the title; released through
// EOS_Connect_ExternalAccountInfo_Release. The strings live next to the struct
// so a single free reclaims everything.
struct ExternalAccountInfoBlock {
    EOS_Connect_ExternalAccountInfo Info;
    std::string                     DisplayName;
    std::string                     AccountId;
};

std::mutex                                       g_blocksMutex;
std::map<EOS_Connect_ExternalAccountInfo*, ExternalAccountInfoBlock*> g_blocks;

EOS_Connect_ExternalAccountInfo* MakeExternalAccountInfo(const UserRecord& user) {
    auto* block = new ExternalAccountInfoBlock();
    block->DisplayName = user.External.DisplayName.empty() ? user.DisplayName
                                                           : user.External.DisplayName;
    block->AccountId   = user.External.AccountId;

    std::memset(&block->Info, 0, sizeof(block->Info));
    block->Info.ApiVersion    = EOS_CONNECT_EXTERNALACCOUNTINFO_API_LATEST;
    block->Info.ProductUserId = IdRegistry::Get().Puid(user.Puid);
    block->Info.DisplayName   = block->DisplayName.c_str();
    block->Info.AccountId     = block->AccountId.c_str();
    block->Info.AccountIdType = user.External.Type;
    block->Info.LastLoginTime = user.External.LastLoginTime;

    std::lock_guard<std::mutex> lock(g_blocksMutex);
    g_blocks[&block->Info] = block;
    return &block->Info;
}

// The local player's external accounts. A Steam login has exactly one; a
// DeviceId login has none, and saying "none" is what stops a caller from
// walking off the end of the list.
uint32_t LocalExternalAccountCount() {
    return Identity::Get().LocalUser().External.Valid ? 1u : 0u;
}

const UserRecord* UserFor(EOS_ProductUserId id) {
    const char* s = IdRegistry::ToString(id);
    if (!s) return nullptr;
    return Identity::Get().FindByPuid(s);
}

} // namespace

// ---------------------------------------------------------------------------
// Login
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(void) EOS_Connect_Login(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, const EOS_Connect_OnLoginCallback CompletionDelegate) {
    auto fail = [&](EOS_EResult code, const char* why) {
        RFLOG(Auth, "EOS_Connect_Login rejected: %s", why);
        if (!CompletionDelegate) return;
        Dispatcher::Get().Post([CompletionDelegate, ClientData, code]() {
            EOS_Connect_LoginCallbackInfo info{};
            info.ResultCode        = code;
            info.ClientData        = ClientData;
            info.LocalUserId       = nullptr;
            info.ContinuanceToken  = nullptr;
            CompletionDelegate(&info);
        });
    };

    if (!Options || !Options->Credentials) { fail(ER::EOS_InvalidParameters, "no credentials"); return; }

    const EOS_Connect_Credentials* cred = Options->Credentials;
    const char* token = cred->Token;
    RFLOG(Auth, "EOS_Connect_Login: credentialType=%d tokenLength=%zu",
          (int)cred->Type, token ? std::strlen(token) : 0u);

    const UserRecord& user = Identity::Get().ResolveLocalUser((int32_t)cred->Type, token);
    EOS_ProductUserId puid = IdRegistry::Get().Puid(user.Puid);

    if (!puid) { fail(ER::EOS_InvalidAuth, "identity could not be derived"); return; }

    RFLOG(Auth, "EOS_Connect_Login: '%s' -> PUID=%s", user.DisplayName.c_str(), user.Puid.c_str());
    LobbyDirectory::Get().SetLocalPuid(user.Puid);
    LobbyDirectory::Get().SetLocalName(user.DisplayName);

    if (CompletionDelegate) {
        Dispatcher::Get().Post([CompletionDelegate, ClientData, puid]() {
            EOS_Connect_LoginCallbackInfo info{};
            info.ResultCode       = ER::EOS_Success;
            info.ClientData       = ClientData;
            info.LocalUserId      = puid;
            info.ContinuanceToken = nullptr;
            CompletionDelegate(&info);
        });
    }

    // Titles that registered for login-status changes expect one after a login.
    Dispatcher::Get().Broadcast<EOS_Connect_LoginStatusChangedCallbackInfo>(
        NotifyKind::ConnectLoginStatusChanged,
        [puid](void* clientData) {
            EOS_Connect_LoginStatusChangedCallbackInfo info{};
            info.ClientData        = clientData;
            info.LocalUserId       = puid;
            info.PreviousStatus    = ELS::EOS_LS_NotLoggedIn;
            info.CurrentStatus     = ELS::EOS_LS_LoggedIn;
            return info;
        });
}

EOS_DECLARE_FUNC(void) EOS_Connect_Logout(EOS_HConnect Handle, const EOS_Connect_LogoutOptions* Options, void* ClientData, const EOS_Connect_OnLogoutCallback CompletionDelegate) {
    RFLOG(Auth, "EOS_Connect_Logout");
    if (!CompletionDelegate) return;
    EOS_ProductUserId puid = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, puid]() {
        EOS_Connect_LogoutCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = puid;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Connect_CreateUser(EOS_HConnect Handle, const EOS_Connect_CreateUserOptions* Options, void* ClientData, const EOS_Connect_OnCreateUserCallback CompletionDelegate) {
    // The account always exists as far as the emulator is concerned; report the
    // id the title would have received from a real account creation.
    EOS_ProductUserId puid = Identity::Get().LocalPuid();
    RFLOG(Auth, "EOS_Connect_CreateUser -> %s", IdRegistry::ToString(puid));
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, puid]() {
        EOS_Connect_CreateUserCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = puid;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Connect_CreateDeviceId(EOS_HConnect Handle, const EOS_Connect_CreateDeviceIdOptions* Options, void* ClientData, const EOS_Connect_OnCreateDeviceIdCallback CompletionDelegate) {
    RFLOG(Auth, "EOS_Connect_CreateDeviceId(model='%s')",
          (Options && Options->DeviceModel) ? Options->DeviceModel : "");
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData]() {
        EOS_Connect_CreateDeviceIdCallbackInfo info{};
        info.ResultCode = ER::EOS_Success;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Connect_DeleteDeviceId(EOS_HConnect Handle, const EOS_Connect_DeleteDeviceIdOptions* Options, void* ClientData, const EOS_Connect_OnDeleteDeviceIdCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData]() {
        EOS_Connect_DeleteDeviceIdCallbackInfo info{};
        info.ResultCode = ER::EOS_Success;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

// ---------------------------------------------------------------------------
// Local user state
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(int32_t) EOS_Connect_GetLoggedInUsersCount(EOS_HConnect Handle) {
    return Identity::Get().HasLocalUser() ? 1 : 0;
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_Connect_GetLoggedInUserByIndex(EOS_HConnect Handle, int32_t Index) {
    if (Index != 0 || !Identity::Get().HasLocalUser()) return nullptr;
    return Identity::Get().LocalPuid();
}

EOS_DECLARE_FUNC(EOS_ELoginStatus) EOS_Connect_GetLoginStatus(EOS_HConnect Handle, EOS_ProductUserId LocalUserId) {
    if (!Identity::Get().HasLocalUser()) return ELS::EOS_LS_NotLoggedIn;
    return LocalUserId == Identity::Get().LocalPuid() ? ELS::EOS_LS_LoggedIn : ELS::EOS_LS_NotLoggedIn;
}

// ---------------------------------------------------------------------------
// External accounts
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(uint32_t) EOS_Connect_GetProductUserExternalAccountCount(EOS_HConnect Handle, const EOS_Connect_GetProductUserExternalAccountCountOptions* Options) {
    if (!Options) return 0;
    const UserRecord* user = UserFor(Options->TargetUserId);
    uint32_t count = (user && user->External.Valid) ? 1u : 0u;
    RFLOG(Auth, "EOS_Connect_GetProductUserExternalAccountCount -> %u", count);
    return count;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Connect_CopyProductUserInfo(EOS_HConnect Handle, const EOS_Connect_CopyProductUserInfoOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!Options || !OutExternalAccountInfo) return ER::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;
    const UserRecord* user = UserFor(Options->TargetUserId);
    if (!user) return ER::EOS_NotFound;
    *OutExternalAccountInfo = MakeExternalAccountInfo(*user);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Connect_CopyProductUserExternalAccountByIndex(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByIndexOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!Options || !OutExternalAccountInfo) return ER::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;
    const UserRecord* user = UserFor(Options->TargetUserId);
    if (!user || !user->External.Valid) return ER::EOS_NotFound;
    // Exactly one external account exists; any other index is out of range.
    if (Options->ExternalAccountInfoIndex != 0) {
        RFLOG(Auth, "CopyProductUserExternalAccountByIndex(%u) -> NotFound (only 1 account)",
              Options->ExternalAccountInfoIndex);
        return ER::EOS_NotFound;
    }
    *OutExternalAccountInfo = MakeExternalAccountInfo(*user);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Connect_CopyProductUserExternalAccountByAccountType(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!Options || !OutExternalAccountInfo) return ER::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;
    const UserRecord* user = UserFor(Options->TargetUserId);
    if (!user || !user->External.Valid) return ER::EOS_NotFound;
    if (user->External.Type != Options->AccountIdType) {
        RFLOG(Auth, "CopyProductUserExternalAccountByAccountType(%d) -> NotFound (account is type %d)",
              (int)Options->AccountIdType, (int)user->External.Type);
        return ER::EOS_NotFound;
    }
    *OutExternalAccountInfo = MakeExternalAccountInfo(*user);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Connect_CopyProductUserExternalAccountByAccountId(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByAccountIdOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo) {
    if (!Options || !OutExternalAccountInfo || !Options->AccountId) return ER::EOS_InvalidParameters;
    *OutExternalAccountInfo = nullptr;
    const UserRecord* user = UserFor(Options->TargetUserId);
    if (!user || !user->External.Valid) return ER::EOS_NotFound;
    if (user->External.AccountId != Options->AccountId) return ER::EOS_NotFound;
    *OutExternalAccountInfo = MakeExternalAccountInfo(*user);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Connect_ExternalAccountInfo_Release(EOS_Connect_ExternalAccountInfo* ExternalAccountInfo) {
    if (!ExternalAccountInfo) return;
    ExternalAccountInfoBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_blocksMutex);
        auto it = g_blocks.find(ExternalAccountInfo);
        if (it == g_blocks.end()) return;   // not ours - ignore rather than crash
        block = it->second;
        g_blocks.erase(it);
    }
    delete block;
}

// ---------------------------------------------------------------------------
// Id mappings
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(void) EOS_Connect_QueryProductUserIdMappings(EOS_HConnect Handle, const EOS_Connect_QueryProductUserIdMappingsOptions* Options, void* ClientData, const EOS_Connect_OnQueryProductUserIdMappingsCallback CompletionDelegate) {
    // Mappings are already known locally: peers announce their ids over the
    // ReFix transport, so there is nothing to fetch.
    EOS_ProductUserId local = Options ? Options->LocalUserId : nullptr;
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local]() {
        EOS_Connect_QueryProductUserIdMappingsCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = local;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Connect_GetProductUserIdMapping(EOS_HConnect Handle, const EOS_Connect_GetProductUserIdMappingOptions* Options, char* OutBuffer, int32_t* InOutBufferLength) {
    if (!Options || !InOutBufferLength) return ER::EOS_InvalidParameters;
    const UserRecord* user = UserFor(Options->TargetProductUserId);
    if (!user || !user->External.Valid) return ER::EOS_NotFound;
    if (user->External.Type != Options->AccountIdType) return ER::EOS_NotFound;

    const std::string& value = user->External.AccountId;
    int32_t needed = (int32_t)value.size() + 1;
    if (!OutBuffer || *InOutBufferLength < needed) { *InOutBufferLength = needed; return ER::EOS_LimitExceeded; }
    std::memcpy(OutBuffer, value.c_str(), needed);
    *InOutBufferLength = needed;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Connect_QueryExternalAccountMappings(EOS_HConnect Handle, const EOS_Connect_QueryExternalAccountMappingsOptions* Options, void* ClientData, const EOS_Connect_OnQueryExternalAccountMappingsCallback CompletionDelegate) {
    EOS_ProductUserId local = Options ? Options->LocalUserId : nullptr;
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local]() {
        EOS_Connect_QueryExternalAccountMappingsCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = local;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_Connect_GetExternalAccountMapping(EOS_HConnect Handle, const EOS_Connect_GetExternalAccountMappingsOptions* Options) {
    if (!Options || !Options->TargetExternalUserId) return nullptr;
    const UserRecord& local = Identity::Get().LocalUser();
    if (local.External.Valid && local.External.AccountId == Options->TargetExternalUserId)
        return Identity::Get().LocalPuid();

    // Any Steam account maps to the ProductUserId that account derives for
    // itself, so a title can name a friend before ever meeting them online.
    if (Options->AccountIdType == EAT::EOS_EAT_STEAM) {
        const std::string external = Options->TargetExternalUserId;
        if (!external.empty() && external.find_first_not_of("0123456789") == std::string::npos) {
            // Record what we know about them too, so the title can go on to ask
            // that ProductUserId about its external account and get an answer.
            const UserRecord& peer =
                Identity::Get().RememberExternalAccount(external, EAT::EOS_EAT_STEAM, "");
            return IdRegistry::Get().Puid(peer.Puid);
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(EOS_NotificationId) EOS_Connect_AddNotifyLoginStatusChanged(EOS_HConnect Handle, const EOS_Connect_AddNotifyLoginStatusChangedOptions* Options, void* ClientData, const EOS_Connect_OnLoginStatusChangedCallback Notification) {
    return Dispatcher::Get().Register(NotifyKind::ConnectLoginStatusChanged, (void*)Notification, ClientData);
}

EOS_DECLARE_FUNC(void) EOS_Connect_RemoveNotifyLoginStatusChanged(EOS_HConnect Handle, EOS_NotificationId InId) {
    Dispatcher::Get().Unregister(InId);
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_Connect_AddNotifyAuthExpiration(EOS_HConnect Handle, const EOS_Connect_AddNotifyAuthExpirationOptions* Options, void* ClientData, const EOS_Connect_OnAuthExpirationCallback Notification) {
    // Local credentials never expire, so this handler is registered and simply
    // never fires - which is the correct behaviour, not a stub.
    return Dispatcher::Get().Register(NotifyKind::ConnectAuthExpiration, (void*)Notification, ClientData);
}

EOS_DECLARE_FUNC(void) EOS_Connect_RemoveNotifyAuthExpiration(EOS_HConnect Handle, EOS_NotificationId InId) {
    Dispatcher::Get().Unregister(InId);
}

namespace refix {

void RegisterConnectApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_Connect_Login);
    REFIX_BIND(reg, EOS_Connect_Logout);
    REFIX_BIND(reg, EOS_Connect_CreateUser);
    REFIX_BIND(reg, EOS_Connect_CreateDeviceId);
    REFIX_BIND(reg, EOS_Connect_DeleteDeviceId);
    REFIX_BIND(reg, EOS_Connect_GetLoggedInUsersCount);
    REFIX_BIND(reg, EOS_Connect_GetLoggedInUserByIndex);
    REFIX_BIND(reg, EOS_Connect_GetLoginStatus);
    REFIX_BIND(reg, EOS_Connect_GetProductUserExternalAccountCount);
    REFIX_BIND(reg, EOS_Connect_CopyProductUserInfo);
    REFIX_BIND(reg, EOS_Connect_CopyProductUserExternalAccountByIndex);
    REFIX_BIND(reg, EOS_Connect_CopyProductUserExternalAccountByAccountType);
    REFIX_BIND(reg, EOS_Connect_CopyProductUserExternalAccountByAccountId);
    REFIX_BIND(reg, EOS_Connect_ExternalAccountInfo_Release);
    REFIX_BIND(reg, EOS_Connect_QueryProductUserIdMappings);
    REFIX_BIND(reg, EOS_Connect_GetProductUserIdMapping);
    REFIX_BIND(reg, EOS_Connect_QueryExternalAccountMappings);
    REFIX_BIND(reg, EOS_Connect_GetExternalAccountMapping);
    REFIX_BIND(reg, EOS_Connect_AddNotifyLoginStatusChanged);
    REFIX_BIND(reg, EOS_Connect_RemoveNotifyLoginStatusChanged);
    REFIX_BIND(reg, EOS_Connect_AddNotifyAuthExpiration);
    REFIX_BIND(reg, EOS_Connect_RemoveNotifyAuthExpiration);
}

} // namespace refix
