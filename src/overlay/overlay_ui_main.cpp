// Overlay screens: the Shift+Tab overlay (top bar, community, players), the game's invite picker and the
// notifications. Profile screens are in overlay_ui_profile.cpp.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "../unreal_steam_emu.h"
#include "../refix_mono.h"

extern void ReFixLog(const char* fmt, ...);

#include <algorithm>
#include <cstdio>

#include "overlay_ui_mods.h"
#include "avatar_image.h"
#include "overlay_host.h"
#include <vector>

namespace refix::overlay {
namespace {

constexpr ULONGLONG kToastFadeInMs = 200;
constexpr float kMargin = 32.0f;
constexpr float kTopBarHeight = 56.0f;
constexpr float kRowHeight = 60.0f;
constexpr float kRowGap = 6.0f;
constexpr float kIconButton = 34.0f;
constexpr float kTileHeight = 72.0f;
constexpr float kAchievementsCardMin = 196.0f;   // header, progress and one recent row

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

bool Matches(const net::RemotePlayer& p, const char* filter) {
    if (!filter || !filter[0]) return true;
    const std::string f = Lower(filter);
    return Lower(p.name).find(f) != std::string::npos || Lower(p.game).find(f) != std::string::npos;
}

// Real AppID with a public Steam page (0 = unknown, 480 = Spacewar, the emulator's fallback).
bool HasStorePage(uint32_t appId) { return appId != 0 && appId != 480; }

std::string StatusLine(const PlayerRow& r, uint64_t localLobby, InviteState invite) {
    char buf[128];
    if (invite == InviteState::Fresh || invite == InviteState::Pending)
        return i18n::T("Invited · waiting for them to join", "Invitado · esperando a que se una");
    if (!r.sameGame) return r.p.game.empty() ? i18n::T("In another game", "En otro juego") : r.p.game;
    if (!r.p.lobbyId) return i18n::T("In this game", "En este juego");
    if (r.p.lobbyId == localLobby) return i18n::T("In your lobby", "En tu sala");
    if (r.p.joinable && r.p.maxMembers) {
        snprintf(buf, sizeof(buf), i18n::T("In a lobby · %u/%u", "En una sala · %u/%u"), r.p.members, r.p.maxMembers);
        return buf;
    }
    return r.p.joinable ? i18n::T("In a lobby", "En una sala") : i18n::T("In a closed lobby", "En una sala cerrada");
}



// The icon of the game's exe (largest it has, up to 256 px), as RGBA. No network: it is in the exe.
bool GameIconRgba(image::Rgba& out) {
    wchar_t exe[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) return false;
    constexpr int kSize = 128;
    HICON icon = nullptr;
    UINT id = 0;
    if (PrivateExtractIconsW(exe, 0, kSize, kSize, &icon, &id, 1, LR_DEFAULTCOLOR) != 1 || !icon) return false;
    ICONINFO ii{};
    bool ok = false;
    if (GetIconInfo(icon, &ii) && ii.hbmColor) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = kSize;
        bi.bmiHeader.biHeight = -kSize;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        std::vector<uint8_t> bgra((size_t)kSize * kSize * 4);
        HDC dc = GetDC(nullptr);
        ok = GetDIBits(dc, ii.hbmColor, 0, kSize, bgra.data(), &bi, DIB_RGB_COLORS) == kSize;
        ReleaseDC(nullptr, dc);
        bool anyAlpha = false;
        for (size_t i = 3; i < bgra.size(); i += 4) anyAlpha |= bgra[i] != 0;
        if (ok) {
            out.width = out.height = kSize;
            out.pixels.resize(bgra.size());
            for (size_t i = 0; i < bgra.size(); i += 4) {
                out.pixels[i] = bgra[i + 2];
                out.pixels[i + 1] = bgra[i + 1];
                out.pixels[i + 2] = bgra[i];
                out.pixels[i + 3] = anyAlpha ? bgra[i + 3] : 255;   // old icons without alpha: opaque
            }
        }
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(icon);
    return ok;
}

void TextAt(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const std::string& text) {
    dl->AddText(font, font->FontSize, pos, col, text.c_str());
}

} // namespace

bool CanJoin(const PlayerRow& r, uint64_t localLobby) {
    if (!r.sameGame || (r.p.lobbyId && r.p.lobbyId == localLobby)) return false;
    return (r.p.lobbyId && r.p.joinable) || (!r.p.lobbyId && !r.p.connect.empty());
}

void App::DrawOverlay(ImVec2 viewport) {
    // A fullscreen game that minimised when we took the focus leaves the desktop behind: hide it.
    const bool gameHidden = host::CurrentGame().minimized;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), viewport,
                                                  gameHidden ? ui::color::BackdropSolid : ui::color::Backdrop);
    // Soft top band so the bar reads over bright games.
    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(viewport.x, ui::S(160)),
                                                            IM_COL32(0, 0, 0, 90), IM_COL32(0, 0, 0, 90),
                                                            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(viewport);
    // Keyboard/controller navigation needs a focused window from the first frame (opened with the Guide
    // button there was none, so the D-pad did nothing).
    if (focusOverlay_) {
        ImGui::SetNextWindowFocus();
        focusOverlay_ = false;
    }
    ImGui::Begin("##overlay", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ImGui closes a popup on Escape before this runs: only close the overlay if none was open last frame.
    // B / Circle on a controller goes back the same way.
    // Not while a field is being edited: there Escape / B only leaves the field.
    if ((ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) && !popupOpenLastFrame_ &&
        !ImGui::IsAnyItemActive())
        CloseOverlay();

    if (inviteOnly_) {
        DrawInvitePicker(viewport);
    } else {
        DrawTopBar(viewport);
        const float m = ui::S(kMargin);
        const float top = m + ui::S(kTopBarHeight) + ui::S(20);
        const float height = viewport.y - top - m;
        const float leftW = std::clamp(viewport.x * 0.34f, ui::S(320), ui::S(440));
        float leftY = top;
        const bool hasAchievements = !achievements_.empty();
        const float gap = ui::S(20);
        const float achMin = hasAchievements ? ui::S(kAchievementsCardMin) : 0.0f;
        const float actH = GetModsCardHeight(leftW);  // 0 when the game has no card
        const float commH = ui::S(56 + 2 * kTileHeight + 10 + 40);
        // Left column, top to bottom: community, the game's mod card, achievements. When both a mod card and
        // achievements are present on a shorter viewport, yield the static community links first so the mod card
        // and the achievements card both fit comfortably without clipping each other.
        const float neededWithComm = commH + gap + (actH > 0.0f ? actH + (achMin > 0.0f ? gap + achMin : 0.0f) : achMin);
        if (HasStorePage(appId_) && (actH <= 0.0f || !hasAchievements || height >= neededWithComm)) {
            DrawCommunity(ImVec2(m, leftY), leftW);
            leftY += commH + gap;
        }
        const float roomH = top + height - leftY;
        if (actH > 0.0f && roomH >= ui::S(120)) {
            const float modsH = (std::min)(roomH, (std::min)(actH, (std::max)(ui::S(160), roomH - (achMin > 0 ? achMin + gap : 0.0f))));
            DrawModsCard(ImVec2(m, leftY), leftW, modsH);
            leftY += modsH + gap;
        }
        const float achH = top + height - leftY;
        if (hasAchievements && achH >= ui::S(110)) DrawAchievementsCard(ImVec2(m, leftY), ImVec2(leftW, achH));
        const float px = m + leftW + ui::S(20);
        float playersW = viewport.x - px - m;
        if (SteamPanelVisible()) {
            const float friendsW = std::clamp(playersW * 0.42f, ui::S(280), ui::S(420));
            playersW -= friendsW + ui::S(20);
            DrawFriends(ImVec2(px + playersW + ui::S(20), top), ImVec2(friendsW, height));
        }
        DrawPlayers(ImVec2(px, top), ImVec2(playersW, height), false);
    }

    DrawProfileMenu();
    DrawSwitchConfirm();
    DrawDeleteConfirm();
    DrawSteamConfirm();
    DrawEditorModal();
    DrawPurchaseModal(viewport);
    if (!inviteOnly_) DrawWebModal(viewport);
    if (!inviteOnly_) DrawAchievementsModal(viewport);
    if (!inviteOnly_) DrawSettingsModal(viewport);
    popupOpenLastFrame_ = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    ImGui::End();
    DrawToasts(viewport, false);
}

