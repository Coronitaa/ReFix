// The "CS:GO LAN" card of the overlay home: LAN rooms (gather before the match, follow the owner into it) and the
// shortcuts to Market & Skins and the LAN server settings. Shown only in CS:GO.
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
#include "../overlay_ui_mods.h"
#include "../../gc/lan_rooms.h"

namespace refix::overlay {

namespace {

constexpr float kPad = 14.0f;   // unscaled, BeginCard scales it
constexpr int kMaxListedRooms = 3;
float g_measuredHeight = 0.0f;  // content height of the last frame: the card never clips its content

bool IsCsgo() {
    static int cached = -1;
    if (cached < 0) {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring name = exe;
        name = name.substr(name.find_last_of(L"\\/") + 1);
        cached = _wcsicmp(name.c_str(), L"csgo.exe") == 0 ? 1 : 0;
    }
    return cached == 1;
}

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

void DrawRoomList(float w) {
    Caption(i18n::T("ROOMS ON THE LAN", "SALAS EN LA LAN"));
    std::vector<refix::rooms::Room> rooms = refix::rooms::List();
    if (rooms.empty()) {
        Caption(i18n::T("No rooms yet. Create one and your friends can join it.",
                        "Todavía no hay salas. Crea una y tus amigos podrán unirse."), ui::color::Muted);
    }
    float rowH = ui::S(36);
    for (size_t i = 0; i < rooms.size() && i < (size_t)kMaxListedRooms; ++i) {
        const auto& r = rooms[i];
        ImGui::PushID((int)i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + rowH), ui::color::Row, ui::S(8));
        Initials(dl, ImVec2(p.x + ui::S(18), p.y + rowH * 0.5f), ui::S(11), r.ownerName, r.inMatch ? ui::color::InGame : ui::color::Accent);
        char line[128];
        snprintf(line, sizeof(line), "%s  ·  %zu", r.ownerName.c_str(), r.memberCount);
        dl->AddText(ImVec2(p.x + ui::S(36), p.y + ui::S(4)), ui::color::Text, ui::Ellipsize(line, w - ui::S(130)).c_str());
        ImGui::PushFont(ui::F().caption);
        dl->AddText(ImVec2(p.x + ui::S(36), p.y + ui::S(19)), r.inMatch ? ui::color::InGame : ui::color::Muted,
                    r.inMatch ? i18n::T("In a match", "En partida") : i18n::T("Waiting in the room", "Esperando en la sala"));
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(p.x + w - ui::S(84), p.y + ui::S(5)));
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Join", "Unirse"), ImVec2(ui::S(78), rowH - ui::S(10))))) refix::rooms::Join(r.ownerSteamId);
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH + ui::S(4)));
        ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    if (ui::Clicked(ui::SecondaryButton(i18n::T("Create room", "Crear sala"), ImVec2(w, ui::S(34))))) refix::rooms::Create();
}

void DrawCurrentRoom(float w) {
    std::vector<refix::rooms::Member> members = refix::rooms::Members();
    bool owner = refix::rooms::IsOwner();
    std::string ownerName = members.empty() ? "" : members.front().name;
    char title[128];
    snprintf(title, sizeof(title), owner ? i18n::T("YOUR ROOM · %zu PLAYERS", "TU SALA · %zu JUGADORES")
                                         : i18n::T("ROOM · %zu PLAYERS", "SALA · %zu JUGADORES"), members.size());
    Caption(title, ui::color::Accent);

    // Member chips, wrapping onto new lines.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 start = ImGui::GetCursorScreenPos();
    float x = start.x, y = start.y, chipH = ui::S(28);
    for (const auto& m : members) {
        std::string label = m.name + (m.isLocal ? i18n::T(" (you)", " (tú)") : "");
        float chipW = ImGui::CalcTextSize(label.c_str()).x + ui::S(40);
        if (x > start.x && x + chipW > start.x + w) { x = start.x; y += chipH + ui::S(6); }
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + chipW, y + chipH), ui::color::Row, chipH * 0.5f);
        Initials(dl, ImVec2(x + chipH * 0.5f, y + chipH * 0.5f), ui::S(10), m.name, m.isOwner ? ui::color::Star : ui::color::Accent);
        dl->AddText(ImVec2(x + chipH + ui::S(4), y + (chipH - ImGui::GetFontSize()) * 0.5f), ui::color::Text, label.c_str());
        x += chipW + ui::S(6);
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, y + chipH + ui::S(8)));

    if (owner) {
        Caption(i18n::T("When you host a match, everyone in the room joins it.",
                        "Cuando hostees una partida, toda la sala entrará en ella."), ui::color::Muted);
        float half = (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Host for the room", "Hostear para la sala"), ImVec2(half, ui::S(34))))) OpenHostMatchModal();
        ImGui::SameLine();
        if (ui::Clicked(ui::DangerButton(i18n::T("Close room", "Cerrar sala"), ImVec2(half, ui::S(34))))) refix::rooms::Leave();
    } else {
        std::string waiting = std::string(i18n::T("Waiting for ", "Esperando a que ")) + ownerName +
                              i18n::T(" to start the match.", " inicie la partida.");
        Caption(waiting.c_str(), ui::color::Muted);
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Leave room", "Salir de la sala"), ImVec2(w, ui::S(34))))) refix::rooms::Leave();
    }
}

} // namespace

float GetModsCardHeight(float width) {
    if (!IsCsgo()) return 0.0f;
    (void)width;
    // First frame: a sensible estimate; afterwards the measured content height.
    return g_measuredHeight > 0.0f ? g_measuredHeight : ui::S(kPad) * 2 + ui::S(230);
}

void DrawModsCard(ImVec2 pos, float width, float height) {
    if (!IsCsgo()) return;
    if (height <= 0.0f) height = GetModsCardHeight(width);
    ui::BeginCard("##csgo_lan_card", pos, ImVec2(width, height), kPad);
    float w = ImGui::GetContentRegionAvail().x;
    float startY = ImGui::GetCursorPosY();

    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Accent, ui::icon::Game);
    ImGui::SameLine();
    ImGui::TextUnformatted("CS:GO LAN");
    ImGui::PopFont();
    Caption(i18n::T("Rooms, LAN matches and your inventory", "Salas, partidas LAN y tu inventario"), ui::color::Muted);
    ImGui::Dummy(ImVec2(0, ui::S(4)));

    if (refix::rooms::Current() == 0) DrawRoomList(w);
    else DrawCurrentRoom(w);

    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    float half = (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ui::Clicked(ui::PrimaryButton(i18n::T("Market & Skins", "Mercado & Skins"), ImVec2(half, ui::S(38))))) OpenMarketGiveMenu();
    ImGui::SameLine();
    if (ui::Clicked(ui::SecondaryButton(i18n::T("LAN server", "Servidor LAN"), ImVec2(half, ui::S(38))))) OpenHostMatchModal();

    g_measuredHeight = ImGui::GetCursorPosY() - startY + ui::S(kPad) * 2 + ui::S(4);
    ui::EndCard();
}

} // namespace refix::overlay
