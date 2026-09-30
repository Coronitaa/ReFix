// ReFix EOS v3 - asynchronous completion and notifications.
//
// Two rules from the real SDK that the emulator has to honour exactly:
//
//   * an async entry point never invokes its completion delegate inline - it
//     returns immediately and the delegate runs later, on the thread that calls
//     EOS_Platform_Tick. Titles rely on this; calling back inline re-enters the
//     game's online subsystem while it still holds locks.
//   * a notification handler registered with EOS_*_AddNotify* stays registered
//     until the matching RemoveNotify, and fires whenever the event occurs.
//
// Callback payloads are built inside the queued lambda, so any strings they
// point at are owned by that lambda and stay alive for the whole call.
#pragma once
#include "refix_common.h"
#include <functional>
#include <deque>

namespace refix {

// Notification categories, so a subsystem can broadcast to every handler that
// asked for a given event without knowing who registered.
enum class NotifyKind {
    LobbyUpdateReceived, LobbyMemberUpdateReceived, LobbyMemberStatusReceived,
    LobbyInviteReceived, LobbyInviteAccepted, LobbyInviteRejected,
    JoinLobbyAccepted, LeaveLobbyRequested, LobbyRtcRoomConnectionChanged,
    SessionInviteReceived, SessionInviteAccepted, JoinSessionAccepted,
    P2PIncomingConnectionRequest, P2PConnectionClosed, P2PConnectionEstablished,
    P2PIncomingPacketQueueFull,
    ConnectLoginStatusChanged, ConnectAuthExpiration,
    AuthLoginStatusChanged,
    PresenceChanged, FriendsUpdate, BlockedUsersUpdate,
    IntegratedPlatformUserLoginStatusChanged,
    UiDisplaySettingsUpdated,
    AchievementsUnlocked,
    Count
};

struct NotifyHandler {
    EOS_NotificationId Id = 0;
    NotifyKind         Kind = NotifyKind::Count;
    void*              Fn = nullptr;
    void*              ClientData = nullptr;
};

class Dispatcher {
public:
    static Dispatcher& Get();

    // Queue work to run during the next EOS_Platform_Tick.
    void Post(std::function<void()> fn);

    // Queue work to run on the first tick at least `delayMs` from now. Used for
    // operations that are genuinely not instantaneous - a lobby search has to
    // leave the network a moment to answer before it reports "no results".
    void PostAfter(uint32_t delayMs, std::function<void()> fn);

    // Run everything queued so far. Called from EOS_Platform_Tick only.
    size_t Drain();

    // --- notification registry -------------------------------------------
    EOS_NotificationId Register(NotifyKind kind, void* fn, void* clientData);
    void               Unregister(EOS_NotificationId id);
    std::vector<NotifyHandler> Handlers(NotifyKind kind) const;

    // Convenience: invoke every handler of `kind` with a typed callback info.
    // The info is produced by `make` for each handler so ClientData can differ.
    template <typename Info, typename Make>
    void Broadcast(NotifyKind kind, Make make) {
        for (const auto& h : Handlers(kind)) {
            auto fn = (void (EOS_CALL*)(const Info*))h.Fn;
            if (!fn) continue;
            void* clientData = h.ClientData;
            Post([fn, clientData, make]() {
                Info info = make(clientData);
                fn(&info);
            });
        }
    }

    // Answer an async entry point we do not model, without leaving the title
    // waiting forever. Every EOS callback info begins with
    // { EOS_EResult ResultCode; void* ClientData; ... }, so a zeroed block with
    // those two fields filled in is safe: callers check ResultCode first.
    void PostGenericCompletion(void* completionDelegate, void* clientData, ER result);

private:
    mutable std::mutex                  m_mutex;
    std::deque<std::function<void()>>   m_queue;
    std::vector<std::pair<uint64_t, std::function<void()>>> m_timed;
    std::vector<NotifyHandler>          m_handlers;
    EOS_NotificationId                  m_nextId = 1;
};

} // namespace refix
