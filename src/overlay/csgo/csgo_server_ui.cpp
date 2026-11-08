// "Servidor LAN": host a listen server with every common setting, or join a match hosted on the LAN. Maps come from
// the installed build's csgo\maps folder, so new or missing maps follow the game version.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../overlay_theme.h"
#include "../overlay_i18n.h"
#include "../overlay_app.h"
#include "../overlay_ui_mods.h"
#include "../../steam_gc_emu.h"
#include "../../gc/gc_relay.h"
#include "../../gc/lan_rooms.h"

namespace refix::overlay {

namespace {

struct ModeOption { const char* id; const char* en; const char* es; int defaultSlots; int defaultRounds; };
const ModeOption kModes[] = {
    { "competitive", "Competitive", "Competitivo", 10, 30 },
    { "casual", "Casual", "Casual", 20, 15 },
    { "scrimcomp2v2", "Wingman", "Compañero", 4, 16 },
    { "deathmatch", "Deathmatch", "Deathmatch", 16, 1 },
    { "gungameprogressive", "Arms Race", "Carrera armamentística", 12, 1 },
    { "gungametrbomb", "Demolition", "Demolición", 10, 20 },
};

struct ServerState {
    bool mapsLoaded = false;
    std::vector<std::string> maps;
    char mapFilter[64] = "";
    std::string map = "de_dust2";
    int mode = 0;
    int slots = 10;
    int maxRounds = 30;
    float roundTime = 1.92f;
    int bots = 0;
    int botDifficulty = 1;
    int startMoney = 800;
    int maxMoney = 16000;
    bool friendlyFire = false;
    bool overtime = true;
    bool cheats = false;
    char password[64] = "";
};
ServerState g_server;

int MapGroupRank(const std::string& m) {
    static const char* kPrefixes[] = { "de_", "cs_", "ar_", "dz_", "gd_" };
    for (int i = 0; i < 5; ++i)
        if (m.rfind(kPrefixes[i], 0) == 0) return i;
    return 5;
}

void LoadMaps() {
    g_server.mapsLoaded = true;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1) + L"csgo\\maps\\";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.bsp").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n.size() < 5 || _wcsicmp(n.c_str() + n.size() - 4, L".bsp") != 0) continue;  // 8.3 matches like x.bspx
            n = n.substr(0, n.size() - 4);
            if (n.find_first_of(L" ;\"") != std::wstring::npos) continue;  // HostLanMatch refuses these names
            if (std::any_of(n.begin(), n.end(), [](wchar_t c) { return c > 127; })) continue; // not a valid "map" argument
            std::string name;
            for (wchar_t c : n) name += (char)c;
            if (name.find("training") == std::string::npos && name.find("test") == std::string::npos) g_server.maps.push_back(name);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(g_server.maps.begin(), g_server.maps.end(), [](const std::string& a, const std::string& b) {
        int ra = MapGroupRank(a), rb = MapGroupRank(b);
        return ra != rb ? ra < rb : a < b;
    });
    if (!g_server.maps.empty() && std::find(g_server.maps.begin(), g_server.maps.end(), g_server.map) == g_server.maps.end())
        g_server.map = g_server.maps.front();
}

void Label(const char* text) {
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Faint, text);
    ImGui::PopFont();
}

void DrawMapList(float w, float h) {
    ImGui::BeginChild("##srv_maps", ImVec2(w, h), false);
    ui::SectionLabel(i18n::T("Map", "Mapa"), (int)g_server.maps.size());
    ImGui::SetNextItemWidth(w - ui::S(8));
    ImGui::InputTextWithHint("##srv_mapfilter", i18n::T("Filter maps", "Filtrar mapas"), g_server.mapFilter, sizeof(g_server.mapFilter));
    ImGui::BeginChild("##srv_maplist", ImVec2(0, 0), false);
    std::string filter = g_server.mapFilter;
    int lastRank = -1;
    static const char* kGroupEs[] = { "Desactivación", "Rehenes", "Carrera armamentística", "Zona de peligro", "Guardián", "Otros" };
    static const char* kGroupEn[] = { "Defusal", "Hostage", "Arms Race", "Danger Zone", "Guardian", "Other" };
    for (const std::string& m : g_server.maps) {
        if (!filter.empty() && m.find(filter) == std::string::npos) continue;
        int rank = MapGroupRank(m);
        if (rank != lastRank) {
            lastRank = rank;
            Label(i18n::T(kGroupEn[rank], kGroupEs[rank]));
        }
        bool sel = g_server.map == m;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(ImGui::GetContentRegionAvail().x, ui::S(30));
        ImGui::PushID(m.c_str());
        if (ImGui::InvisibleButton("##map", sz)) g_server.map = m;
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (sel || hov) dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), sel ? ui::color::RowHover : ui::color::Row, ui::S(8));
        if (sel) dl->AddRectFilled(p, ImVec2(p.x + ui::S(3), p.y + sz.y), ui::color::Accent, ui::S(2));
        dl->AddText(ImVec2(p.x + ui::S(12), p.y + (sz.y - ImGui::GetFontSize()) * 0.5f), sel ? ui::color::Text : ui::color::Muted, m.c_str());
    }
    if (g_server.maps.empty()) ui::TextColored(ui::color::Muted, i18n::T("No maps found in csgo\\maps.", "No hay mapas en csgo\\maps."));
    ImGui::EndChild();
    ImGui::EndChild();
}