void App::DrawTopBar(ImVec2 viewport) {
    const float m = ui::S(kMargin);
    const float h = ui::S(kTopBarHeight);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ui::Fonts& f = ui::F();

    // Left: game title.
    const ImVec2 barMin(m, m);
    const ImVec2 barMax(viewport.x - m, m + h);
    dl->AddRectFilled(barMin, barMax, ui::color::Panel, ui::S(12));
    dl->AddRect(barMin, barMax, ui::color::Border, ui::S(12));
    const std::string title = gameTitle_.empty() ? i18n::T("Game", "Juego") : ToUtf8(gameTitle_);
    const float cy = m + h * 0.5f;
    // The game's own icon in the circle; the joystick when its exe has none.
    const ImVec2 ic(m + ui::S(26), cy);
    const float ir = ui::S(18);
    dl->AddCircleFilled(ic, ir, ui::Fade(ui::color::Accent, 0.16f), 32);
    if (const ImTextureID icon = host::Texture("game-icon", GameIconRgba)) {
        dl->AddImageRounded(icon, ImVec2(ic.x - ir, ic.y - ir), ImVec2(ic.x + ir, ic.y + ir), ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32_WHITE, ir);
        dl->AddCircle(ic, ir, ui::color::Border, 32, ui::S(1));
    } else {
        const ImVec2 gs = f.heading->CalcTextSizeA(f.heading->FontSize, FLT_MAX, 0, ui::icon::Game);
        TextAt(dl, f.heading, ImVec2(ic.x - gs.x * 0.5f, cy - gs.y * 0.5f), ui::color::Accent, ui::icon::Game);
    }
    const float titleX = m + ui::S(56);

    // Right, from the edge inwards: close, profile chip, clock.
    float x = viewport.x - m - ui::S(kIconButton) - ui::S(10);
    ImGui::SetCursorScreenPos(ImVec2(x, cy - ui::S(kIconButton) * 0.5f));
    if (ui::IconButton("close", ui::icon::Close, i18n::T("Close (Shift+Tab)", "Cerrar (Shift+Tab)"), ui::color::Text)) CloseOverlay();
    x -= ui::S(kIconButton) + ui::S(8);
    ImGui::SetCursorScreenPos(ImVec2(x, cy - ui::S(kIconButton) * 0.5f));
    if (ui::IconButton("settings", ui::icon::Settings, i18n::T("Settings", "Ajustes"), ui::color::Text)) settingsRequested_ = true;

    const std::string name = ui::Ellipsize(LocalName(), ui::S(180));
    ImGui::PushFont(f.strong);
    const float nameW = ImGui::CalcTextSize(name.c_str()).x;
    ImGui::PopFont();
    const float chipH = ui::S(44);
    const float chipW = ui::S(6 + 32 + 10) + nameW + ui::S(10 + 16 + 14);
    x -= ui::S(12) + chipW;
    chipMin_ = ImVec2(x, cy - chipH * 0.5f);
    chipMax_ = ImVec2(x + chipW, cy + chipH * 0.5f);
    ImGui::SetCursorScreenPos(chipMin_);
    if (ui::Clicked(ui::NavButton("##chip", ImVec2(chipW, chipH)))) ImGui::OpenPopup("##profilemenu");
    const bool hover = ImGui::IsItemHovered();
    if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ui::Tooltip(!active_ ? i18n::T("Playing without a profile", "Jugando sin perfil")
                : active_->guest ? i18n::T("Playing as a guest", "Jugando como invitado")
                                 : i18n::T("Profile: switch, create or edit", "Perfil: cambiar, crear o editar"));
    dl->AddRectFilled(chipMin_, chipMax_, hover ? ui::color::RowHover : ui::color::PanelRaised, chipH * 0.5f);
    dl->AddRect(chipMin_, chipMax_, ui::color::Border, chipH * 0.5f);
    const ui::AvatarRef avatar = active_ ? ProfileAvatar(*active_) : ui::AvatarRef{};
    ui::DrawAvatar(dl, ImVec2(chipMin_.x + ui::S(6 + 16), cy), ui::S(16), avatar);
    TextAt(dl, f.strong, ImVec2(chipMin_.x + ui::S(48), cy - f.strong->FontSize * 0.55f), ui::color::Text, name);
    TextAt(dl, f.caption, ImVec2(chipMax_.x - ui::S(30), cy - f.caption->FontSize * 0.45f), ui::color::Muted,
           ui::icon::ChevronDown);
    // Next to the profile: its sync ring, then the Steam online services button.
    x = DrawSyncButton(x - ui::S(10), cy);
    x = DrawSteamButton(x - ui::S(10), cy);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char clock[8];
    snprintf(clock, sizeof(clock), "%02u:%02u", st.wHour, st.wMinute);
    const ImVec2 cs = f.heading->CalcTextSizeA(f.heading->FontSize, FLT_MAX, 0, clock);
    x -= ui::S(20) + cs.x;
    TextAt(dl, f.heading, ImVec2(x, cy - cs.y * 0.5f), ui::color::Muted, clock);
    x -= ui::S(16);

    ImGui::PushFont(f.title);
    const std::string shown = ui::Ellipsize(title, (std::max)(ui::S(80), x - ui::S(24) - titleX));
    ImGui::PopFont();
    TextAt(dl, f.title, ImVec2(titleX, cy - f.title->FontSize * 0.55f), ui::color::Text, shown);


}

