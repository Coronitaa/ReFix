#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "overlay_text.h"
#include "refix_overlay.h"

#include "../include/steam/steam_api.h"
#include "../unreal_steam_emu.h"

#include <shellapi.h>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

constexpr ULONGLONG kRefreshMs = 300;
constexpr ULONGLONG kWelcomeMs = 6500;
constexpr ULONGLONG kInviteToastMs = 20000;
constexpr ULONGLONG kInfoToastMs = 4500;

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
        profiles_ = profiles::LoadAll();
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
    WaitForSingleObject(dialogDone_, INFINITE);
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
        profiles_ = profiles::LoadAll();
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
    const host::GameWindow gw = host::CurrentGame();
    if (!toasts_.empty() && gw.foreground) return host::Mode::Toast;
    return host::Mode::Hidden;
}

ImVec2 App::ToastSize() {
    float h = 0;
    for (const Toast& t : toasts_) h += ToastHeight(t.kind == Toast::Kind::Invite) + ui::S(kToastGap);
    return ImVec2(ui::S(kToastWidth) + ui::S(16), std::max(h, ui::S(kToastGap)) + ui::S(12));
}

ImVec2 App::DialogSize() {
    if (dialog_ == DialogKind::Picker && !dialogEditing_) return ImVec2(ui::S(500), ui::S(600));
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
    overlayOpen_ = true;
    inviteOnly_ = inviteOnly;
    inviteLobby_ = inviteLobby;
    inviteConnect_ = connect;
    filter_[0] = '\0';
    host::Wake();
}

void App::CloseOverlay() {
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
    if (!game.title.empty()) gameTitle_ = game.title;
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
    net::SetLocal(p);
}

void App::PollInvites() {
    for (const net::Invite& inv : net::TakeInvites()) {
        const net::RemotePlayer* from = FindPlayer(inv.from);
        const std::string who = from ? from->name : "Un jugador";
        if (inv.appId != appId_) {
            ShowInfo(who + " te invita", "a una partida de otro juego.");
            continue;
        }
        if (inv.lobbyId) {
            LobbyInvite_t cb = {};
            cb.m_ulSteamIDUser = inv.from;
            cb.m_ulSteamIDLobby = inv.lobbyId;
            cb.m_ulGameID = appId_;
            UnrealSteamEmu::PostCallback(LobbyInvite_t::k_iCallback, &cb, sizeof(cb));
        }
        Toast t;
        t.kind = Toast::Kind::Invite;
        t.title = who;
        t.body = "te invita a su partida";
        t.invite = inv;
        t.born = GetTickCount64();
        t.life = kInviteToastMs;
        toasts_.push_back(std::move(t));
        ReFixLog("[Overlay] Invite from %llu (lobby %llu)", (unsigned long long)inv.from, (unsigned long long)inv.lobbyId);
    }
}

void App::ShowInfo(const std::string& title, const std::string& body) {
    Toast t;
    t.kind = Toast::Kind::Info;
    t.title = title;
    t.body = body;
    t.born = GetTickCount64();
    t.life = kInfoToastMs;
    toasts_.push_back(std::move(t));
}

void App::Frame(const host::GameWindow& game, ImVec2 viewport) {
    DrainPosts();
    const ULONGLONG now = GetTickCount64();
    if (started_) {
        Refresh(game);
        PollInvites();
        if (!welcomed_ && game.hwnd && game.foreground) {
            welcomed_ = true;
            if (!profiles_.empty() || active_) {
                Toast t;
                t.kind = Toast::Kind::Welcome;
                t.born = now;
                t.life = kWelcomeMs;
                toasts_.push_back(std::move(t));
            }
        }
    }
    for (Toast& t : toasts_) {
        if (!t.closing && now - t.born > t.life) { t.closing = true; t.closingAt = now; }
    }
    while (!toasts_.empty() && toasts_.front().closing && now - toasts_.front().closingAt > kToastFadeMs)
        toasts_.pop_front();
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [&](const Toast& t) { return t.closing && now - t.closingAt > kToastFadeMs; }),
                  toasts_.end());

    if (viewport.x <= 0 || viewport.y <= 0) return;
    if (dialog_ != DialogKind::None) DrawDialog(viewport);
    else if (overlayOpen_) DrawOverlay(viewport);
    else DrawToasts(viewport, true);
}

void App::Join(const net::RemotePlayer& p) {
    if (p.lobbyId) {
        GameLobbyJoinRequested_t cb = {};
        cb.m_steamIDLobby = CSteamID(p.lobbyId);
        cb.m_steamIDFriend = CSteamID(p.steamId);
        UnrealSteamEmu::PostCallback(GameLobbyJoinRequested_t::k_iCallback, &cb, sizeof(cb));
    } else if (!p.connect.empty()) {
        GameRichPresenceJoinRequested_t cb = {};
        cb.m_steamIDFriend = CSteamID(p.steamId);
        strncpy_s(cb.m_rgchConnect, p.connect.c_str(), _TRUNCATE);
        UnrealSteamEmu::PostCallback(GameRichPresenceJoinRequested_t::k_iCallback, &cb, sizeof(cb));
    }
    ReFixLog("[Overlay] Join requested: player %llu lobby %llu", (unsigned long long)p.steamId, (unsigned long long)p.lobbyId);
    CloseOverlay();
}

void App::Invite(const net::RemotePlayer& p) {
    const uint64_t lobby = inviteOnly_ && inviteLobby_ ? inviteLobby_ : localLobby_;
    const std::string connect = inviteOnly_ && !inviteConnect_.empty() ? inviteConnect_ : localConnect_;
    if (net::SendInvite(p.steamId, lobby, connect)) invitedAt_[p.steamId] = GetTickCount64();
}

void App::ToggleFavorite(const net::RemotePlayer& p) {
    if (!active_) {
        ShowInfo("Crea un perfil", "para guardar jugadores favoritos.");
        return;
    }
    auto& fav = active_->favorites;
    auto it = std::find(fav.begin(), fav.end(), p.steamId);
    if (it != fav.end()) fav.erase(it);
    else fav.push_back(p.steamId);
    profiles::Save(*active_);
    lastRefresh_ = 0;
}

void App::OpenUrl(const std::wstring& url) {
    if (url.rfind(L"https://", 0) != 0 && url.rfind(L"http://", 0) != 0) return;
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    CloseOverlay();
}

void App::DeferProfile(const std::wstring& id) {
    profiles::SetLastProfileId(id);
    pendingSwitch_.clear();
    ShowInfo("Perfil guardado", "Se usará la próxima vez que abras el juego.");
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
        ShowInfo("No se pudo reiniciar", "Cierra y abre el juego para usar el perfil.");
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ReFixLog("[Overlay] Restarting with profile %s", ToUtf8(id).c_str());
    TerminateProcess(GetCurrentProcess(), 0);
}

void App::AcceptInvite(const net::Invite& inv) {
    net::RemotePlayer p;
    p.steamId = inv.from;
    p.lobbyId = inv.lobbyId;
    p.connect = inv.connect;
    Join(p);
}

ui::AvatarRef App::ProfileAvatar(const Profile& p) {
    ui::AvatarRef a;
    a.stock = p.stockAvatar;
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
