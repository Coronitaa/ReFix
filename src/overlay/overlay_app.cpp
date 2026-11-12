#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "game_actions/overlay_actions.h"
#include "avatar_image.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "overlay_web.h"
#include "overlay_sound.h"
#include "overlay_ui_mods.h"
#include "../imgui/imgui_internal.h"
#include "save_swap.h"
#include "refix_overlay.h"
#include "overlay_settings.h"
#include "nvidia_reflex_block.h"
#include "../source_main_thread_queue.h"

#include "../include/steam/steam_api.h"
#include "../unreal_steam_emu.h"
#include "../steam_p2p_hook.h"
#include "../steam_link/steam_link.h"

#include <shellapi.h>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

constexpr ULONGLONG kRefreshMs = 300;
constexpr ULONGLONG kTunnelGraceMs = 15000;   // presence every 2 s; a new Steam P2P session takes a few seconds
constexpr ULONGLONG kNvidiaCheckMs = 200;
constexpr ULONGLONG kWelcomeMs = 6500;
constexpr ULONGLONG kInviteToastMs = 20000;
constexpr ULONGLONG kInfoToastMs = 4500;
constexpr ULONGLONG kInviteFreshMs = 8000;      // "Sent", then the invite can be sent again
constexpr ULONGLONG kInvitePendingMs = 60000;   // after this a player who did not come counts as not invited

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

} // namespace

App& App::Get() {
    static App app;
    return app;
}

void App::Post(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lock(postMu_);
        posts_.push_back(std::move(fn));
    }
    host::Wake();
}

void App::DrainPosts() {
    std::vector<std::function<void()>> todo;
    {
        std::lock_guard<std::mutex> lock(postMu_);
        todo.swap(posts_);
    }
    for (auto& fn : todo) fn();
}

