// The "CS:GO LAN" card of the overlay home: LAN rooms (gather before the match, follow the owner into it) and the
// shortcuts to Market & Skins and the LAN server settings. A cell of the mod grid, registered only in CS:GO.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include "../overlay_theme.h"
#include "../overlay_i18n.h"
#include "../overlay_app.h"
#include "csgo_overlay.h"
#include "../../gc/lan_rooms.h"

namespace refix::overlay {

namespace {

constexpr int kMaxListedRooms = 3;
float g_measuredHeight = 0.0f;  // content height of the last frame: the card never clips its content

void Initials(ImDrawList* dl, ImVec2 center, float radius, const std::string& name, ImU32 col) {
    dl->AddCircleFilled(center, radius, ui::Fade(col, 0.25f));
    dl->AddCircle(center, radius, col, 0, ui::S(1.5f));
    char initial[2] = { name.empty() ? '?' : (char)toupper((unsigned char)name[0]), 0 };
    ImVec2 ts = ImGui::CalcTextSize(initial);
    dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), ui::color::Text, initial);
}

void Caption(const char* text, ImU32 col = ui::color::Faint) {
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(col, text);
    ImGui::PopFont();
}

void DrawRoomList(float availW, float btnH) {
    Caption(i18n::T("ROOMS ON THE LAN", "SALAS EN LA LAN"));
    std::vector<refix::rooms::Room> rooms = refix::rooms::List();
    if (rooms.empty()) {
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        Caption(i18n::T("No rooms yet. Create one and your friends can join it.",
                        "Todavía no hay salas. Crea una y tus amigos podrán unirse."), ui::color::Muted);
        ImGui::Dummy(ImVec2(0, ui::S(4)));
    } else {
        const float rowH = ui::S(36);
        const float joinBtnW = ui::S(74);
        for (size_t i = 0; i < rooms.size() && i < (size_t)kMaxListedRooms; ++i) {
            const auto& r = rooms[i];
            ImGui::PushID((int)i);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, ImVec2(p.x + availW, p.y + rowH), ui::color::Row, ui::S(8));
            Initials(dl, ImVec2(p.x + ui::S(18), p.y + rowH * 0.5f), ui::S(11), r.ownerName, r.inMatch ? ui::color::InGame : ui::color::Accent);
            
            char line[128];
            snprintf(line, sizeof(line), "%s  ·  %zu", r.ownerName.c_str(), r.memberCount);
            const float textMaxW = (std::max)(ui::S(30), availW - joinBtnW - ui::S(48));
            dl->AddText(ImVec2(p.x + ui::S(36), p.y + ui::S(4)), ui::color::Text, ui::Ellipsize(line, textMaxW).c_str());
            ImGui::PushFont(ui::F().caption);
            const char* statusTxt = r.inMatch ? i18n::T("In a match", "En partida") : i18n::T("Waiting in the room", "Esperando en la sala");
            dl->AddText(ImVec2(p.x + ui::S(36), p.y + ui::S(19)), r.inMatch ? ui::color::InGame : ui::color::Muted, ui::Ellipsize(statusTxt, textMaxW).c_str());
            ImGui::PopFont();
            
            ImGui::SetCursorScreenPos(ImVec2(p.x + availW - joinBtnW - ui::S(5), p.y + (rowH - (rowH - ui::S(10))) * 0.5f));
            if (ui::Clicked(ui::PrimaryButton(i18n::T("Join", "Unirse"), ImVec2(joinBtnW, rowH - ui::S(10))))) refix::rooms::Join(r.ownerSteamId);
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH + ui::S(4)));
            ImGui::PopID();
        }
    }
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    if (ui::Clicked(ui::SecondaryButton(i18n::T("Create room", "Crear sala"), ImVec2(availW, btnH)))) refix::rooms::Create();
}

