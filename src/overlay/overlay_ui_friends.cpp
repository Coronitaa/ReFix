// Steam online services in the overlay (ReFix Online): the Steam button of the top bar and its confirmation,
// the profile sync button (ring = sync state) and the Steam friends card next to the LAN players.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "app_names.h"
#include "overlay_host.h"
#include "overlay_i18n.h"
#include "overlay_settings.h"
#include "overlay_text.h"
#include "overlay_sound.h"
#include "profile_sync.h"
#include "../steam_link/steam_link.h"

#include <algorithm>
#include <cmath>

namespace refix::overlay {
namespace {

constexpr float kPillH = 36.0f;
constexpr float kRowH = 54.0f;
constexpr float kAvatarR = 17.0f;
constexpr float kPi = 3.14159265f;

using steamlink::State;
using i18n::T;

bool SteamInstalled() {   // registry lookup once per process, not every frame
    static const bool installed = steamlink::SteamInstalled();
    return installed;
}

float Pulse(float speed = 3.0f) { return 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * speed); }

ImU32 StateColor(State s) {
    switch (s) {
    case State::Online: return ui::color::SyncOk;
    case State::Starting: return ui::color::SyncPending;
    case State::Failed: return ui::color::Danger;
    default: return ui::color::Faint;
    }
}

const char* StateLabel(State s) {
    switch (s) {
    case State::Online: return T("On", "Activado");
    case State::Starting:
        switch (steamlink::CurrentStep()) {
        case steamlink::Step::OpeningSteam: return T("Opening Steam...", "Abriendo Steam...");
        case steamlink::Step::SigningIn: return T("Sign in to Steam...", "Inicia sesión en Steam...");
        default: return T("Connecting...", "Conectando...");
        }
    case State::Failed: return T("Error", "Error");
    default: return T("Off", "Desactivado");
    }
}

// --- friends ------------------------------------------------------------------------------------------------
int Rank(const steamlink::Friend& f, uint32_t appId) {
    if (f.refix) return f.appId == appId ? 0 : 1;
    if (f.playing) return 2;
    return f.personaState ? 3 : 4;
}

ImU32 PersonaColor(const steamlink::Friend& f) {
    if (f.refix) return ui::color::Accent;
    if (f.playing) return ui::color::SteamInGame;
    if (f.personaState == 0) return ui::color::SteamOffline;
    return f.personaState == 3 || f.personaState == 4 ? ui::Fade(ui::color::SteamOnline, 0.65f) : ui::color::SteamOnline;
}

// The same words Steam uses, plus which game a ReFix friend plays.
std::string PersonaLabel(const steamlink::Friend& f, uint32_t appId) {
    if (f.refix) {
        if (f.appId == appId) return T("Playing this game with ReFix", "Jugando a este juego con ReFix");
        std::string game = f.gameName.empty() ? apps::Name(f.appId) : f.gameName;
        if (game.empty()) game = T("another game", "otro juego");
        return std::string(T("Playing ", "Jugando a ")) + game + T(" with ReFix", " con ReFix");
    }
    if (f.playing) {
        const std::string game = apps::Name(f.steamGameId);
        return game.empty() ? std::string(T("In-Game", "En partida")) : std::string(T("Playing ", "Jugando a ")) + game;
    }
    switch (f.personaState) {
    case 0: return T("Offline", "Desconectado");
    case 2: return T("Busy", "Ocupado");
    case 3: return T("Away", "Ausente");
    case 4: return T("Snooze", "Inactivo");
    default: return T("Online", "Conectado");
    }
}

ui::AvatarRef FriendAvatar(const steamlink::Friend& f) {
    ui::AvatarRef a;
    a.stock = (int)(f.steamId % (uint64_t)std::max(1, ui::StockCount()));
    if (f.avatar <= 0) return a;
    const std::string key = "sf:" + std::to_string(f.steamId) + ":" + std::to_string(f.avatar);
    if (host::HasTexture(key)) {
        a.texture = host::Texture(key, nullptr);
        return a;
    }
    image::Rgba img;   // only cached once Steam has the picture: a failed load would stick
    if (steamlink::AvatarRgba(f.avatar, img.width, img.height, img.pixels))
        a.texture = host::Texture(key, [&](image::Rgba& out) { out = std::move(img); return true; });
    return a;
}

// --- sync ring --------------------------------------------------------------------------------------------
ImU32 SyncColor(sync::State s) {
    switch (s) {
    case sync::State::Synced: return ui::color::SyncOk;
    case sync::State::Pending: return ui::color::SyncPending;
    case sync::State::Failed: return ui::color::Danger;
    case sync::State::Syncing: return ui::color::Accent;
    default: return ui::color::Faint;
    }
}

// sync errors are English; the ones a player can act on get their Spanish here.
std::string SyncError(const std::string& e) {
    static const struct { const char* en; const char* es; } kKnown[] = {
        { "Newer saves from another PC: restart the game to take them", "Hay partidas más nuevas de otro PC: reinicia el juego para traerlas" },
        { "Destination not available", "El destino no está disponible" },
        { "Could not write to the destination", "No se pudo escribir en el destino" },
        { "The saved package is damaged", "El archivo guardado está dañado" },
    };
    for (const auto& k : kKnown) if (e == k.en) return T(k.en, k.es);
    return e;
}

std::string SyncTooltip(const sync::Status& s) {
    std::string t;
    switch (s.state) {
    case sync::State::Synced: t = T("Saves synced.", "Partidas sincronizadas."); break;
    case sync::State::Pending: t = T("Changes ready to sync.", "Cambios listos para sincronizar."); break;
    case sync::State::Syncing: t = T("Syncing...", "Sincronizando..."); break;
    case sync::State::Failed:
        t = T("Sync failed: ", "La sincronización falló: ") + SyncError(s.error) + ".";
        break;
    default:
        t = s.destination.empty() ? T("Choose where to keep your saves in Settings (a cloud folder or a USB drive).",
                                      "Elige dónde guardar tus partidas en Ajustes (una carpeta en la nube o un USB).")
                                  : T("The sync folder is not available right now.", "La carpeta de sincronización no está disponible ahora.");
        break;
    }
    if (!s.destination.empty()) t += std::string("\n") + T("Destination: ", "Destino: ") + ToUtf8(s.destination);
    if (s.lastSync) {
        FILETIME ft{ (DWORD)s.lastSync, (DWORD)(s.lastSync >> 32) }, local;
        SYSTEMTIME st;
        FileTimeToLocalFileTime(&ft, &local);
        FileTimeToSystemTime(&local, &st);
        char when[64];
        snprintf(when, sizeof(when), "%02u/%02u %02u:%02u", st.wDay, st.wMonth, st.wHour, st.wMinute);
        t += std::string("\n") + T("Last sync: ", "Última sincronización: ") + when;
    }
    return t + "\n" + T("Click to sync now.", "Haz clic para sincronizar ahora.");
}

void Bullet(const char* text, ImU32 col = ui::color::Muted) {
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Accent, "\xE2\x80\xA2");
    ImGui::SameLine();
    ui::TextColored(col, text);
    ImGui::PopTextWrapPos();
}

} // namespace