void App::DrawCommunity(ImVec2 pos, float width) {
    struct Link {
        const char* glyph;
        const char* en;
        const char* es;
        const wchar_t* format;
    };
    static const Link kLinks[] = {
        { ui::icon::Store, "Store", "Tienda", L"https://store.steampowered.com/app/%u/" },
        { ui::icon::People, "Community", "Comunidad", L"https://steamcommunity.com/app/%u" },
        { ui::icon::Chat, "Discussions", "Discusiones", L"https://steamcommunity.com/app/%u/discussions/" },
        { ui::icon::Guides, "Guides", "Guías", L"https://steamcommunity.com/app/%u/guides/" },
        { ui::icon::Workshop, "Workshop", "Workshop", L"https://steamcommunity.com/app/%u/workshop/" },
        { ui::icon::News, "News", "Noticias", L"https://store.steampowered.com/news/app/%u" },
    };
    const float height = ui::S(56 + 2 * kTileHeight + 10 + 40);
    ui::BeginCard("##community", pos, ImVec2(width, height));
    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Accent, ui::icon::Globe);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Community hub", "Centro de la comunidad"));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(4)));

    const float gap = ui::S(10);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float tileW = (avail - gap * 2) / 3.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < 6; ++i) {
        const ImVec2 p0(origin.x + (i % 3) * (tileW + gap), origin.y + (i / 3) * (ui::S(kTileHeight) + gap));
        const ImVec2 p1(p0.x + tileW, p0.y + ui::S(kTileHeight));
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        const bool clicked = ui::Clicked(ui::NavButton("##tile", ImVec2(tileW, ui::S(kTileHeight))));
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddRectFilled(p0, p1, hover ? ui::color::RowHover : ui::color::Row, ui::S(10));
        if (hover) dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.55f), ui::S(10));
        TextAt(dl, ui::F().heading, ImVec2(p0.x + ui::S(12), p0.y + ui::S(10)), ui::color::Accent, kLinks[i].glyph);
        ImGui::PushFont(ui::F().caption);
        const std::string label = ui::Ellipsize(i18n::T(kLinks[i].en, kLinks[i].es), tileW - ui::S(20));
        ImGui::PopFont();
        TextAt(dl, ui::F().caption, ImVec2(p0.x + ui::S(12), p1.y - ui::S(26)), ui::color::Text, label);
        if (clicked) {
            wchar_t url[160];
            swprintf_s(url, kLinks[i].format, appId_);
            OpenWeb(url);
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 2 * ui::S(kTileHeight) + gap));
    ImGui::Dummy(ImVec2(avail, 0));
    ui::EndCard();
}

