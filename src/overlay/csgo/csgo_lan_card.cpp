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

} // namespace

namespace csgo_ui {

float LanCardHeight(float width) {
    (void)width;
    return ui::S(96);
}

// Content of the "CS:GO LAN" horizontal bar placed under LAN players in the middle column
void DrawLanCard(float width, float height) {
    (void)height;
    const float availW = ImGui::GetContentRegionAvail().x > 0.0f ? ImGui::GetContentRegionAvail().x : width;
    const float pad = ui::S(16);
    const float leftW = (availW > ui::S(480)) ? (availW - pad) * 0.52f : availW;
    const float rightW = (availW > ui::S(480)) ? (availW - leftW - pad) : availW;

    const bool inRoom = (refix::rooms::Current() != 0);
    const bool isHost = inRoom && refix::rooms::IsOwner();
    const float btnH = ui::S(32);

    // Left Column: Room info & members or LAN room status
    ImGui::BeginGroup();
    if (!inRoom) {
        Caption(i18n::T("LAN ROOMS", "SALAS EN LA LAN"), ui::color::Accent);
        std::vector<refix::rooms::Room> rooms = refix::rooms::List();
        if (rooms.empty()) {
            ImGui::Dummy(ImVec2(0, ui::S(2)));
            Caption(i18n::T("No active rooms. Create one and invite your friends to play.",
                            "No hay salas activas. Crea una e invita a tus amigos a jugar."), ui::color::Muted);
            ImGui::Dummy(ImVec2(0, ui::S(4)));
            Caption(i18n::T("Invite Steam and LAN players with the buttons above.",
                            "Invita jugadores de Steam y LAN con los botones superiores."), ui::color::Faint);
        } else {
            const auto& r = rooms.front();
            ImGui::Dummy(ImVec2(0, ui::S(2)));
            char line[128];
            snprintf(line, sizeof(line), "%s (%zu %s)", r.ownerName.c_str(), r.memberCount,
                     r.memberCount == 1 ? i18n::T("player", "jugador") : i18n::T("players", "jugadores"));
            ImGui::TextUnformatted(line);
            ImGui::SameLine();
            if (ui::Clicked(ui::PrimaryButton(i18n::T("Join", "Unirse"), ImVec2(ui::S(70), ui::S(24))))) {
                refix::rooms::Join(r.ownerSteamId);
            }
        }
    } else {
        std::vector<refix::rooms::Member> members = refix::rooms::Members();
        char title[128];
        snprintf(title, sizeof(title), isHost ? i18n::T("YOUR ROOM · %zu PLAYERS", "TU SALA · %zu JUGADORES")
                                             : i18n::T("ROOM · %zu PLAYERS", "SALA · %zu JUGADORES"), members.size());
        Caption(title, ui::color::Accent);
        ImGui::Dummy(ImVec2(0, ui::S(2)));

        // Member pill chips
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 start = ImGui::GetCursorScreenPos();
        float x = start.x, y = start.y, chipH = ui::S(24);
        const float chipSpacing = ui::S(6);
        for (const auto& m : members) {
            std::string label = m.name + (m.isLocal ? i18n::T(" (you)", " (tú)") : "");
            float chipW = ImGui::CalcTextSize(label.c_str()).x + ui::S(32);
            if (chipW > leftW) chipW = leftW;
            if (x > start.x && x + chipW > start.x + leftW) {
                x = start.x;
                y += chipH + chipSpacing;
            }
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + chipW, y + chipH), ui::color::Row, chipH * 0.5f);
            Initials(dl, ImVec2(x + chipH * 0.5f, y + chipH * 0.5f), ui::S(8), m.name, m.isOwner ? ui::color::Star : ui::color::Accent);
            std::string fitLabel = ui::Ellipsize(label, chipW - chipH - ui::S(6));
            dl->AddText(ImVec2(x + chipH + ui::S(2), y + (chipH - ImGui::GetFontSize()) * 0.5f), ui::color::Text, fitLabel.c_str());
            x += chipW + chipSpacing;
        }
        ImGui::SetCursorScreenPos(ImVec2(start.x, y + chipH + ui::S(4)));
        if (isHost) {
            Caption(i18n::T("Host · Start the match when ready, all members follow.",
                            "Anfitrión · Inicia la partida cuando estés listo, todos entrarán."), ui::color::Muted);
        } else {
            Caption(i18n::T("Guest · Waiting for host to start the match...",
                            "Invitado · Esperando a que el anfitrión inicie la partida..."), ui::color::Muted);
        }
    }
    ImGui::EndGroup();

    // Right Column: Symmetrical action buttons
    if (availW > ui::S(480)) {
        ImGui::SameLine(0, pad);
    } else {
        ImGui::Dummy(ImVec2(0, ui::S(6)));
    }

    ImGui::BeginGroup();
    const float spacingX = ImGui::GetStyle().ItemSpacing.x;
    const float halfBtnW = (rightW - spacingX) * 0.5f;

    if (inRoom && isHost) {
        // Host controls: Host match + Close room, then Market & Skins
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Host match", "Hostear partida"), ImVec2(halfBtnW, btnH)))) OpenHud(kServerHud);
        ImGui::SameLine();
        if (ui::Clicked(ui::DangerButton(i18n::T("Close room", "Cerrar sala"), ImVec2(halfBtnW, btnH)))) refix::rooms::Leave();
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Market & Skins", "Mercado & Skins"), ImVec2(rightW, btnH)))) OpenHud(kMarketHud);
    } else if (inRoom && !isHost) {
        // Guest controls: CANNOT host, only Leave room + Market & Skins
        if (ui::Clicked(ui::DangerButton(i18n::T("Leave room", "Salir de la sala"), ImVec2(halfBtnW, btnH)))) refix::rooms::Leave();
        ImGui::SameLine();
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Market & Skins", "Mercado & Skins"), ImVec2(halfBtnW, btnH)))) OpenHud(kMarketHud);
    } else {
        // Not in room: Create room + LAN server, then Market & Skins
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Create room", "Crear sala"), ImVec2(halfBtnW, btnH)))) refix::rooms::Create();
        ImGui::SameLine();
        if (ui::Clicked(ui::SecondaryButton(i18n::T("LAN server", "Servidor LAN"), ImVec2(halfBtnW, btnH)))) OpenHud(kServerHud);
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Market & Skins", "Mercado & Skins"), ImVec2(rightW, btnH)))) OpenHud(kMarketHud);
    }
    ImGui::EndGroup();
}

void RunOnOverlayThread(std::function<void()> fn) { App::Get().Post(std::move(fn)); }

void CloseOverlayNow() { App::Get().CloseOverlay(); }

} // namespace csgo_ui

} // namespace refix::overlay