bool App::SteamPanelVisible() const {
    return SteamInstalled() && steamlink::GetState() != State::Off;
}

// Top bar: Steam mark, "Steam" and the state; the dot pulses while connecting. Returns the new left edge.
float App::DrawSteamButton(float right, float cy) {
    if (!SteamInstalled()) return right;
    const State st = steamlink::GetState();
    const ui::Fonts& f = ui::F();
    const std::string label = StateLabel(st);
    const float nameW = f.strong->CalcTextSizeA(f.strong->FontSize, FLT_MAX, 0, "Steam").x;
    const float stateW = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, label.c_str()).x;
    const float h = ui::S(kPillH), w = ui::S(8 + 24 + 10) + std::max(nameW, stateW) + ui::S(14 + 10 + 12);
    const ImVec2 p0(right - w, cy - h * 0.5f), p1(right, cy + h * 0.5f);
    ImGui::SetCursorScreenPos(p0);
    if (ui::Clicked(ui::NavButton("##steambtn", ImVec2(w, h)))) steamConfirm_ = true;
    const bool hover = ImGui::IsItemHovered();
    if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ui::Tooltip(T("Steam online services: play with your Steam friends over the Internet. Click to turn them on or off.",
                  "Servicios online de Steam: juega con tus amigos de Steam por Internet. Haz clic para activarlos o desactivarlos."));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 col = StateColor(st);
    const bool on = st == State::Online || st == State::Starting;
    dl->AddRectFilled(p0, p1, on ? ui::Fade(col, hover ? 0.22f : 0.14f) : (hover ? ui::color::RowHover : ui::color::PanelRaised), h * 0.5f);
    dl->AddRect(p0, p1, on ? ui::Fade(col, 0.7f) : ui::color::Border, h * 0.5f, 0, on ? ui::S(1.5f) : 1.0f);
    const ImVec2 logo(p0.x + ui::S(8 + 12), cy);
    ui::DrawSteamLogo(dl, logo, ui::S(12), on ? IM_COL32(240, 244, 248, 255) : ui::color::Muted, ui::color::SteamBlue);
    const float tx = p0.x + ui::S(8 + 24 + 10);
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(tx, cy - f.strong->FontSize * 0.95f), on ? ui::color::Text : ui::color::Muted, "Steam");
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx, cy + ui::S(1)), col, label.c_str());
    // State dot; it breathes while Steam is starting.
    const float dotR = ui::S(4.5f) * (st == State::Starting ? 0.75f + 0.5f * Pulse(6.0f) : 1.0f);
    dl->AddCircleFilled(ImVec2(p1.x - ui::S(14), cy), dotR, col, 16);
    if (st == State::Online) dl->AddCircle(ImVec2(p1.x - ui::S(14), cy), ui::S(4.5f) + ui::S(3) * Pulse(2.0f), ui::Fade(col, 0.35f), 16);
    return p0.x;
}