void Segmented(const char* id, int& value, const char* const* labels, int count, float width) {
    float segW = (width - ImGui::GetStyle().ItemSpacing.x * (count - 1)) / count;
    for (int i = 0; i < count; ++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(id);
        ImGui::PushID(i);
        bool clicked = value == i ? ui::PrimaryButton(labels[i], ImVec2(segW, ui::S(30))) : ui::SecondaryButton(labels[i], ImVec2(segW, ui::S(30)));
        ImGui::PopID();
        ImGui::PopID();
        if (ui::Clicked(clicked)) value = i;
    }
}

void DrawSettings(float w, float h) {
    ImGui::BeginChild("##srv_settings", ImVec2(w, h), false);
    float fw = w - ui::S(12);
    ui::SectionLabel(i18n::T("Game mode", "Modo de juego"));
    float modeW = (fw - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
    for (int i = 0; i < 6; ++i) {
        if (i % 3) ImGui::SameLine();
        ImGui::PushID(i);
        const char* label = i18n::T(kModes[i].en, kModes[i].es);
        bool clicked = g_server.mode == i ? ui::PrimaryButton(label, ImVec2(modeW, ui::S(32)))
                                          : ui::SecondaryButton(label, ImVec2(modeW, ui::S(32)));
        ImGui::PopID();
        if (ui::Clicked(clicked) && g_server.mode != i) {
            g_server.mode = i;
            g_server.slots = kModes[i].defaultSlots;
            g_server.maxRounds = kModes[i].defaultRounds;
        }
    }

    ui::SectionLabel(i18n::T("Match", "Partida"));
    float half = (fw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    Label(i18n::T("PLAYERS", "JUGADORES"));
    ImGui::SetNextItemWidth(fw);
    ImGui::SliderInt("##slots", &g_server.slots, 2, 32);
    ImGui::BeginGroup();
    Label(i18n::T("MAX ROUNDS", "RONDAS MÁXIMAS"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderInt("##rounds", &g_server.maxRounds, 1, 60);
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    Label(i18n::T("ROUND TIME (MIN)", "TIEMPO DE RONDA (MIN)"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderFloat("##rtime", &g_server.roundTime, 1.0f, 10.0f, "%.2f");
    ImGui::EndGroup();
    ImGui::BeginGroup();
    Label(i18n::T("START MONEY", "DINERO INICIAL"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderInt("##smoney", &g_server.startMoney, 0, 16000, "$%d");
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    Label(i18n::T("MAX MONEY", "DINERO MÁXIMO"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderInt("##mmoney", &g_server.maxMoney, 1000, 65535, "$%d");
    ImGui::EndGroup();

    ui::SectionLabel(i18n::T("Bots", "Bots"));
    Label(i18n::T("NUMBER OF BOTS", "CANTIDAD DE BOTS"));
    ImGui::SetNextItemWidth(fw);
    ImGui::SliderInt("##bots", &g_server.bots, 0, (std::max)(0, g_server.slots - 1));
    if (g_server.bots > 0) {
        static const char* kDiffEs[] = { "Fácil", "Normal", "Difícil", "Experto" };
        static const char* kDiffEn[] = { "Easy", "Normal", "Hard", "Expert" };
        const char* diff[4];
        for (int i = 0; i < 4; ++i) diff[i] = i18n::T(kDiffEn[i], kDiffEs[i]);
        Label(i18n::T("DIFFICULTY", "DIFICULTAD"));
        Segmented("diff", g_server.botDifficulty, diff, 4, fw);
    }

    ui::SectionLabel(i18n::T("Rules", "Reglas"));
    ImGui::Checkbox(i18n::T("Friendly fire", "Fuego amigo"), &g_server.friendlyFire);
    ImGui::SameLine(half + ui::S(8));
    ImGui::Checkbox(i18n::T("Overtime", "Prórroga"), &g_server.overtime);
    ImGui::Checkbox(i18n::T("Cheats (sv_cheats)", "Trucos (sv_cheats)"), &g_server.cheats);
    Label(i18n::T("PASSWORD (OPTIONAL)", "CONTRASEÑA (OPCIONAL)"));
    ImGui::SetNextItemWidth(fw);
    ImGui::InputText("##srv_pw", g_server.password, sizeof(g_server.password), ImGuiInputTextFlags_Password);
    ImGui::EndChild();
}

void HostNow() {
    nlohmann::json cfg = {
        { "map", g_server.map }, { "mode", kModes[g_server.mode].id }, { "maxplayers", g_server.slots },
        { "maxrounds", g_server.maxRounds }, { "roundtime", g_server.roundTime }, { "startmoney", g_server.startMoney },
        { "maxmoney", g_server.maxMoney }, { "friendlyfire", g_server.friendlyFire ? 1 : 0 }, { "bot_quota", g_server.bots },
        { "bot_difficulty", g_server.botDifficulty }, { "sv_cheats", g_server.cheats ? 1 : 0 },
        { "overtime", g_server.overtime ? 1 : 0 }, { "password", std::string(g_server.password) },
    };
    ReFixGC::SetHostConfigJson(cfg.dump());
    ReFixGC::HostLanMatch();
    App::Get().ShowInfo(i18n::T("LAN server", "Servidor LAN"), std::string(i18n::T("Starting ", "Iniciando ")) + g_server.map);
    App::Get().CloseMatchmakingHUD();
}

} // namespace

void DrawMatchmakingToastWidget() {
    if (!App::Get().IsMatchmakingHUDOpen()) return;
    if (!g_server.mapsLoaded) LoadMaps();

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImVec2 size((std::min)(ui::S(940), vp.x - ui::S(40)), (std::min)(ui::S(680), vp.y - ui::S(40)));
    ImGui::SetNextWindowPos(ImVec2((vp.x - size.x) * 0.5f, (vp.y - size.y) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(22), ui::S(18)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::BackdropSolid);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);
    // The theme's input frames are as dark as this backdrop: lift them so fields and sliders are visible.
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::color::Row);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ui::color::RowHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ui::color::RowHover);
    if (ImGui::Begin("##refix_csgo_server", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(ui::F().title);
        ui::TextColored(ui::color::Accent, ui::icon::Lan);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("LAN server", "Servidor LAN"));
        ImGui::PopFont();
        float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(right - ui::S(30));
        if (ui::IconButton("srv_close", ui::icon::Close, i18n::T("Close", "Cerrar"), ui::color::Muted, true, ui::S(26)))
            App::Get().CloseMatchmakingHUD();
        ui::TextColored(ui::color::Faint, i18n::T("Host a match for the LAN or join one that is already running.",
                                                  "Hostea una partida para la LAN o únete a una que ya esté en marcha."));
        ImGui::Dummy(ImVec2(0, ui::S(8)));

        float footerH = ui::S(64);
        float bodyH = ImGui::GetContentRegionAvail().y - footerH;
        float mapW = ui::S(260), gap = ui::S(20);
        DrawMapList(mapW, bodyH);
        ImGui::SameLine(0, gap);
        DrawSettings(ImGui::GetContentRegionAvail().x, bodyH);

        // Footer: who follows the host, and the two actions.
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(6)));
        size_t members = refix::rooms::Members().size();
        std::string roomText = refix::rooms::Current() == 0
            ? i18n::T("Not in a room: other players join with Start or from their friends list.",
                      "Sin sala: los demás se unen con Iniciar o desde su lista de amigos.")
            : (refix::rooms::IsOwner()
                   ? std::to_string(members) + i18n::T(" players in your room will join automatically.", " jugadores de tu sala entrarán automáticamente.")
                   : std::string(i18n::T("You are in a room: the room owner starts the match.", "Estás en una sala: la partida la inicia el dueño de la sala.")));
        ImGui::AlignTextToFramePadding();
        ui::TextColored(ui::color::Muted, roomText.c_str());
        float btnW = ui::S(190);
        ImGui::SameLine(right - btnW * 2 - ui::S(10));
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Find LAN match", "Buscar partida LAN"), ImVec2(btnW, ui::S(40))))) {
            refix::gc::FindLanMatch(0, [] {
                App::Get().Post([] {
                    App::Get().ShowInfo(i18n::T("LAN match", "Partida LAN"),
                                        i18n::T("Nobody is hosting a match on the LAN.", "Nadie está hosteando una partida en la LAN."));
                });
            });
            App::Get().CloseMatchmakingHUD();
        }
        ImGui::SameLine();
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Host match", "Hostear partida"), ImVec2(btnW, ui::S(40))))) HostNow();
    }
    ImGui::End();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
}

} // namespace refix::overlay