int App::DrawWaitingFriends(float width) {
    if (friendWait_.empty()) return 0;
    const std::string filter = Lower(filter_);
    std::vector<steamlink::Friend> friends = steamlink::Friends();
    friends.erase(std::remove_if(friends.begin(), friends.end(), [&](const steamlink::Friend& x) {
                      return !friendWait_.count(x.steamId) || FriendRow(x) || !LinkedProfilesFor(x).empty() || Lower(x.name).find(filter) == std::string::npos;
                  }),
                  friends.end());
    if (friends.empty()) return 0;
    ImGui::Dummy(ImVec2(0, ui::S(8)));
    ui::SectionLabel(i18n::T("Steam friends in this game", "Amigos de Steam en este juego"), (int)friends.size());
    for (const auto& fr : friends) DrawFriendRow(fr, width);
    return (int)friends.size();
}

void App::DrawPlayers(ImVec2 pos, ImVec2 size, bool pickerStyle) {
    auto isLanPlayer = [](const PlayerRow& r) {
        if (steamlink::IsVirtual(r.p.ip)) return false;
        if (steamlink::IsBridgePort(r.p.port)) return false;
        return true;
    };
    const size_t lanCount = std::count_if(rows_.begin(), rows_.end(), isLanPlayer);

    if (pickerStyle) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::BeginChild("##pickerplayers", size, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    } else {
        ui::BeginCard("##players", pos, size);
        ImGui::PushFont(ui::F().heading);
        ui::TextColored(ui::color::Accent, ui::icon::Lan);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Players on the LAN", "Jugadores en la LAN"));
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::PushFont(ui::F().caption);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ui::S(5));
        char count[48];
        snprintf(count, sizeof(count), lanCount == 1 ? i18n::T("%zu online", "%zu conectado") : i18n::T("%zu online", "%zu conectados"),
                 lanCount);
        ui::TextColored(ui::color::Muted, count);
        ImGui::PopFont();
    }

    // Search box with a leading glyph.
    const ImVec2 sp = ImGui::GetCursorScreenPos();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ui::S(38), ui::S(9)));
    ImGui::InputTextWithHint("##filter", i18n::T("Search a player or game", "Buscar jugador o juego"), filter_, sizeof(filter_));
    ImGui::PopStyleVar();
    ImGui::GetWindowDrawList()->AddText(ui::F().body, ui::F().body->FontSize, ImVec2(sp.x + ui::S(13), sp.y + ui::S(9)),
                                        ui::color::Faint, ui::icon::Search);
    ImGui::Dummy(ImVec2(0, ui::S(2)));

    ImGui::BeginChild("##list", ImVec2(0, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    const float width = ImGui::GetContentRegionAvail().x;
    int shown = 0;
    auto section = [&](const char* label, auto&& pick) {
        int n = 0;
        for (const PlayerRow& r : rows_) n += pick(r) && Matches(r.p, filter_) ? 1 : 0;
        if (n == 0) return;
        if (shown) ImGui::Dummy(ImVec2(0, ui::S(8)));
        ui::SectionLabel(label, n);
        for (const PlayerRow& r : rows_) {
            if (pick(r) && Matches(r.p, filter_)) DrawPlayerRow(r, width, pickerStyle);
        }
        shown += n;
    };
    if (pickerStyle) {
        section(i18n::T("Favourites", "Favoritos"), [](const PlayerRow& r) { return r.favorite; });
        section(i18n::T("On the LAN", "En la LAN"), [&](const PlayerRow& r) { return !r.favorite && isLanPlayer(r); });
        section(i18n::T("Through Steam", "Por Steam"), [&](const PlayerRow& r) { return !r.favorite && !isLanPlayer(r); });
        if (const int n = DrawWaitingFriends(width)) shown += n;
    } else {
        section(i18n::T("Favourites", "Favoritos"), [&](const PlayerRow& r) { return r.favorite && isLanPlayer(r); });
        section(i18n::T("In this game", "En este juego"), [&](const PlayerRow& r) { return !r.favorite && isLanPlayer(r) && r.sameGame; });
        section(i18n::T("In other games", "En otros juegos"), [&](const PlayerRow& r) { return !r.favorite && isLanPlayer(r) && !r.sameGame; });
    }
    if (shown == 0) {
        ImGui::Dummy(ImVec2(0, ui::S(36)));
        const bool isEmpty = pickerStyle ? rows_.empty() : (lanCount == 0);
        const char* head = isEmpty ? i18n::T("Nobody else on the LAN yet", "Nadie más en la LAN por ahora")
                                   : i18n::T("No player matches", "Ningún jugador coincide");
        const char* body = isEmpty
                               ? i18n::T("Players show up here as soon as they open a game with ReFix LAN on this network.",
                                         "Los jugadores aparecen aquí en cuanto abren un juego con ReFix LAN en esta red.")
                               : i18n::T("Try another name or clear the search.", "Prueba con otro nombre o borra la búsqueda.");
        ImGui::PushFont(ui::F().title);
        const float gw = ImGui::CalcTextSize(ui::icon::Lan).x;
        ImGui::SetCursorPosX((width - gw) * 0.5f);
        ui::TextColored(ui::color::Faint, ui::icon::Lan);
        ImGui::PopFont();
        ImGui::PushFont(ui::F().strong);
        ImGui::SetCursorPosX((width - ImGui::CalcTextSize(head).x) * 0.5f);
        ImGui::TextUnformatted(head);
        ImGui::PopFont();
        ImGui::PushFont(ui::F().caption);
        ImGui::PushTextWrapPos(width);
        const float bw = std::min(ImGui::CalcTextSize(body).x, width);
        ImGui::SetCursorPosX((width - bw) * 0.5f);
        ui::TextColored(ui::color::Muted, body);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    }
    ImGui::EndChild();
    if (pickerStyle) ImGui::EndChild();
    else ui::EndCard();
}

void App::DrawPlayerRow(const PlayerRow& row, float width, bool pickerStyle) {
    const net::RemotePlayer& p = row.p;
    const ui::Fonts& f = ui::F();
    const float h = ui::S(kRowHeight);
    const float d = ui::S(kIconButton);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID((int)(p.steamId & 0x7FFFFFFF));
    ImGui::PushID(pickerStyle ? "pick_row" : "lan_row");
    ImGui::PushID((int)p.port);

    const bool hover = ImGui::IsMouseHoveringRect(p0, p1) && ImGui::IsWindowHovered();
    dl->AddRectFilled(p0, p1, hover ? ui::color::RowHover : ui::color::Row, ui::S(12));
    if (highlight_ == p.steamId) {
        dl->AddRect(p0, p1, ui::color::Accent, ui::S(12), 0, ui::S(1.5f));
        ImGui::SetScrollHereY(0.3f);
        highlight_ = 0;
    }

    const float r = ui::S(20);
    const ImVec2 ac(p0.x + ui::S(12) + r, p0.y + h * 0.5f);
    ui::DrawAvatar(dl, ac, r, PlayerAvatar(p));
    const ImU32 dot = row.sameGame ? ui::color::InGame : ui::color::Online;
    dl->AddCircleFilled(ImVec2(ac.x + r * 0.72f, ac.y + r * 0.72f), ui::S(6), ui::color::Row, 16);
    dl->AddCircleFilled(ImVec2(ac.x + r * 0.72f, ac.y + r * 0.72f), ui::S(4.5f), dot, 16);

    // Actions, right to left.
    const InviteState invite = InviteStateFor(row);
    float x = p1.x - ui::S(12);
    const float by = p0.y + (h - d) * 0.5f;
    if (pickerStyle) {
        const ImVec2 bs(ui::S(132), ui::S(38));
        x -= bs.x;
        ImGui::SetCursorScreenPos(ImVec2(x, p0.y + (h - bs.y) * 0.5f));
        // Settled states are a plain status, not a dead button: nothing to press there.
        auto status = [&](const char* text, ImU32 col) {
            const std::string label = std::string(ui::icon::Check) + "  " + text;
            const float lw = f.strong->CalcTextSizeA(f.strong->FontSize, FLT_MAX, 0, label.c_str()).x;
            TextAt(dl, f.strong, ImVec2(x + bs.x - lw - ui::S(6), p0.y + (h - f.strong->FontSize) * 0.5f - ui::S(1)), col, label);
        };
        switch (invite) {
        case InviteState::InLobby:
            status(i18n::T("In the lobby", "En la sala"), ui::color::Accent);
            break;
        case InviteState::Fresh:
            status(i18n::T("Sent", "Enviada"), ui::color::Invite);
            break;
        case InviteState::Pending:
            if (ui::SecondaryButton(i18n::T("Invite again", "Reenviar"), bs)) Invite(p);
            ui::Tooltip(i18n::T("Invited, not here yet", "Invitado, todavía no se unió"));
            break;
        case InviteState::NoLobby:
            ui::PrimaryButton(i18n::T("Invite", "Invitar"), bs, false);
            ui::Tooltip(i18n::T("Create or join a lobby first", "Primero crea o únete a una sala"));
            break;
        case InviteState::Ready:
            if (ui::PrimaryButton(i18n::T("Invite", "Invitar"), bs)) Invite(p);
            break;
        }
    } else {
        x -= d;
        ImGui::SetCursorScreenPos(ImVec2(x, by));
        if (ui::IconButton("fav", row.favorite ? ui::icon::StarFill : ui::icon::Star,
                           row.favorite ? i18n::T("Remove from favourites", "Quitar de favoritos")
                                        : i18n::T("Add to favourites", "Añadir a favoritos"),
                           row.favorite ? ui::color::Star : ui::color::Muted))
            ToggleFavorite(p);
        x -= d + ui::S(6);
        ImGui::SetCursorScreenPos(ImVec2(x, by));
        switch (invite) {
        case InviteState::InLobby:
            ui::IconButton("inv", ui::icon::Check, i18n::T("Already in your lobby", "Ya está en tu sala"), ui::color::Accent, false);
            break;
        case InviteState::Fresh:
            ui::IconButton("inv", ui::icon::Check, i18n::T("Invite sent", "Invitación enviada"), ui::color::Accent, false);
            break;
        case InviteState::Pending:
            if (ui::IconButton("inv", ui::icon::Refresh, i18n::T("Invited, not here yet: invite again", "Invitado, todavía no se unió: reenviar"),
                               ui::color::Invite))
                Invite(p);
            break;
        case InviteState::NoLobby:
            ui::IconButton("inv", ui::icon::Invite, i18n::T("Create or join a lobby to invite", "Crea o únete a una sala para invitar"),
                           ui::color::Invite, false);
            break;
        case InviteState::Ready:
            if (ui::IconButton("inv", ui::icon::Invite, i18n::T("Invite to my lobby", "Invitar a mi sala"), ui::color::Invite)) Invite(p);
            break;
        }
        if (CanJoin(row, localLobby_)) {
            x -= d + ui::S(6);
            ImGui::SetCursorScreenPos(ImVec2(x, by));
            if (ui::IconButton("join", ui::icon::Join, i18n::T("Join their match", "Unirse a su partida"), ui::color::Accent)) Join(p);
        }
    }

    const float tx = ac.x + r + ui::S(12);
    const float tw = std::max(ui::S(40), x - ui::S(10) - tx);
    ImGui::PushFont(f.strong);
    const std::string name = ui::Ellipsize(p.name, tw);
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    const std::string status = ui::Ellipsize(StatusLine(row, localLobby_, invite), tw);
    ImGui::PopFont();
    const float textH = f.strong->FontSize + ui::S(2) + f.caption->FontSize;
    const float ty = p0.y + (h - textH) * 0.5f - ui::S(1);
    TextAt(dl, f.strong, ImVec2(tx, ty), ui::color::Text, name);
    const bool waiting = invite == InviteState::Fresh || invite == InviteState::Pending;
    TextAt(dl, f.caption, ImVec2(tx, ty + f.strong->FontSize + ui::S(2)),
           waiting ? ui::color::Invite : row.sameGame ? ui::color::InGame : ui::color::Muted, status);

    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(width, h + ui::S(kRowGap)));
}

