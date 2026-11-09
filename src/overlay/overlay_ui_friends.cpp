// Steam online services in the overlay (ReFix Online): the small "Steam" switch in the players card header and
// the Steam friends card next to the LAN players. Both exist only when a Steam client is installed.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "overlay_i18n.h"
#include "overlay_settings.h"
#include "overlay_sound.h"
#include "../steam_link/steam_link.h"

#include <algorithm>

namespace refix::overlay {
namespace {

constexpr float kSwitchH = 30.0f;
constexpr float kRowH = 52.0f;
constexpr float kAvatarR = 16.0f;

using steamlink::State;

ImU32 StateColor(State s) {
    switch (s) {
    case State::Online: return ui::color::Online;
    case State::Starting: return ui::color::Star;
    case State::Failed: return ui::color::Danger;
    default: return ui::color::Faint;
    }
}

const char* StateTooltip(State s) {
    switch (s) {
    case State::Online:
        return i18n::T("Steam online services are on: you play with your Steam friends over the Internet as well as "
                       "on the LAN. Click to go back to LAN only.",
                       "Servicios online de Steam activados: juegas con tus amigos de Steam por Internet además de en "
                       "la LAN. Haz clic para volver a solo LAN.");
    case State::Starting:
        return i18n::T("Connecting to Steam (it opens by itself if it was closed). Click to cancel.",
                       "Conectando con Steam (se abre solo si estaba cerrado). Haz clic para cancelar.");
    case State::Failed:
        return i18n::T("Could not connect to Steam. Click to try again.", "No se pudo conectar con Steam. Haz clic para reintentar.");
    default:
        return i18n::T("Turn on Steam online services to play with your Steam friends over the Internet. "
                       "LAN play keeps working.",
                       "Activa los servicios online de Steam para jugar con tus amigos de Steam por Internet. "
                       "El juego en LAN sigue funcionando.");
    }
}

const char* PersonaLabel(const steamlink::Friend& f, uint32_t appId) {
    if (f.refix && f.appId == appId) return i18n::T("Playing this game with ReFix", "Jugando a este juego con ReFix");
    if (f.refix) return i18n::T("Playing another game with ReFix", "Jugando a otro juego con ReFix");
    if (f.playing) return i18n::T("In a game", "En un juego");
    switch (f.personaState) {
    case 0: return i18n::T("Offline", "Desconectado");
    case 2: return i18n::T("Busy", "Ocupado");
    case 3: case 4: return i18n::T("Away", "Ausente");
    default: return i18n::T("Online", "Conectado");
    }
}

ImU32 PersonaColor(const steamlink::Friend& f) {
    if (f.refix) return ui::color::Accent;
    if (f.playing) return ui::color::InGame;
    return f.personaState == 0 ? ui::color::Faint : ui::color::Online;
}

// Stable stock badge per friend until Steam avatars are wired in.
ui::AvatarRef FriendAvatar(uint64_t steamId) {
    ui::AvatarRef a;
    a.stock = (int)(steamId % (uint64_t)std::max(1, ui::StockCount()));
    return a;
}

// Registry lookup once per process, not every frame.
bool SteamInstalled() {
    static const bool installed = steamlink::SteamInstalled();
    return installed;
}

} // namespace

bool App::SteamPanelVisible() const {
    return SteamInstalled() && steamlink::GetState() != State::Off;
}

void App::DrawSteamSwitch(ImVec2 rightCenter) {
    if (!SteamInstalled()) return;
    const State st = steamlink::GetState();
    const ui::Fonts& f = ui::F();
    const char* label = "Steam";
    const float textW = f.strong->CalcTextSizeA(f.strong->FontSize, FLT_MAX, 0, label).x;
    const float h = ui::S(kSwitchH), w = textW + ui::S(14 + 10 + 14);
    const ImVec2 p0(rightCenter.x - w, rightCenter.y - h * 0.5f), p1(rightCenter.x, rightCenter.y + h * 0.5f);
    const ImVec2 back = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p0);
    const bool clicked = ui::Clicked(ui::NavButton("##steamswitch", ImVec2(w, h)));
    const bool hover = ImGui::IsItemHovered();
    if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ui::Tooltip(StateTooltip(st));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool on = st == State::Online || st == State::Starting;
    dl->AddRectFilled(p0, p1, on ? ui::Fade(StateColor(st), hover ? 0.28f : 0.18f) : (hover ? ui::color::RowHover : ui::color::Row), h * 0.5f);
    dl->AddRect(p0, p1, on ? ui::Fade(StateColor(st), 0.6f) : ui::color::Border, h * 0.5f);
    dl->AddCircleFilled(ImVec2(p0.x + ui::S(14), rightCenter.y), ui::S(4.5f), StateColor(st), 16);
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + ui::S(24), rightCenter.y - f.strong->FontSize * 0.55f),
                on ? ui::color::Text : ui::color::Muted, label);
    ImGui::SetCursorScreenPos(back);
    if (!clicked) return;
    if (on) {
        settings::SetSteamOnline(false);
        steamlink::Disable();
    } else {
        settings::SetSteamOnline(true);
        steamlink::SetLocalGame(appId_);
        steamlink::Enable();
    }
}