void App::DrawSteamConfirm() {
    if (!steamConfirm_) return;
    if (!ui::BeginCenteredModal("##steamconfirm", 540)) return;
    const State st = steamlink::GetState();
    const bool turningOn = st == State::Off || st == State::Failed;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ui::DrawSteamLogo(dl, ImVec2(at.x + ui::S(16), at.y + ui::S(16)), ui::S(16), IM_COL32(240, 244, 248, 255), ui::color::SteamBlue);
    ImGui::SetCursorScreenPos(ImVec2(at.x + ui::S(44), at.y + ui::S(3)));
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted(turningOn ? T("Turn on Steam online services?", "¿Activar los servicios online de Steam?")
                                     : T("Turn off Steam online services?", "¿Desactivar los servicios online de Steam?"));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(10)));
    ImGui::PushFont(ui::F().body);
    if (turningOn) {
        if (st == State::Failed && !steamlink::LastError().empty())
            Bullet((std::string(T("Last try: ", "Último intento: ")) + steamlink::LastError()).c_str(), ui::color::Danger);
        Bullet(T("ReFix opens Steam if it is closed and signs in as Spacewar: your friends will see you playing Spacewar.",
                 "ReFix abre Steam si está cerrado y se conecta como Spacewar: tus amigos te verán jugando a Spacewar."));
        Bullet(T("Your Steam friends who use ReFix can see your games and join them over the Internet. Players on your LAN can play with them through this PC.",
                 "Tus amigos de Steam que usen ReFix podrán ver tus partidas y unirse por Internet. Los jugadores de tu LAN podrán jugar con ellos a través de este PC."));
        Bullet(T("Only your Steam friends can connect, through Steam's network. The game itself never talks to Steam.",
                 "Solo tus amigos de Steam pueden conectarse, a través de la red de Steam. El juego en sí nunca habla con Steam."));
        Bullet(T("If Steam closes, online play with friends stops. LAN play keeps working.",
                 "Si Steam se cierra, el juego online con amigos se corta. El juego en LAN sigue funcionando."));
    } else {
        Bullet(T("You go back to playing on your LAN only.", "Vuelves a jugar solo en tu LAN."));
        Bullet(T("Steam friends who are in your game through Steam are disconnected.",
                 "Los amigos que estén en tu partida a través de Steam se desconectarán."));
        Bullet(T("The friends list is hidden. Your profiles and saves stay as they are.",
                 "La lista de amigos se oculta. Tus perfiles y partidas guardadas no cambian."));
    }
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(12)));
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bs((w - ui::S(10)) * 0.5f, ui::S(42));
    const bool confirm = turningOn ? ui::PrimaryButton(T("Turn on", "Activar"), bs)
                                   : ui::DangerButton(st == State::Starting ? T("Stop connecting", "Dejar de conectar") : T("Turn off", "Desactivar"), bs);
    ImGui::SameLine(0, ui::S(10));
    const bool cancel = ui::SecondaryButton(T("Cancel", "Cancelar"), bs) || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (confirm) {
        settings::SetSteamOnline(turningOn);
        if (turningOn) {
            steamlink::Disable();   // a failed attempt is retried from scratch
            steamlink::SetLocalGame(appId_);
            steamlink::SetLocalGameName(ToUtf8(gameTitle_));
            steamlink::Enable();
        } else {
            steamlink::Disable();
        }
    }
    if (confirm || cancel) {
        steamConfirm_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Top bar, next to the profile: the profile's sync state as a ring around a Steam mark (Steam profiles) or a
// cloud glyph (local ones), spinning while it works. Returns the new left edge.
float App::DrawSyncButton(float right, float cy) {
    const sync::Status s = sync::Get();
    if (s.state == sync::State::Off) return right;
    const float d = ui::S(kPillH);
    const ImVec2 c(right - d * 0.5f, cy);
    ImGui::SetCursorScreenPos(ImVec2(right - d, cy - d * 0.5f));
    const bool clicked = ui::Clicked(ui::NavButton("##syncbtn", ImVec2(d, d)));
    const bool hover = ImGui::IsItemHovered();
    if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ui::Tooltip(SyncTooltip(s).c_str());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 col = SyncColor(s.state);
    const float r = d * 0.5f;
    dl->AddCircleFilled(c, r, hover ? ui::color::RowHover : ui::color::PanelRaised, 32);
    if (s.state == sync::State::Syncing) {   // loading arc
        const float a0 = (float)ImGui::GetTime() * 5.0f;
        dl->PathArcTo(c, r - ui::S(2), a0, a0 + kPi * 1.3f, 24);
        dl->PathStroke(col, 0, ui::S(3));
        dl->AddCircle(c, r - ui::S(2), ui::Fade(col, 0.2f), 32, ui::S(3));
    } else {
        dl->AddCircle(c, r - ui::S(2), col, 32, ui::S(3));
    }
    const bool steamProfile = active_ && profiles::IsSteamProfile(*active_);
    const bool grey = s.state == sync::State::Unavailable;
    if (steamProfile) {
        ui::DrawSteamLogo(dl, c, r - ui::S(7), grey ? ui::color::Muted : IM_COL32(240, 244, 248, 255), ui::color::SteamBlue);
    } else {
        // Done: a tick in the ring's green; otherwise the sync arrows.
        const bool done = s.state == sync::State::Synced;
        ui::GlyphCentered(dl, ui::F().glyph, ui::S(18), c, grey ? ui::color::Muted : done ? ui::color::SyncOk : ui::color::Text,
                          done ? ui::icon::Check : ui::icon::Refresh);
    }
    if (clicked) {
        if (s.destination.empty()) { settingsSection_ = 4; settingsRequested_ = true; }   // Settings > Syncing   // nowhere to sync yet: the choice is in Settings
        else sync::SyncNow();
    }
    return right - d;
}

const PlayerRow* App::FriendRow(const steamlink::Friend& fr) const {
    if (!fr.refix || !fr.virtIp) return nullptr;
    for (const PlayerRow& r : rows_) if (r.p.ip == fr.virtIp || r.p.steamId == fr.steamId) return &r;
    return nullptr;
}

void App::DrawFriendRow(const steamlink::Friend& fr, float width) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + ui::S(kRowH));
    dl->AddRectFilled(p0, p1, fr.refix ? ui::Fade(ui::color::Accent, 0.08f) : ui::color::Row, ui::S(10));
    if (fr.refix) dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.35f), ui::S(10));
    const float cy = p0.y + ui::S(kRowH) * 0.5f;
    const ImVec2 ac(p0.x + ui::S(12) + ui::S(kAvatarR), cy);
    const ImU32 col = PersonaColor(fr);
    ui::DrawAvatar(dl, ac, ui::S(kAvatarR), FriendAvatar(fr));
    dl->AddCircle(ac, ui::S(kAvatarR) + ui::S(2), col, 32, ui::S(2));   // Steam's coloured frame

    const PlayerRow* row = FriendRow(fr);   // a ReFix friend in this game is also a LAN player (tunnel): invite from here
    // Same actions as a LAN player of this game: invite to my lobby, join theirs.
    float right = p1.x - ui::S(8);
    if (row && row->sameGame) {
        const float d = ui::S(36);
        ImGui::PushID((const void*)(uintptr_t)fr.steamId);
        right -= d;
        ImGui::SetCursorScreenPos(ImVec2(right, cy - d * 0.5f));
        switch (InviteStateFor(*row)) {
        case InviteState::InLobby:
            ui::IconButton("inv", ui::icon::Check, T("Already in your lobby", "Ya está en tu sala"), ui::color::Accent, false, d);
            break;
        case InviteState::Fresh:
            ui::IconButton("inv", ui::icon::Check, T("Invite sent", "Invitación enviada"), ui::color::Accent, false, d);
            break;
        case InviteState::Pending:
            if (ui::IconButton("inv", ui::icon::Refresh, T("Invited, not here yet: invite again", "Invitado, todavía no se unió: reenviar"),
                               ui::color::Invite, true, d))
                Invite(row->p);
            break;
        case InviteState::NoLobby:
            ui::IconButton("inv", ui::icon::Invite, T("Create or join a lobby to invite", "Crea o únete a una sala para invitar"),
                           ui::color::Invite, false, d);
            break;
        case InviteState::Ready:
            if (ui::IconButton("inv", ui::icon::Invite, T("Invite to my lobby", "Invitar a mi sala"), ui::color::Invite, true, d)) Invite(row->p);
            break;
        }
        if (CanJoin(*row, localLobby_)) {
            right -= d + ui::S(6);
            ImGui::SetCursorScreenPos(ImVec2(right, cy - d * 0.5f));
            if (ui::IconButton("join", ui::icon::Join, T("Join their match", "Unirse a su partida"), ui::color::Accent, true, d)) Join(row->p);
        }
        ImGui::PopID();
        right -= ui::S(6);
    }
    const float textX = p0.x + ui::S(12 + 2 * kAvatarR + 14);
    const float textW = std::max(ui::S(40), right - textX);
    ImGui::PushFont(f.strong);
    const std::string name = ui::Ellipsize(fr.name, textW);
    ImGui::PopFont();
    // In ReFix per Steam, but no presence through the tunnel yet: say why there is no invite or join.
    std::string label = PersonaLabel(fr, appId_);
    ImU32 statusCol = col;
    auto wait = friendWait_.find(fr.steamId);
    if (!row && wait != friendWait_.end()) {
        label = wait->second.late ? T("Can't be reached through Steam: no invite or join", "No se le alcanza por Steam: sin invitar ni unirse")
                                  : T("Connecting through Steam...", "Conectando por Steam...");
        statusCol = wait->second.late ? ui::color::Danger : ui::color::SyncPending;
        if (wait->second.late && ImGui::IsMouseHoveringRect(p0, p1)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ui::S(420));
            ImGui::TextUnformatted(T("Steam says this friend plays this game with ReFix, but nothing has arrived from their game through "
                                      "Steam's network. Both of you need ReFix 2.4 or newer with Steam online services on. "
                                      "If it stays like this, turn on [Debug] EnableLog in ReFix.ini on both PCs and send both ReFix.log files.",
                                      "Steam dice que este amigo juega a este juego con ReFix, pero no llega nada de su juego por la red "
                                      "de Steam. Los dos necesitáis ReFix 2.4 o posterior con los servicios online de Steam activados. "
                                      "Si sigue así, activad [Debug] EnableLog en ReFix.ini en los dos PCs y enviad los dos ReFix.log."));
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    ImGui::PushFont(f.caption);
    const std::string status = ui::Ellipsize(label, textW);
    ImGui::PopFont();
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(textX, cy - f.strong->FontSize), fr.personaState ? ui::color::Text : ui::color::Muted, name.c_str());
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(textX, cy + ui::S(2)), statusCol, status.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(6)));
    ImGui::Dummy(ImVec2(width, 0));
}

