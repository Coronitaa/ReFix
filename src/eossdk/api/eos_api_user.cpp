// ReFix EOS v3 - EOS_Auth_*, EOS_UserInfo_*, EOS_Friends_*, EOS_Presence_*.
//
// These interfaces describe *people*. Without an Epic account service behind us
// the honest answer to most Epic-account questions is "there is no such
// account", and saying so plainly is what keeps a title on the Connect path it
// can actually complete. Presence, by contrast, is fully modelled: Redpoint
// carries its "join this session" payload in the presence join info, so it has
// to round-trip exactly.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"

using namespace refix;

// ===========================================================================
// Auth - Epic accounts
// ===========================================================================
EOS_DECLARE_FUNC(void) EOS_Auth_Login(EOS_HAuth Handle, const EOS_Auth_LoginOptions* Options, void* ClientData, const EOS_Auth_OnLoginCallback CompletionDelegate) {
    // There is no Epic account service in the loop. Reporting the failure the
    // real SDK would report keeps titles on their platform-credential path
    // instead of leaving them waiting on a login that can never complete.
    RFLOG(Auth, "EOS_Auth_Login: no Epic account service available -> EOS_InvalidAuth");
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData]() {
        EOS_Auth_LoginCallbackInfo info{};
        info.ResultCode = ER::EOS_InvalidAuth;
        info.ClientData = ClientData;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Auth_Logout(EOS_HAuth Handle, const EOS_Auth_LogoutOptions* Options, void* ClientData, const EOS_Auth_OnLogoutCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId local = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local]() {
        EOS_Auth_LogoutCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = local;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(int32_t) EOS_Auth_GetLoggedInAccountsCount(EOS_HAuth Handle) { return 0; }
EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_Auth_GetLoggedInAccountByIndex(EOS_HAuth Handle, int32_t Index) { return nullptr; }
EOS_DECLARE_FUNC(EOS_ELoginStatus) EOS_Auth_GetLoginStatus(EOS_HAuth Handle, EOS_EpicAccountId LocalUserId) { return ELS::EOS_LS_NotLoggedIn; }
EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_Auth_GetSelectedAccountId(EOS_HAuth Handle, const EOS_EpicAccountId LocalUserId, EOS_EpicAccountId* OutSelectedAccountId) {
    if (OutSelectedAccountId) *OutSelectedAccountId = nullptr;
    return nullptr;
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_Auth_AddNotifyLoginStatusChanged(EOS_HAuth Handle, const EOS_Auth_AddNotifyLoginStatusChangedOptions* Options, void* ClientData, const EOS_Auth_OnLoginStatusChangedCallback Notification) {
    return Dispatcher::Get().Register(NotifyKind::AuthLoginStatusChanged, (void*)Notification, ClientData);
}
EOS_DECLARE_FUNC(void) EOS_Auth_RemoveNotifyLoginStatusChanged(EOS_HAuth Handle, EOS_NotificationId InId) {
    Dispatcher::Get().Unregister(InId);
}

// ===========================================================================
// UserInfo
// ===========================================================================
EOS_DECLARE_FUNC(void) EOS_UserInfo_QueryUserInfo(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoOptions* Options, void* ClientData, const EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId local  = Options ? Options->LocalUserId  : nullptr;
    EOS_EpicAccountId target = Options ? Options->TargetUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local, target]() {
        EOS_UserInfo_QueryUserInfoCallbackInfo info{};
        info.ResultCode   = ER::EOS_NotFound;   // no Epic account directory
        info.ClientData   = ClientData;
        info.LocalUserId  = local;
        info.TargetUserId = target;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_UserInfo_CopyUserInfo(EOS_HUserInfo Handle, const EOS_UserInfo_CopyUserInfoOptions* Options, EOS_UserInfo** OutUserInfo) {
    if (OutUserInfo) *OutUserInfo = nullptr;
    return ER::EOS_NotFound;
}

EOS_DECLARE_FUNC(void) EOS_UserInfo_Release(EOS_UserInfo* UserInfo) { }

// ===========================================================================
// Friends
// ===========================================================================
EOS_DECLARE_FUNC(void) EOS_Friends_QueryFriends(EOS_HFriends Handle, const EOS_Friends_QueryFriendsOptions* Options, void* ClientData, const EOS_Friends_OnQueryFriendsCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId local = Options ? Options->LocalUserId : nullptr;
    // An empty friends list is a complete, successful answer.
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local]() {
        EOS_Friends_QueryFriendsCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = local;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(int32_t) EOS_Friends_GetFriendsCount(EOS_HFriends Handle, const EOS_Friends_GetFriendsCountOptions* Options) { return 0; }
EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_Friends_GetFriendAtIndex(EOS_HFriends Handle, const EOS_Friends_GetFriendAtIndexOptions* Options) { return nullptr; }
EOS_DECLARE_FUNC(EOS_EFriendsStatus) EOS_Friends_GetStatus(EOS_HFriends Handle, const EOS_Friends_GetStatusOptions* Options) { return EOS_EFriendsStatus::EOS_FS_NotFriends; }
EOS_DECLARE_FUNC(int32_t) EOS_Friends_GetBlockedUsersCount(EOS_HFriends Handle, const EOS_Friends_GetBlockedUsersCountOptions* Options) { return 0; }
EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_Friends_GetBlockedUserAtIndex(EOS_HFriends Handle, const EOS_Friends_GetBlockedUserAtIndexOptions* Options) { return nullptr; }

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_Friends_AddNotifyFriendsUpdate(EOS_HFriends Handle, const EOS_Friends_AddNotifyFriendsUpdateOptions* Options, void* ClientData, const EOS_Friends_OnFriendsUpdateCallback FriendsUpdateHandler) {
    return Dispatcher::Get().Register(NotifyKind::FriendsUpdate, (void*)FriendsUpdateHandler, ClientData);
}
EOS_DECLARE_FUNC(void) EOS_Friends_RemoveNotifyFriendsUpdate(EOS_HFriends Handle, EOS_NotificationId NotificationId) {
    Dispatcher::Get().Unregister(NotificationId);
}
EOS_DECLARE_FUNC(EOS_NotificationId) EOS_Friends_AddNotifyBlockedUsersUpdate(EOS_HFriends Handle, const EOS_Friends_AddNotifyBlockedUsersUpdateOptions* Options, void* ClientData, const EOS_Friends_OnBlockedUsersUpdateCallback BlockedUsersUpdateHandler) {
    return Dispatcher::Get().Register(NotifyKind::BlockedUsersUpdate, (void*)BlockedUsersUpdateHandler, ClientData);
}
EOS_DECLARE_FUNC(void) EOS_Friends_RemoveNotifyBlockedUsersUpdate(EOS_HFriends Handle, EOS_NotificationId NotificationId) {
    Dispatcher::Get().Unregister(NotificationId);
}

namespace refix {

void RegisterAuthApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_Auth_Login);
    REFIX_BIND(reg, EOS_Auth_Logout);
    REFIX_BIND(reg, EOS_Auth_GetLoggedInAccountsCount);
    REFIX_BIND(reg, EOS_Auth_GetLoggedInAccountByIndex);
    REFIX_BIND(reg, EOS_Auth_GetLoginStatus);
    REFIX_BIND(reg, EOS_Auth_GetSelectedAccountId);
    REFIX_BIND(reg, EOS_Auth_AddNotifyLoginStatusChanged);
    REFIX_BIND(reg, EOS_Auth_RemoveNotifyLoginStatusChanged);
}

void RegisterUserApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_UserInfo_QueryUserInfo);
    REFIX_BIND(reg, EOS_UserInfo_CopyUserInfo);
    REFIX_BIND(reg, EOS_UserInfo_Release);

    REFIX_BIND(reg, EOS_Friends_QueryFriends);
    REFIX_BIND(reg, EOS_Friends_GetFriendsCount);
    REFIX_BIND(reg, EOS_Friends_GetFriendAtIndex);
    REFIX_BIND(reg, EOS_Friends_GetStatus);
    REFIX_BIND(reg, EOS_Friends_GetBlockedUsersCount);
    REFIX_BIND(reg, EOS_Friends_GetBlockedUserAtIndex);
    REFIX_BIND(reg, EOS_Friends_AddNotifyFriendsUpdate);
    REFIX_BIND(reg, EOS_Friends_RemoveNotifyFriendsUpdate);
    REFIX_BIND(reg, EOS_Friends_AddNotifyBlockedUsersUpdate);
    REFIX_BIND(reg, EOS_Friends_RemoveNotifyBlockedUsersUpdate);
}

} // namespace refix