void DrawCurrentRoom(float availW, float btnH) {
    std::vector<refix::rooms::Member> members = refix::rooms::Members();
    bool owner = refix::rooms::IsOwner();
    std::string ownerName = members.empty() ? "" : members.front().name;
    char title[128];
    snprintf(title, sizeof(title), owner ? i18n::T("YOUR ROOM · %zu PLAYERS", "TU SALA · %zu JUGADORES")
                                         : i18n::T("ROOM · %zu PLAYERS", "SALA · %zu JUGADORES"), members.size());
    Caption(title, ui::color::Accent);

    // Member chips, symmetrical and wrapping cleanly without overflowing availW
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 start = ImGui::GetCursorScreenPos();
    float x = start.x, y = start.y, chipH = ui::S(26);
    const float chipSpacing = ui::S(6);
    for (const auto& m : members) {
        std::string label = m.name + (m.isLocal ? i18n::T(" (you)", " (tú)") : "");
        float chipW = ImGui::CalcTextSize(label.c_str()).x + ui::S(36);
        if (chipW > availW) chipW = availW; // Never exceed card width
        if (x > start.x && x + chipW > start.x + availW) {
            x = start.x;
            y += chipH + chipSpacing;
        }
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + chipW, y + chipH), ui::color::Row, chipH * 0.5f);
        Initials(dl, ImVec2(x + chipH * 0.5f, y + chipH * 0.5f), ui::S(9), m.name, m.isOwner ? ui::color::Star : ui::color::Accent);
        std::string fitLabel = ui::Ellipsize(label, chipW - chipH - ui::S(8));
        dl->AddText(ImVec2(x + chipH + ui::S(3), y + (chipH - ImGui::GetFontSize()) * 0.5f), ui::color::Text, fitLabel.c_str());
        x += chipW + chipSpacing;
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, y + chipH + ui::S(6)));

    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float halfW = (availW - itemSpacing) * 0.5f;

    if (owner) {
        // HOST REACTIVE VIEW: Host can start match or close room
        Caption(i18n::T("Host · When you start, everyone in the room joins.",
                        "Anfitrión · Al iniciar, toda la sala entrará."), ui::color::Muted);
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Host match", "Hostear partida"), ImVec2(halfW, btnH)))) csgo_ui::OpenHud(csgo_ui::kServerHud);
        ImGui::SameLine();
        if (ui::Clicked(ui::DangerButton(i18n::T("Close room", "Cerrar sala"), ImVec2(halfW, btnH)))) refix::rooms::Leave();
    } else {
        // GUEST REACTIVE VIEW: Guest sees who they are waiting for and can leave
        std::string waiting = std::string(i18n::T("Guest · Waiting for ", "Invitado · Esperando a que ")) + ownerName +
                              i18n::T(" to start...", " inicie...");
        Caption(ui::Ellipsize(waiting, availW).c_str(), ui::color::Muted);
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::DangerButton(i18n::T("Leave room", "Salir de la sala"), ImVec2(halfW, btnH)))) refix::rooms::Leave();
        ImGui::SameLine();
        ui::SecondaryButton(i18n::T("In Room", "En Sala"), ImVec2(halfW, btnH), false); // Symmetrical passive indicator
    }
}

} // namespace

namespace csgo_ui {

float LanCardHeight(float width) {
    (void)width;
    const bool inRoom = (refix::rooms::Current() != 0);
    return inRoom ? ui::S(210) : ui::S(195);
}

// Content of the "CS:GO LAN" grid cell; the overlay draws the card and its title.
void DrawLanCard(float width, float height) {
    (void)height;
    // Always use the exact available width inside the div/card
    const float availW = ImGui::GetContentRegionAvail().x > 0.0f ? ImGui::GetContentRegionAvail().x : width;
    const float startY = ImGui::GetCursorPosY();
    Caption(i18n::T("Rooms, LAN matches and your inventory", "Salas, partidas LAN y tu inventario"), ui::color::Muted);
    ImGui::Dummy(ImVec2(0, ui::S(3)));

    const float btnH = ui::S(34);
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float halfW = (availW - itemSpacing) * 0.5f;

    const bool inRoom = (refix::rooms::Current() != 0);
    const bool isHost = inRoom && refix::rooms::IsOwner();

    // Symmetrical body: Room list or active room without internal scroller
    if (!inRoom) {
        DrawRoomList(availW, btnH);
    } else {
        DrawCurrentRoom(availW, btnH);
    }

    ImGui::Dummy(ImVec2(0, ui::S(3)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, ui::S(3)));

    // Symmetrical bottom action row (Market & Skins vs LAN Server)
    // Buttons are reactive to Host vs Guest
    if (ui::Clicked(ui::PrimaryButton(i18n::T("Market & Skins", "Mercado & Skins"), ImVec2(halfW, btnH)))) OpenHud(kMarketHud);
    ImGui::SameLine();
    
    if (inRoom && !isHost) {
        // Guest mode: reactive button clearly indicates server is managed by host
        if (ui::Clicked(ui::SecondaryButton(i18n::T("View server", "Ver servidor"), ImVec2(halfW, btnH)))) OpenHud(kServerHud);
    } else {
        // Host or not in room: full server management button
        if (ui::Clicked(ui::SecondaryButton(i18n::T("LAN server", "Servidor LAN"), ImVec2(halfW, btnH)))) OpenHud(kServerHud);
    }

    g_measuredHeight = ImGui::GetCursorPosY() - startY + ui::S(4);
}

void RunOnOverlayThread(std::function<void()> fn) { App::Get().Post(std::move(fn)); }

void CloseOverlayNow() { App::Get().CloseOverlay(); }

} // namespace csgo_ui

} // namespace refix::overlay