std::optional<Profile> App::RunDialog(DialogKind kind) {
    dialogDone_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Post([this, kind] {
        ReloadProfiles();
        dialog_ = kind;
        dialogEditing_ = kind == DialogKind::FirstRun;
        if (dialogEditing_) BeginEditor(true);
    });
    if (!host::Start(this)) {
        CloseHandle(dialogDone_);
        dialogDone_ = nullptr;
        return std::nullopt;
    }
    host::Wake();
    while (true) {
        DWORD waitRes = MsgWaitForMultipleObjectsEx(1, &dialogDone_, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waitRes == WAIT_OBJECT_0) {
            break;
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CloseHandle(dialogDone_);
    dialogDone_ = nullptr;
    return dialogResult_;
}

void App::FinishDialog(std::optional<Profile> result) {
    dialogResult_ = std::move(result);
    dialog_ = DialogKind::None;
    dialogEditing_ = false;
    editor_.open = false;
    if (dialogDone_) SetEvent(dialogDone_);
}

void App::Begin(uint32_t appId, std::optional<Profile> active) {
    Post([this, appId, active] {
        appId_ = appId;
        active_ = active;
        ReloadProfiles();
        if (!settings::Get().uiSounds) sound::SetEnabled(false);   // the player muted them in the settings
        started_ = true;
    });
    if (!host::Start(this)) return;
    host::EnableHotkey();
    net::Start();
}

host::Mode App::DesiredMode() {
    if (dialog_ != DialogKind::None) return host::Mode::Dialog;
    if (!started_) return host::Mode::Hidden;
    if (overlayOpen_) return host::Mode::Overlay;
    if (hudMarketOpen_ || hudMatchmakingOpen_) return host::Mode::Overlay;
    const host::GameWindow gw = host::CurrentGame();
    if (!toasts_.empty() && gw.foreground) return host::Mode::Toast;
    return host::Mode::Hidden;
}

ImVec2 App::ToastSize() {
    float h = 0;
    for (const Toast& t : toasts_) h += ToastHeight(t.kind) + ui::S(kToastGap);
    return ImVec2(ui::S(kToastWidth) + ui::S(16), std::max(h, ui::S(kToastGap)) + ui::S(12));
}

ImVec2 App::DialogSize() {
    if (dialog_ == DialogKind::Picker && !dialogEditing_) return ImVec2(ui::S(620), ui::S(660));
    return ImVec2(ui::S(540), ui::S(720));
}

void App::OnHotkey() {
    if (dialog_ != DialogKind::None || !started_) return;
    if (overlayOpen_) CloseOverlay();
    else OpenOverlay();
}

void App::OnModeChanged(host::Mode from, host::Mode to) {
    if (to == host::Mode::Overlay) PostOverlayActivated(true);
    else if (from == host::Mode::Overlay) PostOverlayActivated(false);
}

void App::OnCloseRequested() {
    if (dialog_ != DialogKind::None) FinishDialog(std::nullopt);
    else CloseOverlay();
}

void App::PostOverlayActivated(bool active) {
    GameOverlayActivated_t cb = {};
    cb.m_bActive = active ? 1 : 0;
    cb.m_bUserInitiated = true;
    cb.m_nAppID = appId_;
    cb.m_dwOverlayPID = GetCurrentProcessId();
    UnrealSteamEmu::PostCallback(GameOverlayActivated_t::k_iCallback, &cb, sizeof(cb));
}

void App::OpenOverlay(bool inviteOnly, uint64_t inviteLobby, const std::string& connect) {
    if (!started_) return;
    if (!overlayOpen_) {
        focusOverlay_ = true;
        sound::Play(sound::Cue::Open);
        ReloadProfiles();   // another ReFix game may have created or changed profiles meanwhile
    }
    overlayOpen_ = true;
    inviteOnly_ = inviteOnly;
    inviteLobby_ = inviteLobby;
    inviteConnect_ = connect;
    filter_[0] = '\0';
    host::Wake();
}

void App::CloseOverlay() {
    if (overlayOpen_) sound::Play(sound::Cue::Close);
    if (webOpen_) webOpenRequested_ = true;   // the page is still there when the overlay reopens
    overlayOpen_ = false;
    inviteOnly_ = false;
    highlight_ = 0;
    host::Wake();
}

std::string App::LocalName() const {
    const char* n = UnrealSteamEmu::GetPersonaName();
    return n ? n : "";
}

const net::RemotePlayer* App::FindPlayer(uint64_t steamId) const {
    for (const PlayerRow& r : rows_) {
        if (r.p.steamId == steamId) return &r.p;
    }
    return nullptr;
}

void App::Refresh(const host::GameWindow& game) {
    const ULONGLONG now = GetTickCount64();
    if (now - lastRefresh_ < kRefreshMs) return;
    lastRefresh_ = now;
    if (!game.title.empty() && game.title != gameTitle_) {
        gameTitle_ = game.title;
        steamlink::SetLocalGameName(ToUtf8(gameTitle_));   // friends see which game you play
    }
    // The window title names this game better than its exe ("Left 4 Dead" for left4dead.exe).
    if (!gameNamed_ && active_ && !active_->guest && !gameTitle_.empty()) {
        gameNamed_ = true;
        Profile p = *active_;
        if (profiles::AddGame(p, profiles::GameKey(), gameTitle_) && profiles::Save(p)) {
            active_ = p;
            ReloadProfiles();
        }
    }
    const UnrealSteamEmu::OverlayLobbyInfo lobby = UnrealSteamEmu::Overlay_LocalLobby();
    localLobby_ = lobby.lobbyId;
    localConnect_ = UnrealSteamEmu::Overlay_ConnectString();

    std::vector<PlayerRow> rows;
    for (net::RemotePlayer& p : net::Players()) {
        PlayerRow r;
        r.sameGame = p.appId == appId_;
        r.favorite = active_ && std::find(active_->favorites.begin(), active_->favorites.end(), p.steamId) !=
                                    active_->favorites.end();
        r.p = std::move(p);
        rows.push_back(std::move(r));
    }
    std::sort(rows.begin(), rows.end(), [](const PlayerRow& a, const PlayerRow& b) {
        if (a.favorite != b.favorite) return a.favorite;
        if (a.sameGame != b.sameGame) return a.sameGame;
        return Lower(a.p.name) < Lower(b.p.name);
    });
    rows_ = std::move(rows);

    // Steam shows these friends in ReFix (rich presence), but invite and join need their presence through the tunnel.
    std::map<uint64_t, FriendWait> waiting;
    for (const steamlink::Friend& f : steamlink::Friends()) {
        if (!f.refix || f.appId != appId_ || FriendRow(f)) continue;
        auto it = friendWait_.find(f.steamId);
        FriendWait w = it != friendWait_.end() ? it->second : FriendWait{ now, false };
        if (!w.late && now - w.since >= kTunnelGraceMs) {
            w.late = true;
            ReFixLog("[Overlay] Steam friend %llu plays this game with ReFix, but nothing came from them through the Steam tunnel in %llu s",
                     (unsigned long long)f.steamId, (unsigned long long)((now - w.since) / 1000));
        }
        waiting[f.steamId] = w;
    }
    friendWait_ = std::move(waiting);
    PublishPresence();
}

void App::PublishPresence() {
    net::Presence p;
    p.steamId = UnrealSteamEmu::GetLocalSteamID();
    p.appId = appId_;
    p.name = LocalName();
    p.game = ToUtf8(gameTitle_);
    p.avatarKind = net::AvatarKind::Stock;
    p.avatarKey = active_ ? (uint32_t)active_->stockAvatar : 0;
    if (active_ && active_->customAvatar) {
        const std::wstring stamp = active_->id + L":" + std::to_wstring(profileVersion_);
        if (avatarKeyFor_ != stamp) {
            avatarKeyFor_ = stamp;
            avatarKey_ = 0;
            std::vector<uint8_t> jpeg;
            if (image::EncodeJpeg(profiles::AvatarPath(active_->id), image::kNetAvatarSize, jpeg)) {
                avatarKey_ = net::Crc32(jpeg.data(), jpeg.size());
                net::SetLocalAvatar(avatarKey_, std::move(jpeg));
            }
        }
        if (avatarKey_) {
            p.avatarKind = net::AvatarKind::Custom;
            p.avatarKey = avatarKey_;
        }
    }
    const UnrealSteamEmu::OverlayLobbyInfo lobby = UnrealSteamEmu::Overlay_LocalLobby();
    p.lobbyId = lobby.lobbyId;
    p.joinable = lobby.joinable;
    p.members = (uint8_t)std::min<uint32_t>(lobby.members, 255);
    p.maxMembers = (uint8_t)std::min<uint32_t>(lobby.maxMembers, 255);
    p.connect = localConnect_;
#if REFIX_STEAM_LINK
    p.steamOnlineId = (steamlink::GetState() == steamlink::State::Online) ? steamlink::LocalSteamId() : 0;
#endif
    net::SetLocal(p);
}

void App::PollInvites() {
    for (const net::Invite& inv : net::TakeInvites()) {
        const net::RemotePlayer* from = FindPlayer(inv.from);
        const std::string who = from ? from->name : i18n::T("A player", "Un jugador");
        const bool showToast = settings::Get().inviteToasts;   // the game gets the invite either way
        if (inv.appId != appId_) {
            ShowInfo(who + i18n::T(" invited you", " te invita"), i18n::T("to a match of another game.", "a una partida de otro juego."));
            continue;
        }
        if (from && from->ip != 0) {
            SteamP2PHook::RegisterPeer(inv.from, from->ip, from->port);
            char ipStr[32];
            snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
                     (from->ip >> 24) & 0xFF, (from->ip >> 16) & 0xFF,
                     (from->ip >> 8) & 0xFF, from->ip & 0xFF);
            UnrealSteamEmu::SimulatePeerEndpoint(inv.from, ipStr, from->port);
        }
        if (inv.lobbyId) {
            LobbyInvite_t cb = {};
            cb.m_ulSteamIDUser = inv.from;
            cb.m_ulSteamIDLobby = inv.lobbyId;
            cb.m_ulGameID = appId_;
            UnrealSteamEmu::PostCallback(LobbyInvite_t::k_iCallback, &cb, sizeof(cb));
        }
        if (!showToast) continue;
        Toast t;
        t.kind = Toast::Kind::Invite;
        t.title = who;
        t.body = i18n::T("invited you to their match", "te invita a su partida");
        t.invite = inv;
        t.born = GetTickCount64();
        t.life = kInviteToastMs;
        toasts_.push_back(std::move(t));
        sound::Play(sound::Cue::Notify);
        ReFixLog("[Overlay] Invite from %llu (lobby %llu)", (unsigned long long)inv.from, (unsigned long long)inv.lobbyId);
    }
}

void App::ShowInfo(const std::string& title, const std::string& body) {
    // Feedback to something done in the open overlay always shows; notices while playing follow the setting.
    if (!overlayOpen_ && !settings::Get().otherToasts) return;
    Toast t;
    t.kind = Toast::Kind::Info;
    t.title = title;
    t.body = body;
    t.born = GetTickCount64();
    t.life = kInfoToastMs;
    toasts_.push_back(std::move(t));
}

void App::Frame(const host::GameWindow& game, ImVec2 viewport) {
    // The web view is a child window: it shows only while last frame drew the browser modal.
    web::SetVisible(webDrawn_);
    webDrawn_ = false;
    DrainPosts();
    const ULONGLONG now = GetTickCount64();
    // NVIDIA App's overlay (its "Alt+Z" notice at game start, stats, Reflex indicator) stays off while this game or
    // ours is in front ("foreground" covers both: the ReFix overlay is a window of the game process). From the first
    // frame, not only once the game started Steam: the notice comes with the game's window.
    if (now - lastNvidiaCheck_ >= kNvidiaCheckMs) {
        lastNvidiaCheck_ = now;
        SetNvidiaOverlayHidden(settings::Get().blockReflex && game.foreground);
    }
    if (started_) {
        Refresh(game);
        PollInvites();
        PollAchievements();
        PollNvidiaReflexHooks();
        if (!welcomed_ && game.hwnd && game.foreground) {
            welcomed_ = true;
            if (settings::Get().welcomeToast) {
                Toast t;
                t.kind = Toast::Kind::Welcome;
                t.born = now;
                t.life = kWelcomeMs;
                toasts_.push_back(std::move(t));
            }
        }
    }
    // A notification's time only runs while it can be seen (game in front or overlay open): one that arrives
    // while the game is in the background still gets its full time on screen.
    const bool toastsVisible = overlayOpen_ || game.foreground;
    if (!toastsVisible && lastToastTick_ && now > lastToastTick_) {
        for (Toast& t : toasts_) {
            if (!t.closing) t.born += now - lastToastTick_;
        }
    }
    lastToastTick_ = now;
    for (Toast& t : toasts_) {
        // Notifications posted later in this frame have born > now: unsigned "now - born" would wrap and close them.
        if (!t.closing && now > t.born && now - t.born > t.life) { t.closing = true; t.closingAt = now; }
    }
    while (!toasts_.empty() && toasts_.front().closing && now - toasts_.front().closingAt > kToastFadeMs)
        toasts_.pop_front();
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [&](const Toast& t) { return t.closing && now - t.closingAt > kToastFadeMs; }),
                  toasts_.end());

    {
        refix::IRefixServices& svc = refix::GetRefixServices();
        ImGui::GetAllocatorFunctions(&svc.imgui_alloc, &svc.imgui_free, &svc.imgui_alloc_user_data);
        svc.imgui_context = ImGui::GetCurrentContext();
    }
    if (viewport.x <= 0 || viewport.y <= 0) return;
    if (dialog_ != DialogKind::None) DrawDialog(viewport);
    else if (overlayOpen_) DrawOverlay(viewport);
    else DrawToasts(viewport, true);

    if (hudMarketOpen_) DrawMarketGiveModal();
    if (hudMatchmakingOpen_) DrawMatchmakingToastWidget();
    for (const auto& modal : refix::overlay::OverlayActionsRegistry::Get().GetModals()) {
        modal.renderFn(viewport.x, viewport.y);
    }
    // Keyboard/controller focus moved to another item this frame (never set by the mouse).
    if (ImGui::GetCurrentContext()->NavJustMovedToId != 0 && (overlayOpen_ || dialog_ != DialogKind::None))
        sound::Play(sound::Cue::Move);
}