void App::DrawFriends(ImVec2 pos, ImVec2 size) {
    const State st = steamlink::GetState();
    std::vector<steamlink::Friend> friends = steamlink::Friends();
    std::sort(friends.begin(), friends.end(), [&](const steamlink::Friend& a, const steamlink::Friend& b) {
        const int ra = Rank(a, appId_), rb = Rank(b, appId_);
        if (ra != rb) return ra < rb;
        return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    const int online = (int)std::count_if(friends.begin(), friends.end(), [](const steamlink::Friend& x) { return x.personaState != 0; });

    ui::BeginCard("##friends", pos, size);
    const ImVec2 hp = ImGui::GetCursorScreenPos();
    ui::DrawSteamLogo(ImGui::GetWindowDrawList(), ImVec2(hp.x + ui::S(11), hp.y + ui::S(13)), ui::S(11),
                      IM_COL32(240, 244, 248, 255), ui::color::SteamBlue);
    ImGui::SetCursorScreenPos(ImVec2(hp.x + ui::S(30), hp.y));
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted(T("Steam friends", "Amigos de Steam"));
    ImGui::PopFont();
    if (st == State::Online) {
        ImGui::SameLine();
        ImGui::PushFont(ui::F().caption);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ui::S(5));
        char count[48];
        snprintf(count, sizeof(count), T("%d online", "%d conectados"), online);
        ui::TextColored(ui::color::Muted, count);
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));

    if (st != State::Online) {
        ImGui::PushFont(ui::F().caption);
        ImGui::PushTextWrapPos(0.0f);
        if (st == State::Starting) {
            const bool signIn = steamlink::CurrentStep() == steamlink::Step::SigningIn;
            ui::TextColored(ui::color::Muted, signIn ? T("Waiting for Steam to sign in. If Steam asks for your account or an update, finish it there.",
                                                         "Esperando a que Steam inicie sesión. Si Steam pide tu cuenta o una actualización, termínala allí.")
                                                     : T("Connecting to Steam (it opens by itself if it was closed)...",
                                                         "Conectando con Steam (se abre solo si estaba cerrado)..."));
        } else {
            ui::TextColored(ui::color::Danger, T("Could not connect to Steam.", "No se pudo conectar con Steam."));
            const std::string err = steamlink::LastError();
            if (!err.empty()) ui::TextColored(ui::color::Muted, err.c_str());
            ImGui::Dummy(ImVec2(0, ui::S(6)));
            if (ui::SecondaryButton(T("Try again", "Reintentar"))) {
                steamlink::Disable();
                steamlink::Enable();
            }
        }
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ui::EndCard();
        return;
    }

    ImGui::BeginChild("##friendlist", ImVec2(0, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    const float width = ImGui::GetContentRegionAvail().x;
    auto section = [&](const char* label, auto&& pick) {
        int n = 0;
        for (const auto& fr : friends) n += pick(fr) ? 1 : 0;
        if (!n) return;
        ui::SectionLabel(label, n);
        for (const auto& fr : friends) if (pick(fr)) DrawFriendRow(fr, width);
        ImGui::Dummy(ImVec2(0, ui::S(6)));
    };
    section(T("Online in ReFix", "En línea en ReFix"), [](const steamlink::Friend& x) { return x.refix; });
    section(T("In-Game", "En partida"), [](const steamlink::Friend& x) { return !x.refix && x.playing; });
    section(T("Online", "Conectados"), [](const steamlink::Friend& x) { return !x.refix && !x.playing && x.personaState != 0; });
    section(T("Offline", "Desconectados"), [](const steamlink::Friend& x) { return !x.refix && x.personaState == 0; });
    if (friends.empty()) ui::TextColored(ui::color::Muted, T("Your Steam friends list is empty.", "Tu lista de amigos de Steam está vacía."));
    ImGui::EndChild();
    ui::EndCard();
}

} // namespace refix::overlay
