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
#include "../net/steam_backend.h"
#include "../core/refix_config.h"

using namespace refix;

namespace {

// The friends a title can see are the player's Steam friends. Each one is named
// by the EpicAccountId *they* would derive for themselves, so the id a title
// stores here is the same id that comes back from the network when that friend
// actually shows up in a lobby.
struct FriendEntry {
    uint64_t          SteamId = 0;
    std::string       Name;
    std::string       Eaid;
    std::string       Puid;
};

std::mutex                g_friendsMutex;
std::vector<FriendEntry>  g_friends;

void RefreshFriends() {
    std::vector<FriendEntry> list;
    for (const auto& f : SteamBackend::Get().Friends()) {
        FriendEntry e;
        e.SteamId = f.SteamId;
        // Steam has not necessarily cached this person's persona name yet (it
        // has been asked for, and a later refresh will have it). Until then the
        // title still needs something to draw, or the friends list is a column
        // of blank rows.
        e.Name = f.PersonaName;
        if (e.Name.empty())
            e.Name = "Steam user " + std::to_string(f.SteamId & 0xFFFFFFFFull);
        const std::string external = std::to_string(f.SteamId);
        const UserRecord& peer =
            Identity::Get().RememberExternalAccount(external, EAT::EOS_EAT_STEAM, e.Name);
        e.Eaid = peer.Eaid;
        e.Puid = peer.Puid;
        IdRegistry::Get().Eaid(e.Eaid);
        IdRegistry::Get().Puid(e.Puid);
        list.push_back(std::move(e));
    }
    std::lock_guard<std::mutex> lock(g_friendsMutex);
    g_friends.swap(list);
}

const FriendEntry* FindFriendByEaid(EOS_EpicAccountId id) {
    const char* text = IdRegistry::ToString(id);
    if (!text) return nullptr;
    std::lock_guard<std::mutex> lock(g_friendsMutex);
    for (const auto& f : g_friends) if (f.Eaid == text) return &f;
    return nullptr;
}

} // namespace

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
    RefreshFriends();
    if (!CompletionDelegate) return;
    EOS_EpicAccountId local  = Options ? Options->LocalUserId  : nullptr;
    EOS_EpicAccountId target = Options ? Options->TargetUserId : nullptr;
    const bool known = target && (FindFriendByEaid(target) != nullptr ||
                                  target == Identity::Get().LocalEaid());
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local, target, known]() {
        EOS_UserInfo_QueryUserInfoCallbackInfo info{};
        info.ResultCode   = known ? ER::EOS_Success : ER::EOS_NotFound;
        info.ClientData   = ClientData;
        info.LocalUserId  = local;
        info.TargetUserId = target;
        CompletionDelegate(&info);
    });
}

namespace {
// EOS_UserInfo owns its strings; keep them alive until the matching Release.
struct UserInfoBlock {
    EOS_UserInfo Info;
    std::string  DisplayName;
    std::string  Nickname;
};
std::mutex                                g_userInfoMutex;
std::map<EOS_UserInfo*, UserInfoBlock*>   g_userInfoBlocks;
} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_UserInfo_CopyUserInfo(EOS_HUserInfo Handle, const EOS_UserInfo_CopyUserInfoOptions* Options, EOS_UserInfo** OutUserInfo) {
    if (!Options || !OutUserInfo) return ER::EOS_InvalidParameters;
    *OutUserInfo = nullptr;

    std::string name;
    if (Options->TargetUserId == Identity::Get().LocalEaid()) {
        name = Identity::Get().LocalUser().DisplayName;
    } else if (const FriendEntry* f = FindFriendByEaid(Options->TargetUserId)) {
        name = f->Name;
    } else {
        return ER::EOS_NotFound;
    }

    auto* block = new UserInfoBlock();
    block->DisplayName = name;
    std::memset(&block->Info, 0, sizeof(block->Info));
    block->Info.ApiVersion  = EOS_USERINFO_COPYUSERINFO_API_LATEST;
    block->Info.UserId      = Options->TargetUserId;
    block->Info.DisplayName = block->DisplayName.c_str();
    {
        std::lock_guard<std::mutex> lock(g_userInfoMutex);
        g_userInfoBlocks[&block->Info] = block;
    }
    *OutUserInfo = &block->Info;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_UserInfo_Release(EOS_UserInfo* UserInfo) {
    if (!UserInfo) return;
    UserInfoBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_userInfoMutex);
        auto it = g_userInfoBlocks.find(UserInfo);
        if (it == g_userInfoBlocks.end()) return;
        block = it->second;
        g_userInfoBlocks.erase(it);
    }
    delete block;
}

// ===========================================================================
// Friends
// ===========================================================================
EOS_DECLARE_FUNC(void) EOS_Friends_QueryFriends(EOS_HFriends Handle, const EOS_Friends_QueryFriendsOptions* Options, void* ClientData, const EOS_Friends_OnQueryFriendsCallback CompletionDelegate) {
    RefreshFriends();
    if (!CompletionDelegate) return;
    EOS_EpicAccountId local = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, local]() {
        EOS_Friends_QueryFriendsCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = local;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(int32_t) EOS_Friends_GetFriendsCount(EOS_HFriends Handle, const EOS_Friends_GetFriendsCountOptions* Options) {
    std::lock_guard<std::mutex> lock(g_friendsMutex);
    return (int32_t)g_friends.size();
}

EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_Friends_GetFriendAtIndex(EOS_HFriends Handle, const EOS_Friends_GetFriendAtIndexOptions* Options) {
    if (!Options || Options->Index < 0) return nullptr;
    std::string eaid;
    {
        std::lock_guard<std::mutex> lock(g_friendsMutex);
        if ((size_t)Options->Index >= g_friends.size()) return nullptr;
        eaid = g_friends[Options->Index].Eaid;
    }
    return IdRegistry::Get().Eaid(eaid);
}

EOS_DECLARE_FUNC(EOS_EFriendsStatus) EOS_Friends_GetStatus(EOS_HFriends Handle, const EOS_Friends_GetStatusOptions* Options) {
    if (!Options) return EOS_EFriendsStatus::EOS_FS_NotFriends;
    return FindFriendByEaid(Options->TargetUserId) ? EOS_EFriendsStatus::EOS_FS_Friends
                                                   : EOS_EFriendsStatus::EOS_FS_NotFriends;
}
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