void App::DrawInvitePicker(ImVec2 viewport) {
    const ImVec2 size(std::min(ui::S(560), viewport.x - ui::S(48)), std::min(ui::S(640), viewport.y - ui::S(48)));
    const ImVec2 pos((viewport.x - size.x) * 0.5f, (viewport.y - size.y) * 0.5f);
    ui::BeginCard("##invitepicker", pos, size, 24.0f);
    ImGui::PushFont(ui::F().title);
    ImGui::TextUnformatted(i18n::T("Invite to the match", "Invitar a la partida"));
    ImGui::PopFont();
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Muted, i18n::T("Choose who to invite. They get a notification with a Join button.",
                                              "Elige a quién invitar. Recibirá un aviso con el botón Unirse."));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    const float footer = ui::S(56);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    DrawPlayers(ImGui::GetCursorScreenPos(), ImVec2(avail.x, avail.y - footer), true);
    const ImVec2 bs(ui::S(120), ui::S(40));
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - ui::S(24) - bs.x, pos.y + size.y - ui::S(24) - bs.y));
    if (ui::SecondaryButton(i18n::T("Close", "Cerrar"), bs)) CloseOverlay();
    ui::EndCard();
}

void App::DrawToasts(ImVec2 viewport, bool ownWindow) {
    if (toasts_.empty()) return;
    const ImVec2 area = ToastSize();
    const float margin = ownWindow ? 0.0f : ui::S(24);
    const ImVec2 origin(viewport.x - margin - area.x, viewport.y - margin - area.y);
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(area);
    ImGui::Begin("##toasts", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ui::Fonts& f = ui::F();
    const ULONGLONG now = GetTickCount64();
    const float w = ui::S(kToastWidth);
    float bottom = origin.y + area.y - ui::S(6);
    int index = 0;
    // Newest at the bottom, closest to the corner.
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it, ++index) {
        Toast& t = *it;
        const bool invite = t.kind == Toast::Kind::Invite;
        const float h = ToastHeight(t.kind);
        float a = std::min(1.0f, (float)(now - t.born) / kToastFadeInMs);
        if (t.closing) a = std::min(a, 1.0f - std::min(1.0f, (float)(now - t.closingAt) / kToastFadeMs));
        const float slide = (1.0f - a) * ui::S(28);
        const ImVec2 p0(origin.x + area.x - ui::S(8) - w + slide, bottom - h);
        const ImVec2 p1(p0.x + w, bottom);
        bottom -= h + ui::S(kToastGap);
        ImGui::PushID(index);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);

        if (t.kind != Toast::Kind::Achievement) ui::Card(dl, p0, p1, ui::Fade(ui::color::PanelRaised, a), 14.0f);   // draws its own
        if (t.kind != Toast::Kind::Achievement)
            dl->AddRectFilled(ImVec2(p0.x, p0.y + ui::S(14)), ImVec2(p0.x + ui::S(3), p1.y - ui::S(14)),
                              ui::Fade(invite ? ui::color::Invite : ui::color::Accent, a), ui::S(2));
        const float r = ui::S(22);
        const ImVec2 ac(p0.x + ui::S(18) + r, p0.y + (invite ? ui::S(18) + r : h * 0.5f));
        const float tx = ac.x + r + ui::S(14);
        const float tw = p1.x - ui::S(16) - tx;
        auto line = [&](ImFont* font, float y, ImU32 col, const std::string& text) {
            ImGui::PushFont(font);
            const std::string s = ui::Ellipsize(text, tw);
            ImGui::PopFont();
            dl->AddText(font, font->FontSize, ImVec2(tx, y), ui::Fade(col, a), s.c_str());
        };

        if (t.kind == Toast::Kind::Achievement) {
            DrawAchievementToast(dl, t, p0, p1, a);
        } else if (t.kind == Toast::Kind::Welcome) {
            ui::DrawAvatar(dl, ac, r, active_ ? ProfileAvatar(*active_) : ui::AvatarRef{});
            const float y = ac.y - (f.strong->FontSize + ui::S(26)) * 0.5f;
            line(f.strong, y, ui::color::Text, LocalName());
            // "Press [Shift] [Tab] to open ReFix", every piece centred on the same line.
            const float rowY = y + f.strong->FontSize + ui::S(4) + ui::KeycapHeight() * 0.5f;
            const ImU32 muted = ui::Fade(ui::color::Muted, a);
            auto words = [&](float x, const char* text) {
                dl->AddText(f.caption, f.caption->FontSize, ImVec2(x, rowY - f.caption->FontSize * 0.5f), muted, text);
                return x + f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, text).x;
            };
            float x = words(tx, i18n::T("Press", "Pulsa")) + ui::S(6);
            x += ui::KeycapAt(dl, ImVec2(x, rowY), "Shift", a) + ui::S(4);
            x += ui::KeycapAt(dl, ImVec2(x, rowY), "Tab", a) + ui::S(6);
            words(x, i18n::T("to open ReFix", "para abrir ReFix"));
        } else if (invite) {
            const net::RemotePlayer* from = FindPlayer(t.invite.from);
            ui::AvatarRef av;
            if (from) av = PlayerAvatar(*from);
            else av.stock = (int)(t.invite.from % (uint64_t)ui::StockCount());
            ui::DrawAvatar(dl, ac, r, av);
            line(f.strong, ac.y - f.strong->FontSize - ui::S(1), ui::color::Text, t.title);
            line(f.caption, ac.y + ui::S(2), ui::color::Muted, t.body);
            const ImVec2 bs(ui::S(118), ui::S(36));
            ImGui::SetCursorScreenPos(ImVec2(p1.x - ui::S(16) - bs.x * 2 - ui::S(8), p1.y - ui::S(14) - bs.y));
            if (ui::SecondaryButton(i18n::T("Ignore", "Ignorar"), bs) && !t.closing) { t.closing = true; t.closingAt = now; }
            ImGui::SameLine(0, ui::S(8));
            if (t.invite.lobbyId && t.invite.lobbyId == localLobby_) {
                ui::PrimaryButton(i18n::T("In the lobby", "Ya en la sala"), bs, false);
            } else if (ui::PrimaryButton(i18n::T("Join", "Unirse"), bs) && !t.closing) {
                t.closing = true;
                t.closingAt = now;
                AcceptInvite(t.invite);
            }
        } else {
            dl->AddCircleFilled(ac, r, ui::Fade(ui::color::Accent, 0.16f * a), 32);
            const ImVec2 gs = f.heading->CalcTextSizeA(f.heading->FontSize, FLT_MAX, 0, ui::icon::Info);
            dl->AddText(f.heading, f.heading->FontSize, ImVec2(ac.x - gs.x * 0.5f, ac.y - gs.y * 0.5f),
                        ui::Fade(ui::color::Accent, a), ui::icon::Info);
            line(f.strong, ac.y - f.strong->FontSize - ui::S(1), ui::color::Text, t.title);
            line(f.caption, ac.y + ui::S(2), ui::color::Muted, t.body);
        }
        // A click anywhere else on a notification dismisses it.
        if (!invite && !t.closing && ImGui::IsMouseHoveringRect(p0, p1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            t.closing = true;
            t.closingAt = now;
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
    ImGui::End();
}

void App::OpenPurchasePrompt(uint32_t appId, uint64_t orderId, const std::string& itemName, const std::string& itemDesc, bool isMicroTxn, std::function<void(bool)> onComplete) {
    pendingPurchase_.active = true;
    pendingPurchase_.appId = appId ? appId : appId_;
    pendingPurchase_.orderId = orderId;
    pendingPurchase_.itemName = itemName;
    pendingPurchase_.itemDesc = itemDesc;
    pendingPurchase_.isMicroTxn = isMicroTxn;
    pendingPurchase_.onComplete = onComplete;
    OpenOverlay(false);
}

void App::DrawPurchaseModal(ImVec2 viewport) {
    if (!pendingPurchase_.active) return;
    if (!ui::BeginCenteredModal("##purchasemodal", 480)) return;

    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Accent, ui::icon::Store);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Do you want to purchase this item?", "¿Deseas adquirir este artículo?"));
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(8)));

    std::string displayName = pendingPurchase_.itemName;
    if (displayName.empty()) {
        if (!pendingPurchase_.isMicroTxn && pendingPurchase_.appId != 0) {
            if (pendingPurchase_.appId == 3630990) displayName = "Kaiju Girls Bundle (Liz, Mortha, Sephia)";
            else if (pendingPurchase_.appId == 4820740) displayName = "Lumi Bundle";
            else if (pendingPurchase_.appId == 3950240) displayName = "Fuzzy PES Bundle (Ginger Wasabi)";
            else if (pendingPurchase_.appId == 948680) displayName = "Crush Crush Moist (18+ NSFW DLC)";
            else if (pendingPurchase_.appId == 1336600) displayName = "Mallory Character Bundle";
            else if (pendingPurchase_.appId == 782250) displayName = "Lotus Character Bundle";
            else if (pendingPurchase_.appId == 2279440) displayName = "Lydia Character Bundle";
            else displayName = i18n::T("DLC Package (AppID: ", "Paquete DLC (AppID: ") + std::to_string(pendingPurchase_.appId) + ")";
        } else {
            displayName = i18n::T("In-game Item", "Artículo del juego");
        }
    }

    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Text, displayName.c_str());
    ImGui::PopFont();

    if (!pendingPurchase_.itemDesc.empty() && pendingPurchase_.itemDesc != displayName) {
        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, pendingPurchase_.itemDesc.c_str());
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0, ui::S(16)));

    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bs((w - ui::S(10)) * 0.5f, ui::S(42));

    if (ui::PrimaryButton(i18n::T("Purchase", "Adquirir"), bs)) {
        ImGui::CloseCurrentPopup();
        ExecutePurchase(true);
    }
    ImGui::SameLine(0, ui::S(10));
    if (ui::SecondaryButton(i18n::T("Cancel", "Cancelar"), bs)) {
        ImGui::CloseCurrentPopup();
        ExecutePurchase(false);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        ImGui::CloseCurrentPopup();
        ExecutePurchase(false);
    }

    ImGui::EndPopup();
}