void App::Join(const net::RemotePlayer& p) {
    if (p.lobbyId != 0) {
        // P1.b: ONLY dispatch GameLobbyJoinRequested_t (callback 333).
        // Let CS:GO call JoinLobby and perform its handshake natively without competing JoinLobby calls.
        GameLobbyJoinRequested_t cb = {};
        cb.m_steamIDLobby = CSteamID(p.lobbyId);
        cb.m_steamIDFriend = CSteamID(p.steamId);
        UnrealSteamEmu::PostCallback(GameLobbyJoinRequested_t::k_iCallback, &cb, sizeof(cb));
    } else if (!p.connect.empty()) {
        // P1.b: ONLY dispatch GameRichPresenceJoinRequested_t (callback 337).
        // Do NOT execute +gcconnectG... as an engine console command and do not call engine from overlay thread.
        GameRichPresenceJoinRequested_t cb = {};
        cb.m_steamIDFriend = CSteamID(p.steamId);
        strncpy_s(cb.m_rgchConnect, p.connect.c_str(), _TRUNCATE);
        UnrealSteamEmu::PostCallback(GameRichPresenceJoinRequested_t::k_iCallback, &cb, sizeof(cb));
    } else if (p.ip != 0) {
        // Direct IP connect without rich presence connect string: queue to main thread (satisfies R5)
        char ipBuf[64];
        snprintf(ipBuf, sizeof(ipBuf), "connect %u.%u.%u.%u:%u",
                 (p.ip >> 24) & 0xFF, (p.ip >> 16) & 0xFF, (p.ip >> 8) & 0xFF, p.ip & 0xFF,
                 p.port != 0 ? p.port : 27015);
        std::string cmd = ipBuf;
        refix::source::MainThreadQueue::Enqueue([cmd]() {
            ExecuteEngineClientCommand(cmd.c_str());
        });
    }
    ReFixLog("[Overlay] Join requested: player %llu lobby %llu connect='%s'",
             (unsigned long long)p.steamId, (unsigned long long)p.lobbyId, p.connect.c_str());
    CloseOverlay();
}

