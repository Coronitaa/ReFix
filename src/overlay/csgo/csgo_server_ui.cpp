// "Servidor LAN": host a listen server with every common setting, or join a match hosted on the LAN. Maps come from
// the installed build's csgo\maps folder, so new or missing maps follow the game version.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../overlay_theme.h"
#include "../overlay_i18n.h"
#include "../overlay_app.h"
#include "csgo_overlay.h"
#include "../../steam_gc_emu.h"
#include "../../gc/gc_relay.h"
#include "../../gc/lan_rooms.h"
#include "../../csgo_schema.h"
#include "../../keyvalue.h"

namespace refix::overlay {

namespace {

// Game modes come from the installed build: gamemodes.txt (types, modes, player limits; gamemodes_server.txt on top),
// each mode's own gamemode_*.cfg for the defaults, and items_game.txt "skirmish_modes" for War Games. A 2018 build
// has no Danger Zone, a newer one does, and the limits follow whatever that build ships.
struct ModeOption {
    std::string id;     // gamemodes.txt mode name ("competitive"), or the War Game's name ("flyingscoutsman")
    std::string en, es;
    int gameType = 0;
    int gameMode = 0;
    int skirmish = 0;   // War Games: skirmish id (game_type 5)
    int maxPlayers = 10;
    int defaultRounds = 30;
    float defaultRoundTime = 1.92f;
    int defaultWarmupTime = 60;
    int defaultStartMoney = 800;
    int defaultMaxMoney = 16000;
    bool defaultFriendlyFire = false;  // mp_friendlyfire defaults to 0
    bool defaultOvertime = false;
    int defaultBots = 9;
    int defaultBotDifficulty = 2;
    const char* mapPrefix = nullptr;  // maps the mode needs
};
std::vector<ModeOption> g_modes;
bool g_modesLoaded = false;

std::string GameDir() {
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    return dir.substr(0, dir.find_last_of("\\/") + 1) + "csgo\\";
}

std::string LowerAscii(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Reads the ConVars a mode config sets ("name value // comment"); later files override earlier ones.
void ReadCfg(const std::string& path, ModeOption& m) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        line = line.substr(0, line.find("//"));
        std::istringstream ss(line);
        std::string name, value;
        if (!(ss >> name >> value)) continue;
        if (value.size() >= 2 && value.front() == '"') value = value.substr(1, value.size() - 2);
        name = LowerAscii(name);
        const double v = atof(value.c_str());
        if (name == "mp_maxrounds") m.defaultRounds = std::clamp((int)v, 1, 100);
        else if (name == "mp_roundtime") m.defaultRoundTime = std::clamp((float)v, 0.5f, 60.0f);
        else if (name == "mp_warmuptime") m.defaultWarmupTime = std::clamp((int)v, 0, 3600);
        else if (name == "mp_startmoney") m.defaultStartMoney = std::clamp((int)v, 0, 65535);
        else if (name == "mp_maxmoney") m.defaultMaxMoney = std::clamp((int)v, 0, 65535);
        else if (name == "mp_friendlyfire") m.defaultFriendlyFire = v != 0;
        else if (name == "mp_overtime_enable") m.defaultOvertime = v != 0;
        else if (name == "bot_quota") m.defaultBots = std::clamp((int)v, 0, 63);
        else if (name == "bot_difficulty") m.defaultBotDifficulty = std::clamp((int)v, 0, 3);
    }
}

// The KeyValues reader keeps one of the mode's two "exec" entries; both files follow from either name.
void ReadModeCfgs(const std::string& cfgDir, std::string exec, ModeOption& m) {
    exec = LowerAscii(exec);
    const size_t server = exec.find("_server.cfg");
    std::string base = server != std::string::npos ? exec.substr(0, server) : exec.substr(0, exec.rfind(".cfg"));
    if (base.empty()) return;
    ReadCfg(cfgDir + base + ".cfg", m);
    ReadCfg(cfgDir + base + "_server.cfg", m);
}

std::string Localized(const std::string& token, bool english, const std::string& fallback) {
    auto& schema = refix::csgo::SchemaManager::Get();
    if (token.empty() || !schema.IsLoaded()) return fallback;  // the GC thread loads it; never race its loading
    std::string text = english ? schema.ResolveTokenEn(token) : schema.ResolveToken(token);
    return text.empty() ? fallback : text;
}

// War Games of this build: items_game.txt skirmish_modes { "<id>" { name, loc_name, gamemode, server_exec } }.
void AddWarGames(const std::string& gameDir, const ModeOption& skirmishMode) {
    KeyValueRead kv((gameDir + "scripts\\items\\items_game.txt").c_str());
    const KeyValue* root = kv.Good() ? kv.Root().GetSubkey("items_game") : nullptr;
    const KeyValue* list = root ? root->GetSubkey("skirmish_modes") : nullptr;
    if (!list) return;
    for (const KeyValue& sk : list->Subkeys()) {
        const int id = FromString<int>(sk.Name());
        if (id <= 0) continue;
        ModeOption m = skirmishMode;
        // Defaults of the mode it is built on, then its own server config.
        for (const ModeOption& base : g_modes)
            if (base.id == sk.GetString("gamemode")) m = base;
        m.id = std::string(sk.GetString("name"));
        m.gameType = skirmishMode.gameType;
        m.gameMode = skirmishMode.gameMode;
        m.maxPlayers = skirmishMode.maxPlayers;
        m.skirmish = id;
        m.mapPrefix = nullptr;
        std::istringstream exec{ std::string(sk.GetString("server_exec")) };
        std::string word, cfg;
        while (exec >> word) cfg = word;  // "execwithwhitelist op08_flying_scoutsman.cfg"
        if (!cfg.empty()) ReadCfg(gameDir + "cfg\\" + cfg, m);
        const std::string token(sk.GetString("loc_name"));
        m.en = Localized(token, true, m.id);
        m.es = Localized(token, false, m.en);
        // Some builds also list a regular mode here (Arms Race, Demolition): the mode button already hosts it.
        const bool repeatsMode = std::any_of(g_modes.begin(), g_modes.end(), [&](const ModeOption& o) {
            return !o.skirmish && (o.en == m.en || o.id == m.id);
        });
        if (!m.id.empty() && !repeatsMode) g_modes.push_back(m);
    }
}

bool g_namesLocalized = false;

void LoadModes() {
    g_modesLoaded = true;
    g_namesLocalized = refix::csgo::SchemaManager::Get().IsLoaded();
    g_modes.clear();
    const std::string gameDir = GameDir();
    KeyValueRead kv((gameDir + "gamemodes.txt").c_str());
    if (kv.Good()) kv.MergeToFromFile((gameDir + "gamemodes_server.txt").c_str());  // optional server overrides
    const KeyValue* root = kv.Good() ? kv.Root().GetSubkey("GameModes.txt") : nullptr;
    const KeyValue* types = root ? root->GetSubkey("gameTypes") : nullptr;
    ModeOption skirmishMode;
    bool hasSkirmish = false;
    for (const KeyValue& type : types ? types->Subkeys() : std::vector<KeyValue>{}) {
        if (type.GetNumber<int>("singleplayeronly", 0)) continue;  // Training
        const std::string typeName = LowerAscii(std::string(type.Name()));
        if (typeName == "custom") continue;                        // no rules of its own
        const KeyValue* modes = type.GetSubkey("gameModes");
        for (const KeyValue& mode : modes ? modes->Subkeys() : std::vector<KeyValue>{}) {
            ModeOption m;
            m.id = LowerAscii(std::string(mode.Name()));
            m.gameType = type.GetNumber<int>("value", 0);
            m.gameMode = mode.GetNumber<int>("value", 0);
            m.maxPlayers = std::clamp(mode.GetNumber<int>("maxplayers", 10), 2, 64);
            if (const KeyValue* exec = mode.GetSubkey("exec"))
                for (const KeyValue& e : exec->Subkeys()) ReadModeCfgs(gameDir + "cfg\\", std::string(e.String()), m);
            if (m.id == "survival") m.mapPrefix = "dz_";
            if (m.id == "coopmission") m.mapPrefix = "coop_";
            const std::string token(mode.GetString("nameID"));
            m.en = Localized(token, true, m.id);
            m.es = Localized(token, false, m.en);
            if (typeName == "skirmish") { skirmishMode = m; hasSkirmish = true; continue; }  // listed per War Game
            g_modes.push_back(m);
        }
    }
    if (hasSkirmish) AddWarGames(gameDir, skirmishMode);
    if (g_modes.empty()) {  // no readable gamemodes.txt: the classic modes every build has
        ModeOption comp;
        comp.id = "competitive"; comp.en = "Competitive"; comp.es = "Competitivo"; comp.gameMode = 1;
        ModeOption casual;
        casual.id = "casual"; casual.en = casual.es = "Casual"; casual.maxPlayers = 20; casual.defaultRounds = 15;
        g_modes = { comp, casual };
    }
}

const std::vector<ModeOption>& Modes() {
    if (!g_modesLoaded || (!g_namesLocalized && refix::csgo::SchemaManager::Get().IsLoaded())) LoadModes();
    return g_modes;
}

// Maps only one mode can run (Danger Zone, Co-op Strike).
bool IsModeOnlyMap(const std::string& m) { return m.rfind("dz_", 0) == 0 || m.rfind("coop_", 0) == 0; }

struct ServerState {
    bool mapsLoaded = false;
    std::vector<std::string> maps;
    char mapFilter[64] = "";
    std::string map = "de_dust2";
    int mode = 0;
    bool modeChosen = false;  // false until a mode's defaults were applied
    int slots = 10;
    int maxRounds = 30;
    float roundTime = 1.92f;
    int warmupTime = 60;
    int bots = 9;
    int botDifficulty = 2;
    int startMoney = 800;
    int maxMoney = 16000;
    bool friendlyFire = true;
    bool overtime = false;
    bool cheats = false;
    char password[64] = "";
};
ServerState g_server;

int MapGroupRank(const std::string& m) {
    static const char* kPrefixes[] = { "de_", "cs_", "ar_", "dz_", "gd_", "coop_" };
    for (int i = 0; i < 6; ++i)
        if (m.rfind(kPrefixes[i], 0) == 0) return i;
    return 6;
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
    static const char* kGroupEs[] = { "Desactivación", "Rehenes", "Carrera armamentística", "Zona de peligro", "Guardián", "Ataque cooperativo", "Otros" };
    static const char* kGroupEn[] = { "Defusal", "Hostage", "Arms Race", "Danger Zone", "Guardian", "Co-op Strike", "Other" };
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

void ApplyModeDefaults(int i) {
    const auto& modes = Modes();
    if (i < 0 || i >= (int)modes.size()) return;
    if (!g_server.mapsLoaded) LoadMaps();
    const ModeOption& m = modes[i];
    g_server.mode = i;
    g_server.modeChosen = true;
    g_server.slots = m.maxPlayers;
    g_server.maxRounds = m.defaultRounds;
    g_server.roundTime = m.defaultRoundTime;
    g_server.warmupTime = m.defaultWarmupTime;
    g_server.startMoney = m.defaultStartMoney;
    g_server.maxMoney = m.defaultMaxMoney;
    g_server.friendlyFire = m.defaultFriendlyFire;
    g_server.overtime = m.defaultOvertime;
    g_server.bots = (std::min)(m.defaultBots, (std::max)(0, m.maxPlayers - 1));
    g_server.botDifficulty = m.defaultBotDifficulty;
    g_server.cheats = false;
    // Danger Zone and Co-op Strike only run on their own maps, and those maps only in that mode.
    auto fits = [&](const std::string& map) {
        return m.mapPrefix ? map.rfind(m.mapPrefix, 0) == 0 : !IsModeOnlyMap(map);
    };
    if (!fits(g_server.map)) {
        for (const std::string& map : g_server.maps)
            if (fits(map)) { g_server.map = map; break; }
    }
}

// Play-menu names and aliases -> this build's mode list ("wingman", "skirmish_3", "flyingscoutsman"...).
int FindModeIndex(const char* modeId) {
    if (!modeId || !*modeId) return -1;
    std::string m = LowerAscii(modeId);
    static const std::pair<const char*, const char*> kAliases[] = {
        { "comp", "competitive" }, { "wingman", "scrimcomp2v2" }, { "competitive2v2", "scrimcomp2v2" },
        { "weaponsexpert", "scrimcomp5v5" }, { "dm", "deathmatch" }, { "armsrace", "gungameprogressive" },
        { "mg_skirmish_armsrace", "gungameprogressive" }, { "demolition", "gungametrbomb" },
        { "mg_skirmish_demolition", "gungametrbomb" }, { "dangerzone", "survival" }, { "guardian", "cooperative" },
        { "skirmish", "flyingscoutsman" }, { "mg_skirmish_flyingscoutsman", "flyingscoutsman" },
    };
    for (const auto& a : kAliases)
        if (m == a.first) m = a.second;
    const auto& modes = Modes();
    const int skirmishId = m.rfind("skirmish_", 0) == 0 ? atoi(m.c_str() + 9) : 0;
    for (int i = 0; i < (int)modes.size(); ++i) {
        if (skirmishId ? modes[i].skirmish == skirmishId : modes[i].id == m) return i;
    }
    return -1;
}

void DrawSettings(float w, float h) {
    ImGui::BeginChild("##srv_settings", ImVec2(w, h), false);
    float fw = w - ui::S(12);
    ui::SectionLabel(i18n::T("Game mode", "Modo de juego"));
    float modeW = (fw - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
    const auto& modes = Modes();
    for (int i = 0, col = 0; i < (int)modes.size(); ++i, ++col) {
        if (modes[i].skirmish && (i == 0 || !modes[i - 1].skirmish)) {
            Label(i18n::T("WAR GAMES", "JUEGOS DE GUERRA"));
            col = 0;
        }
        if (col % 3) ImGui::SameLine();
        ImGui::PushID(i);
        const char* label = i18n::T(modes[i].en.c_str(), modes[i].es.c_str());
        bool clicked = g_server.mode == i ? ui::PrimaryButton(label, ImVec2(modeW, ui::S(32)))
                                          : ui::SecondaryButton(label, ImVec2(modeW, ui::S(32)));
        ImGui::PopID();
        // Clicking any mode button (even the currently active one) loads that mode's default configuration.
        if (ui::Clicked(clicked)) {
            ApplyModeDefaults(i);
        }
    }

    ui::SectionLabel(i18n::T("Match", "Partida"));
    float half = (fw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginGroup();
    Label(i18n::T("PLAYERS", "JUGADORES"));
    ImGui::SetNextItemWidth(half);
    // This build's own limit for the mode (gamemodes.txt maxplayers).
    const int maxSlots = g_server.mode < (int)modes.size() ? modes[g_server.mode].maxPlayers : 64;
    g_server.slots = std::clamp(g_server.slots, 2, maxSlots);
    if (ImGui::SliderInt("##slots", &g_server.slots, 2, maxSlots)) {
        if (g_server.bots > g_server.slots - 1) g_server.bots = (std::max)(0, g_server.slots - 1);
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    Label(i18n::T("WARMUP TIME (SEC)", "TIEMPO DE CALENTAMIENTO (SEG)"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderInt("##warmup", &g_server.warmupTime, 0, 300, "%d s");
    ImGui::EndGroup();
    ImGui::BeginGroup();
    Label(i18n::T("MAX ROUNDS", "RONDAS MÁXIMAS"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderInt("##rounds", &g_server.maxRounds, 1, 60);
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    Label(i18n::T("ROUND TIME (MIN)", "TIEMPO DE RONDA (MIN)"));
    ImGui::SetNextItemWidth(half);
    ImGui::SliderFloat("##rtime", &g_server.roundTime, 1.0f, 45.0f, "%.2f");
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
    ImGui::SliderInt("##mmoney", &g_server.maxMoney, 0, 65535, "$%d");
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
    if (g_server.mode >= (int)Modes().size()) return;
    nlohmann::json cfg = {
        { "map", g_server.map }, { "mode", Modes()[g_server.mode].id }, { "skirmish", Modes()[g_server.mode].skirmish },
        { "game_type", Modes()[g_server.mode].gameType }, { "game_mode", Modes()[g_server.mode].gameMode },
        { "maxplayers", g_server.slots },
        { "maxrounds", g_server.maxRounds }, { "roundtime", g_server.roundTime },
        { "warmuptime", g_server.warmupTime }, { "startmoney", g_server.startMoney },
        { "maxmoney", g_server.maxMoney }, { "friendlyfire", g_server.friendlyFire ? 1 : 0 }, { "bot_quota", g_server.bots },
        { "bot_difficulty", g_server.botDifficulty }, { "sv_cheats", g_server.cheats ? 1 : 0 },
        { "overtime", g_server.overtime ? 1 : 0 }, { "password", std::string(g_server.password) },
    };
    ReFixGC::SetHostConfigJson(cfg.dump());
    ReFixGC::HostLanMatch();
    App::Get().ShowInfo(i18n::T("LAN server", "Servidor LAN"), std::string(i18n::T("Starting ", "Iniciando ")) + g_server.map);
    csgo_ui::CloseHud(csgo_ui::kServerHud);
}

} // namespace

void csgo_ui::SetServerMode(const char* modeId) {
    const int idx = FindModeIndex(modeId);
    if (idx >= 0) {
        ApplyModeDefaults(idx);
    }
}

void csgo_ui::DrawServerHud(float viewportW, float viewportH) {
    if (!g_server.mapsLoaded) LoadMaps();
    if (!g_server.modeChosen) {
        const int competitive = FindModeIndex("competitive");
        ApplyModeDefaults(competitive >= 0 ? competitive : 0);
    }

    const ImVec2 vp(viewportW, viewportH);
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
            csgo_ui::CloseHud(csgo_ui::kServerHud);
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
            csgo_ui::CloseHud(csgo_ui::kServerHud);
        }
        ImGui::SameLine();
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Host match", "Hostear partida"), ImVec2(btnW, ui::S(40))))) HostNow();
    }
    ImGui::End();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
}

} // namespace refix::overlay