void App::ExecutePurchase(bool authorized) {
    if (!pendingPurchase_.active) return;
    const auto p = pendingPurchase_;
    pendingPurchase_.active = false;

    if (authorized) {
        ReFixLog("[Overlay] Purchase confirmed: appId=%u, orderId=%llu, microTxn=%d", p.appId, (unsigned long long)p.orderId, (int)p.isMicroTxn);
        if (p.onComplete) {
            p.onComplete(true);
        }
        if (p.isMicroTxn) {
            UnrealSteamEmu::AuthorizeMicroTxn(p.appId, p.orderId, true);
        } else {
            UnrealSteamEmu::UnlockDLC(p.appId);
            ReFixMono_UnlockDLC(p.appId);
        }
        ShowInfo(i18n::T("Item Acquired", "Artículo adquirido"),
                 i18n::T("The purchase has been validated locally.", "La compra se ha validado localmente."));
    } else {
        ReFixLog("[Overlay] Purchase cancelled: appId=%u, orderId=%llu", p.appId, (unsigned long long)p.orderId);
        if (p.onComplete) {
            p.onComplete(false);
        }
        if (p.isMicroTxn) {
            UnrealSteamEmu::AuthorizeMicroTxn(p.appId, p.orderId, false);
        }
    }
    CloseOverlay();
}

} // namespace refix::overlay