InviteState App::InviteStateFor(const PlayerRow& row) const {
    const uint64_t lobby = inviteOnly_ && inviteLobby_ ? inviteLobby_ : localLobby_;
    const std::string& connect = inviteOnly_ && !inviteConnect_.empty() ? inviteConnect_ : localConnect_;
    if (lobby != 0 && row.p.lobbyId == lobby) return InviteState::InLobby;
    if (lobby == 0 && connect.empty()) return InviteState::NoLobby;
    auto it = invitedAt_.find(row.p.steamId);
    // An invite to an earlier lobby does not count for this one.
    if (it == invitedAt_.end() || it->second.lobby != lobby || it->second.connect != connect) return InviteState::Ready;
    const ULONGLONG age = GetTickCount64() - it->second.at;
    if (age < kInviteFreshMs) return InviteState::Fresh;
    return age < kInvitePendingMs ? InviteState::Pending : InviteState::Ready;
}

void App::Invite(const net::RemotePlayer& p) {
    uint64_t lobby = inviteOnly_ && inviteLobby_ ? inviteLobby_ : localLobby_;
    if (lobby == 0) {
        lobby = UnrealSteamEmu::Matchmaking_CreateLobby(1 /* k_ELobbyTypePublic */, 5);
        localLobby_ = lobby;
    }
    const std::string connect = inviteOnly_ && !inviteConnect_.empty() ? inviteConnect_ : localConnect_;
    if (lobby == 0 && connect.empty()) return;
    if (lobby != 0 && p.lobbyId == lobby) return;   // already there
    if (net::SendInvite(p.steamId, lobby, connect)) {
        invitedAt_[p.steamId] = SentInvite{ GetTickCount64(), lobby, connect };
    } else {
        sound::Play(sound::Cue::Denied);
        ShowInfo(i18n::T("Could not send the invite", "No se pudo enviar la invitación"),
                 i18n::T("The player is no longer on the LAN.", "El jugador ya no está en la LAN."));
    }
}