void App::DrawFriendRow(const steamlink::Friend& fr, float width) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + ui::S(kRowH));
    dl->AddRectFilled(p0, p1, ui::color::Row, ui::S(10));
    const float cy = p0.y + ui::S(kRowH) * 0.5f;
    ui::DrawAvatar(dl, ImVec2(p0.x + ui::S(10) + ui::S(kAvatarR), cy), ui::S(kAvatarR), FriendAvatar(fr.steamId));
    dl->AddCircleFilled(ImVec2(p0.x + ui::S(10) + ui::S(kAvatarR) * 1.7f, cy + ui::S(kAvatarR) * 0.7f), ui::S(4.5f), PersonaColor(fr), 16);

    // A friend running ReFix in this game is also in the players list (through the tunnel): invite from here.
    const PlayerRow* row = nullptr;
    if (fr.refix && fr.virtIp) {
        for (const PlayerRow& r : rows_) if (r.p.ip == fr.virtIp) row = &r;
    }
    float right = p1.x - ui::S(8);
    if (row && row->sameGame) {
        const float d = ui::S(36);
        ImGui::SetCursorScreenPos(ImVec2(right - d, cy - d * 0.5f));
        ImGui::PushID((const void*)(uintptr_t)fr.steamId);
        if (ui::IconButton("invite", ui::icon::Invite, i18n::T("Invite to your game", "Invitar a tu partida"), ui::color::Invite, true, d))
            Invite(row->p);
        ImGui::PopID();
        right -= d + ui::S(6);
    }
    const float textX = p0.x + ui::S(10 + 2 * kAvatarR + 12);
    const float textW = std::max(ui::S(40), right - textX);
    ImGui::PushFont(f.strong);
    const std::string name = ui::Ellipsize(fr.name, textW);
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    const std::string status = ui::Ellipsize(PersonaLabel(fr, appId_), textW);
    ImGui::PopFont();
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(textX, cy - f.strong->FontSize), fr.personaState ? ui::color::Text : ui::color::Muted, name.c_str());
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(textX, cy + ui::S(2)), PersonaColor(fr), status.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(6)));
    ImGui::Dummy(ImVec2(width, 0));
}

void App::DrawFriends(ImVec2 pos, ImVec2 size) {
    const State st = steamlink::GetState();
    std::vector<steamlink::Friend> friends = steamlink::Friends();
    std::stable_sort(friends.begin(), friends.end(), [&](const steamlink::Friend& a, const steamlink::Friend& b) {
        auto rank = [&](const steamlink::Friend& x) {
            return x.refix && x.appId == appId_ ? 0 : x.refix ? 1 : x.playing ? 2 : x.personaState ? 3 : 4;
        };
        return rank(a) < rank(b);
    });
    const int online = (int)std::count_if(friends.begin(), friends.end(), [](const steamlink::Friend& x) { return x.personaState != 0; });

    ui::BeginCard("##friends", pos, size);
    ImGui::PushFont(ui::F().heading);
    ui::TextColored(StateColor(st), ui::icon::People);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Steam friends", "Amigos de Steam"));
    ImGui::PopFont();
    if (st == State::Online) {
        ImGui::SameLine();
        ImGui::PushFont(ui::F().caption);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ui::S(5));
        char count[48];
        snprintf(count, sizeof(count), i18n::T("%d online", "%d conectados"), online);
        ui::TextColored(ui::color::Muted, count);
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));

    if (st != State::Online) {
        ImGui::PushFont(ui::F().caption);
        ImGui::PushTextWrapPos(0.0f);
        if (st == State::Starting) {
            ui::TextColored(ui::color::Muted, i18n::T("Connecting to Steam...", "Conectando con Steam..."));
        } else {
            ui::TextColored(ui::color::Danger, i18n::T("Could not connect to Steam.", "No se pudo conectar con Steam."));
            const std::string err = steamlink::LastError();
            if (!err.empty()) ui::TextColored(ui::color::Muted, err.c_str());
            ImGui::Dummy(ImVec2(0, ui::S(6)));
            if (ui::SecondaryButton(i18n::T("Try again", "Reintentar"))) {
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
    section(i18n::T("With ReFix", "Con ReFix"), [](const steamlink::Friend& x) { return x.refix; });
    section(i18n::T("Online", "Conectados"), [](const steamlink::Friend& x) { return !x.refix && x.personaState != 0; });
    section(i18n::T("Offline", "Desconectados"), [](const steamlink::Friend& x) { return !x.refix && x.personaState == 0; });
    if (friends.empty()) ui::TextColored(ui::color::Muted, i18n::T("Your Steam friends list is empty.", "Tu lista de amigos de Steam está vacía."));
    ImGui::EndChild();
    ui::EndCard();
}

} // namespace refix::overlay