void App::ToggleFavorite(const net::RemotePlayer& p) {
    if (!active_ || active_->guest) {
        ShowInfo(i18n::T("Create a profile", "Crea un perfil"),
                 i18n::T("to keep favourite players.", "para guardar jugadores favoritos."));
        return;
    }
    auto& fav = active_->favorites;
    auto it = std::find(fav.begin(), fav.end(), p.steamId);
    if (it != fav.end()) fav.erase(it);
    else fav.push_back(p.steamId);
    profiles::Save(*active_);
    lastRefresh_ = 0;
}

void App::OpenWeb(const std::wstring& url) {
    if (url.rfind(L"https://", 0) != 0 && url.rfind(L"http://", 0) != 0) return;
    if (!web::Open(host::Window(), url)) {
        OpenUrl(url);
        return;
    }
    if (!overlayOpen_) OpenOverlay();
    webUrl_ = url;
    webOpen_ = true;
    webOpenRequested_ = true;
}

void App::CloseWeb() {
    webOpen_ = false;
    webOpenRequested_ = false;
    web::Close();
}

void App::OpenUrl(const std::wstring& url) {
    if (url.rfind(L"https://", 0) != 0 && url.rfind(L"http://", 0) != 0) return;
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    CloseOverlay();
}

void App::DeferProfile(const std::wstring& id) {
    profiles::SetLastProfileId(id);
    pendingSwitch_.clear();
    ShowInfo(i18n::T("Profile saved", "Perfil guardado"),
             i18n::T("It will be used the next time you open the game.", "Se usará la próxima vez que abras el juego."));
}

void App::RestartWithProfile(const std::wstring& id) {
    profiles::SetLastProfileId(id);
    RestoreLaunchEnvironment();
    SetEnvironmentVariableW(L"REFIX_PROFILE_ID", id.c_str());
    SetEnvironmentVariableW(L"REFIX_RESTART_PID", std::to_wstring(GetCurrentProcessId()).c_str());
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring cmd = GetCommandLineW();
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        ShowInfo(i18n::T("Could not restart", "No se pudo reiniciar"),
                 i18n::T("Close and open the game to use the profile.", "Cierra y abre el juego para usar el perfil."));
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ReFixLog("[Overlay] Restarting with profile %s", ToUtf8(id).c_str());
    UnrealSteamEmu::FlushUserStats();
    TerminateProcess(GetCurrentProcess(), 0);
}

bool App::DeleteProfile(const std::wstring& id) {
    if ((active_ && active_->id == id) || !profiles::Remove(id)) {
        sound::Play(sound::Cue::Denied);
        ShowInfo(i18n::T("Could not delete the profile", "No se pudo eliminar el perfil"),
                 i18n::T("It is in use in a game right now.", "Ahora mismo se está usando en un juego."));
        return false;
    }
    saves::ForgetOwner(id);
    ReloadProfiles();
    ++profileVersion_;
    return true;
}

void App::AcceptInvite(const net::Invite& inv) {
    const net::RemotePlayer* from = FindPlayer(inv.from);
    net::RemotePlayer p;
    if (from) {
        p = *from;
    } else {
        p.steamId = inv.from;
        p.lobbyId = inv.lobbyId;
        p.connect = inv.connect;
    }
    if (inv.lobbyId != 0) p.lobbyId = inv.lobbyId;
    if (!inv.connect.empty()) p.connect = inv.connect;
    Join(p);
}

void App::ReloadProfiles() {
    allProfiles_ = profiles::LoadAll();
    profiles_ = profiles::ForGame(allProfiles_, profiles::GameKey());
}

void App::ImportProfile(const std::wstring& id) {
    auto it = std::find_if(allProfiles_.begin(), allProfiles_.end(), [&](const Profile& p) { return p.id == id; });
    if (it == allProfiles_.end()) return;
    Profile p = *it;
    profiles::AddGame(p, profiles::GameKey(), gameTitle_.empty() ? profiles::DefaultGameName() : gameTitle_);
    if (!profiles::Save(p)) {
        sound::Play(sound::Cue::Denied);
        ShowInfo(i18n::T("Could not import the profile", "No se pudo importar el perfil"),
                 i18n::T("Its folder in %APPDATA%\\ReFix could not be written.", "No se pudo escribir su carpeta en %APPDATA%\\ReFix."));
        return;
    }
    ReFixLog("[Overlay] Profile %s imported from another game", ToUtf8(p.id).c_str());
    ReFixOverlay::InvalidateAvatar(p.steamId);
    ReloadProfiles();
    editor_.open = false;
    if (dialog_ != DialogKind::None) {
        dialogEditing_ = false;
        if (profiles::TryLock(p.id)) FinishDialog(p);   // play with it right away
    } else {
        pendingSwitch_ = p.id;                          // restart into it, after the confirmation
    }
}

ui::AvatarRef App::ProfileAvatar(const Profile& p) {
    ui::AvatarRef a;
    a.stock = p.stockAvatar;
    a.steam = profiles::IsSteamProfile(p);
    if (!p.customAvatar) return a;
    const std::string key = "p:" + ToUtf8(p.id) + ":" + std::to_string(profileVersion_);
    const std::wstring path = profiles::AvatarPath(p.id);
    a.texture = host::Texture(key, [&](image::Rgba& img) { return image::LoadFile(path, img); });
    return a;
}

ui::AvatarRef App::PlayerAvatar(const net::RemotePlayer& p) {
    ui::AvatarRef a;
    if (p.avatarKind == net::AvatarKind::Stock) {
        a.stock = (int)(p.avatarKey % (uint32_t)ui::StockCount());
        return a;
    }
    a.stock = (int)(p.steamId % (uint64_t)ui::StockCount());
    const std::string key = "r:" + std::to_string(p.steamId) + ":" + std::to_string(p.avatarKey);
    if (host::HasTexture(key)) {
        a.texture = host::Texture(key, nullptr);
        return a;
    }
    std::vector<uint8_t> jpeg;
    if (net::FetchAvatar(p.steamId, p.avatarKey, jpeg)) {
        a.texture = host::Texture(key, [&](image::Rgba& img) { return image::DecodeMemory(jpeg.data(), jpeg.size(), img); });
    }
    return a;
}

ui::AvatarRef App::EditorAvatar() {
    ui::AvatarRef a;
    a.stock = editor_.stock;
    if (editor_.custom && !editor_.imagePath.empty()) {
        const std::string key = "e:" + std::to_string(editor_.imageVersion);
        const std::wstring path = editor_.imagePath;
        a.texture = host::Texture(key, [&](image::Rgba& img) { return image::LoadFile(path, img); });
    }
    return a;
}

} // namespace refix::overlay
