#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "overlay_ui_mods.h"
#include "overlay_theme.h"
#include "overlay_i18n.h"
#include "overlay_app.h"
#include "refix_overlay.h"
#include "../mods/mod_host.h"
#include "../steam_gc_emu.h"
#include "../unreal_steam_emu.h"
#include "presence_net.h"
#include <algorithm>
#include <vector>
#include <string>

extern "C" void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed);

namespace refix::overlay {

void ExecuteEngineClientCommand(const char* cmd) {
    if (!cmd || !*cmd) return;
    HMODULE hEngine = GetModuleHandleA("engine.dll");
    if (!hEngine) return;
    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    auto factory = (CreateInterfaceFn)GetProcAddress(hEngine, "CreateInterface");
    if (!factory) return;
    void* engineClient = factory("VEngineClient014", nullptr);
    if (!engineClient) return;

    typedef void(__thiscall* ExecuteClientCmdFn)(void*, const char*);
    void** vtable = *(void***)engineClient;
    if (vtable && vtable[108]) {
        auto fn = (ExecuteClientCmdFn)vtable[108];
        fn(engineClient, cmd);
    }
}

static bool s_showMarketGiveModal = false;
static bool s_showMatchmakingWidget = false;
static bool s_showCustomSettings = false;

static int s_selectedMapIdx = 0;
static int s_selectedModeIdx = 0;
static bool s_cfgBots = true;
static bool s_cfgFastRounds = false; // MR12 vs MR15
static bool s_cfgCompMoney = true;   // 16K start money
static bool s_cfgStatTrak = false;
static char s_marketSearch[128] = "";

static const char* kOfficialMaps[] = {
    "de_dust2", "de_mirage", "de_inferno", "de_nuke",
    "de_overpass", "de_vertigo", "de_ancient", "de_cache", "cs_office"
};
static const size_t kNumOfficialMaps = sizeof(kOfficialMaps) / sizeof(kOfficialMaps[0]);

static const char* kGameModes[] = {
    "Competitive (5v5 Clásico)", "Casual (10v10)", "Deathmatch", "Wingman (2v2 Compañero)", "Arms Race"
};

struct MarketItemEntry {
    uint32_t defIndex;
    const char* name;
    const char* category;
    uint32_t quality;
    uint32_t rarity;
    uint32_t paintKit;
    float wear;
    uint32_t seed;
};

static const MarketItemEntry kCasesMarket[] = {
    { 4001, "CS:GO Weapon Case 1", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4002, "eSports 2013 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4003, "Operation Bravo Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4004, "CS:GO Weapon Case 2", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4005, "eSports 2013 Winter Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4009, "Winter Offensive Weapon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4010, "CS:GO Weapon Case 3", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4011, "Operation Phoenix Weapon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4017, "Huntsman Weapon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4018, "Operation Breakout Weapon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4019, "eSports 2014 Summer Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4029, "Operation Vanguard Weapon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4061, "Chroma Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4089, "Chroma 2 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4091, "Falchion Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4138, "Shadow Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4186, "Revolver Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4187, "Operation Wildfire Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4233, "Chroma 3 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4236, "Gamma Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4281, "Gamma 2 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4288, "Glove Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4351, "Spectrum Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4352, "Operation Hydra Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4353, "Spectrum 2 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4403, "Clutch Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4471, "Horizon Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4548, "Danger Zone Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4598, "Prisma Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4620, "CS20 Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4669, "Shattered Web Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4698, "Fracture Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4790, "Snakebite Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4846, "Dreams & Nightmares Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4874, "Recoil Case", "Caja", 4, 0, 0, 0.0f, 0 },
    { 4901, "Revolution Case", "Caja", 4, 0, 0, 0.0f, 0 },
};

static const MarketItemEntry kKeysMarket[] = {
    { 1203, "CS:GO Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1204, "eSports Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1214, "Winter Offensive Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1303, "Operation Phoenix Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1305, "Huntsman Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1309, "Operation Breakout Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1313, "Operation Vanguard Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1322, "Chroma Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1325, "Chroma 2 Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1330, "Falchion Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1333, "Shadow Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1334, "Revolver Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 7000, "Operation Wildfire Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1347, "Chroma 3 Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1350, "Gamma Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1351, "Gamma 2 Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1356, "Glove Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1364, "Spectrum Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1365, "Operation Hydra Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1366, "Spectrum 2 Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1373, "Clutch Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1374, "Horizon Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1375, "Danger Zone Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1383, "Prisma Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1384, "CS20 Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1385, "Shattered Web Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1391, "Fracture Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1398, "Snakebite Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1403, "Dreams & Nightmares Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1405, "Recoil Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
    { 1407, "Revolution Case Key", "Llave", 4, 0, 0, 0.0f, 0 },
};

static const MarketItemEntry kRiflesMarket[] = {
    { 9,   "AWP | Dragon Lore", "Rifle", 4, 6, 344, 0.001f, 420 },
    { 9,   "AWP | Asiimov", "Rifle", 4, 6, 279, 0.18f, 137 },
    { 9,   "AWP | Hyper Beast", "Rifle", 4, 6, 475, 0.02f, 77 },
    { 9,   "AWP | Medusa", "Rifle", 4, 6, 446, 0.005f, 999 },
    { 7,   "AK-47 | Fire Serpent", "Rifle", 4, 6, 180, 0.06f, 321 },
    { 7,   "AK-47 | Asiimov", "Rifle", 4, 6, 801, 0.05f, 555 },
    { 7,   "AK-47 | Bloodsport", "Rifle", 4, 6, 639, 0.01f, 888 },
    { 7,   "AK-47 | Vulcan", "Rifle", 4, 6, 302, 0.02f, 101 },
    { 7,   "AK-47 | Redline", "Rifle", 4, 5, 282, 0.15f, 202 },
    { 16,  "M4A4 | Howl", "Rifle", 4, 8, 309, 0.03f, 666 },
    { 16,  "M4A4 | Asiimov", "Rifle", 4, 6, 255, 0.19f, 404 },
    { 16,  "M4A4 | Neo-Noir", "Rifle", 4, 6, 695, 0.04f, 512 },
    { 60,  "M4A1-S | Hyper Beast", "Rifle", 4, 6, 430, 0.01f, 333 },
    { 60,  "M4A1-S | Printstream", "Rifle", 4, 6, 757, 0.02f, 707 },
    { 60,  "M4A1-S | Decimator", "Rifle", 4, 5, 644, 0.03f, 818 },
};

static const MarketItemEntry kPistolsMarket[] = {
    { 1,   "Desert Eagle | Printstream", "Pistola", 4, 6, 757, 0.01f, 101 },
    { 1,   "Desert Eagle | Code Red", "Pistola", 4, 6, 711, 0.02f, 202 },
    { 1,   "Desert Eagle | Kumicho Dragon", "Pistola", 4, 5, 527, 0.03f, 303 },
    { 4,   "Glock-18 | Fade", "Pistola", 4, 6, 38, 0.005f, 415 },
    { 4,   "Glock-18 | Water Elemental", "Pistola", 4, 5, 353, 0.02f, 505 },
    { 61,  "USP-S | Kill Confirmed", "Pistola", 4, 6, 504, 0.01f, 606 },
    { 61,  "USP-S | Neo-Noir", "Pistola", 4, 6, 653, 0.02f, 707 },
};

static const MarketItemEntry kKnivesMarket[] = {
    { 507, "Karambit | Doppler (Sapphire)", "Cuchillo", 3, 7, 416, 0.001f, 420 },
    { 507, "Karambit | Doppler (Ruby)", "Cuchillo", 3, 7, 415, 0.001f, 777 },
    { 507, "Karambit | Fade", "Cuchillo", 3, 7, 38, 0.005f, 999 },
    { 515, "Butterfly Knife | Fade", "Cuchillo", 3, 7, 38, 0.005f, 415 },
    { 515, "Butterfly Knife | Doppler", "Cuchillo", 3, 7, 418, 0.008f, 101 },
    { 515, "Butterfly Knife | Gamma Doppler (Emerald)", "Cuchillo", 3, 7, 568, 0.001f, 888 },
    { 508, "M9 Bayonet | Lore", "Cuchillo", 3, 7, 562, 0.01f, 555 },
    { 508, "M9 Bayonet | Doppler", "Cuchillo", 3, 7, 418, 0.005f, 666 },
    { 500, "Bayonet | Doppler (Phase 2)", "Cuchillo", 3, 7, 419, 0.01f, 321 },
    { 523, "Talon Knife | Tiger Tooth", "Cuchillo", 3, 7, 413, 0.02f, 137 },
    { 525, "Skeleton Knife | Crimson Web", "Cuchillo", 3, 7, 12, 0.06f, 909 },
    { 516, "Shadow Daggers | Doppler (Ruby)", "Cuchillo", 3, 7, 415, 0.001f, 123 },
    { 509, "Huntsman Knife | Fade", "Cuchillo", 3, 7, 38, 0.005f, 234 },
    { 522, "Stiletto Knife | Tiger Tooth", "Cuchillo", 3, 7, 413, 0.01f, 345 },
};

static const MarketItemEntry kGlovesAndAgents[] = {
    { 5030, "Sport Gloves | Vice", "Guantes", 3, 7, 10048, 0.06f, 420 },
    { 5031, "Driver Gloves | King Snake", "Guantes", 3, 7, 10041, 0.07f, 777 },
    { 5032, "Hand Wraps | Cobalt Skulls", "Guantes", 3, 7, 10053, 0.08f, 101 },
    { 5033, "Moto Gloves | Spearmint", "Guantes", 3, 7, 10026, 0.09f, 202 },
    { 5034, "Specialist Gloves | Crimson Kimono", "Guantes", 3, 7, 10033, 0.05f, 303 },
};

static const MarketItemEntry kStickersMarket[] = {
    { 6000, "Pegatina | Titan (Holo) | Katowice 2014", "Pegatina", 4, 7, 0, 0.0f, 0 },
    { 6001, "Pegatina | iBUYPOWER (Holo) | Katowice 2014", "Pegatina", 4, 7, 0, 0.0f, 0 },
    { 6002, "Pegatina | Howling Dawn", "Pegatina", 4, 8, 0, 0.0f, 0 },
    { 6003, "Pegatina | Corona (Foil / Crown Foil)", "Pegatina", 4, 6, 0, 0.0f, 0 },
};

void OpenMarketGiveMenu() {
    s_showMarketGiveModal = true;
    App::Get().OpenOverlay(false);
}

void OpenHostMatchModal() {
    s_showMatchmakingWidget = true;
    App::Get().OpenOverlay(false);
}

void OpenMatchmakingToast() {
    s_showMatchmakingWidget = true;
    App::Get().OpenOverlay(false);
}

static ImU32 GetRarityColor(uint32_t rarity) {
    switch (rarity) {
    case 8: return IM_COL32(235, 120, 30, 255);  // Contraband (Orange)
    case 7: return IM_COL32(255, 215, 0, 255);   // Gold / Special (Yellow)
    case 6: return IM_COL32(235, 75, 75, 255);   // Covert (Red)
    case 5: return IM_COL32(211, 44, 230, 255);  // Classified (Pink)
    case 4: return IM_COL32(136, 71, 255, 255);  // Restricted (Purple)
    case 3: return IM_COL32(75, 105, 255, 255);  // Mil-Spec (Blue)
    case 2: return IM_COL32(94, 152, 217, 255);  // Industrial Grade (Light Blue)
    default: return IM_COL32(176, 195, 217, 255); // Consumer Grade (Grey)
    }
}

static void RenderItemsList(const MarketItemEntry* items, size_t count, int idOffset) {
    std::string filter = s_marketSearch;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    ImGui::BeginChild("##items_scroll_area", ImVec2(0, ui::S(380)), false);
    for (size_t i = 0; i < count; ++i) {
        const auto& itm = items[i];
        if (!filter.empty()) {
            std::string n = itm.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(filter) == std::string::npos) continue;
        }

        ImGui::PushID((int)i + idOffset);

        // Color badge for rarity
        ImU32 rCol = GetRarityColor(itm.rarity);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + ui::S(4), ImGui::GetCursorScreenPos().y + ui::S(22)),
            rCol, ui::S(2));

        ImGui::Dummy(ImVec2(ui::S(8), 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(itm.name);

        ImGui::SameLine();
        char catBuf[64];
        snprintf(catBuf, sizeof(catBuf), "[%s]", itm.category);
        ui::TextColored(ui::color::Muted, catBuf);

        ImGui::SameLine(ui::S(590));
        uint32_t finalQual = (s_cfgStatTrak && itm.paintKit > 0) ? 9 : itm.quality;
        const char* btnLabel = (finalQual == 9) ? "+ StatTrak##give" : "Añadir##give";
        if (ui::Clicked(ui::PrimaryButton(btnLabel, ImVec2(ui::S(140), ui::S(24))))) {
            CSGO_GrantItemToLocal(itm.defIndex, 1, finalQual, itm.rarity, itm.paintKit, itm.wear, itm.seed);
            App::Get().ShowInfo("Inventario Actualizado", std::string("Se ha otorgado ") + itm.name);
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void DrawMarketGiveModal() {
    if (!s_showMarketGiveModal) return;

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float winW = (std::min)(ui::S(840), vp.x - ui::S(40));
    float winH = (std::min)(ui::S(580), vp.y - ui::S(40));
    ImGui::SetNextWindowPos(ImVec2((vp.x - winW) * 0.5f, (vp.y - winH) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(20), ui::S(18)));

    if (ImGui::Begin("Mercado ReFix - Gestor de Inventario Local##refix_market", &s_showMarketGiveModal,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

        // Header Title & Close Button
        ImGui::PushFont(ui::F().heading);
        ui::TextColored(ui::color::Accent, ui::icon::Store);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Community Market & Inventory Manager", "Mercado ReFix y Gestor de Inventario"));
        ImGui::PopFont();

        ImGui::SameLine(winW - ui::S(56));
        if (ui::Clicked(ui::NavButton("X##close_market_x", ImVec2(ui::S(28), ui::S(26))))) {
            s_showMarketGiveModal = false;
        }

        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, i18n::T("Add official cases, keys, knives, and custom skins directly to your profile inventory.",
                                                  "Añade cajas, llaves, cuchillos y skins oficiales directamente a tu inventario."));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(4)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Search Bar & StatTrak Toggle
        ImGui::SetNextItemWidth(ui::S(460));
        ImGui::InputTextWithHint("##market_search", "Buscar arma, cuchillo, caja o pegatina...", s_marketSearch, sizeof(s_marketSearch));

        ImGui::SameLine(ui::S(510));
        ImGui::Checkbox("StatTrak (Contador de Bajas)", &s_cfgStatTrak);

        ImGui::Dummy(ImVec2(0, ui::S(4)));

        if (ImGui::BeginTabBar("##market_tabs")) {
            // TAB 1: WEAPON CASES
            if (ImGui::BeginTabItem(i18n::T("Weapon Cases (36)", "Cajas de Armas (36)"))) {
                std::string filter = s_marketSearch;
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

                ImGui::BeginChild("##cases_list", ImVec2(0, ui::S(360)), false);
                for (size_t i = 0; i < sizeof(kCasesMarket) / sizeof(kCasesMarket[0]); ++i) {
                    if (!filter.empty()) {
                        std::string n = kCasesMarket[i].name;
                        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                        if (n.find(filter) == std::string::npos) continue;
                    }
                    ImGui::PushID((int)i);
                    ImGui::Text("%s", kCasesMarket[i].name);
                    ImGui::SameLine(ui::S(420));
                    if (ui::Clicked(ui::NavButton("+1##case", ImVec2(ui::S(55), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 1, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Caja Añadida", std::string("Se ha añadido 1x ") + kCasesMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::NavButton("+5##case", ImVec2(ui::S(55), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 5, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Cajas Añadidas", std::string("Se han añadido 5x ") + kCasesMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::PrimaryButton("+20##case", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 20, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Cajas Añadidas", std::string("Se han añadido 20x ") + kCasesMarket[i].name);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // TAB 2: CASE KEYS
            if (ImGui::BeginTabItem(i18n::T("Case Keys (36)", "Llaves de Cajas (36)"))) {
                std::string filter = s_marketSearch;
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

                ImGui::BeginChild("##keys_list", ImVec2(0, ui::S(360)), false);
                for (size_t i = 0; i < sizeof(kKeysMarket) / sizeof(kKeysMarket[0]); ++i) {
                    if (!filter.empty()) {
                        std::string n = kKeysMarket[i].name;
                        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                        if (n.find(filter) == std::string::npos) continue;
                    }
                    ImGui::PushID((int)i + 1000);
                    ImGui::Text("%s", kKeysMarket[i].name);
                    ImGui::SameLine(ui::S(420));
                    if (ui::Clicked(ui::NavButton("+1##key", ImVec2(ui::S(55), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 1, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Llave Añadida", std::string("Se ha añadido 1x ") + kKeysMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::NavButton("+5##key", ImVec2(ui::S(55), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 5, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Llaves Añadidas", std::string("Se han añadido 5x ") + kKeysMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::PrimaryButton("+20##key", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 20, 4, 0, 0, 0.0f, 0);
                        App::Get().ShowInfo("Llaves Añadidas", std::string("Se han añadido 20x ") + kKeysMarket[i].name);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // TAB 3: RIFLES
            if (ImGui::BeginTabItem(i18n::T("Rifles & Snipers", "Rifles & Snipers"))) {
                RenderItemsList(kRiflesMarket, sizeof(kRiflesMarket) / sizeof(kRiflesMarket[0]), 2000);
                ImGui::EndTabItem();
            }

            // TAB 4: PISTOLS
            if (ImGui::BeginTabItem(i18n::T("Pistols", "Pistolas"))) {
                RenderItemsList(kPistolsMarket, sizeof(kPistolsMarket) / sizeof(kPistolsMarket[0]), 3000);
                ImGui::EndTabItem();
            }

            // TAB 5: KNIVES
            if (ImGui::BeginTabItem(i18n::T("Knives", "Cuchillos"))) {
                RenderItemsList(kKnivesMarket, sizeof(kKnivesMarket) / sizeof(kKnivesMarket[0]), 4000);
                ImGui::EndTabItem();
            }

            // TAB 6: GLOVES & AGENTS
            if (ImGui::BeginTabItem(i18n::T("Gloves & Agents", "Guantes & Agentes"))) {
                RenderItemsList(kGlovesAndAgents, sizeof(kGlovesAndAgents) / sizeof(kGlovesAndAgents[0]), 5000);
                ImGui::EndTabItem();
            }

            // TAB 7: STICKERS
            if (ImGui::BeginTabItem(i18n::T("Stickers", "Pegatinas"))) {
                RenderItemsList(kStickersMarket, sizeof(kStickersMarket) / sizeof(kStickersMarket[0]), 6000);
                ImGui::EndTabItem();
            }

            // TAB 8: PRIME UPGRADE
            if (ImGui::BeginTabItem(i18n::T("Prime Status", "Estado Prime"))) {
                ImGui::Dummy(ImVec2(0, ui::S(10)));
                ImGui::PushFont(ui::F().strong);
                ui::TextColored(ui::color::Online, i18n::T("CS:GO Prime Status Upgrade", "Mejora de Estado Prime Oficial"));
                ImGui::PopFont();
                ImGui::TextWrapped(i18n::T("Prime Status matches you with other Prime Status players, grants access to Prime-exclusive souvenir items, item drops, and weapon cases.",
                                         "El estado Prime te empareja con otros jugadores Prime, otorga acceso a cajas de armas exclusivas, drops semanales e insignias de servicio."));
                ImGui::Dummy(ImVec2(0, ui::S(16)));

                if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Prime Status (Free via ReFix)", "Activar Estado Prime (Gratis vía ReFix)"), ImVec2(ui::S(320), ui::S(36))))) {
                    ::ReFixOverlay::PromptPurchase(730, 10001, "CS:GO Prime Status Upgrade",
                        "Activación de Estado Prime para el perfil actual.", false, [](bool ok) {
                            if (ok) {
                                App::Get().ShowInfo("Prime Activado", "El Estado Prime ha sido habilitado con éxito para este perfil.");
                            }
                        });
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Dummy(ImVec2(0, ui::S(6)));
        if (ui::Clicked(ui::NavButton(i18n::T("Close Menu", "Cerrar Menú"), ImVec2(ui::S(140), ui::S(28))))) {
            s_showMarketGiveModal = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void DrawMatchmakingToastWidget() {
    if (!s_showMatchmakingWidget) return;

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    const float cardW = ui::S(380);
    const float pad = ui::S(24);
    ImGui::SetNextWindowPos(ImVec2(vp.x - cardW - pad, vp.y - pad), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(cardW, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(18), ui::S(16)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);

    if (ImGui::Begin("##matchmaking_toast_widget", &s_showMatchmakingWidget,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav)) {

        // Header
        ImGui::PushFont(ui::F().strong);
        ui::TextColored(ui::color::Accent, ui::icon::Game);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Matchmaking LAN (P2P)", "Partida LAN / Matchmaking"));
        ImGui::PopFont();

        ImGui::SameLine(cardW - ui::S(46));
        if (ui::Clicked(ui::NavButton("X##close_mm_toast", ImVec2(ui::S(24), ui::S(22))))) {
            s_showMatchmakingWidget = false;
            App::Get().CloseOverlay();
        }

        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, i18n::T("Play with nearby friends on local network", "Juega con amigos en tu red local"));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(4)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Action 1: Search LAN Match
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Search LAN Match", "Buscar Partida LAN"), ImVec2(cardW - ui::S(36), ui::S(36))))) {
            std::string connectCmd = "connect 127.0.0.1:27015";
            auto peers = net::Players();
            for (const auto& p : peers) {
                if (p.appId == 730 && !p.connect.empty()) {
                    connectCmd = p.connect;
                    break;
                } else if (p.appId == 730 && p.ip != 0) {
                    char ipBuf[64];
                    snprintf(ipBuf, sizeof(ipBuf), "connect %u.%u.%u.%u:27015",
                             (p.ip >> 24) & 0xFF, (p.ip >> 16) & 0xFF, (p.ip >> 8) & 0xFF, p.ip & 0xFF);
                    connectCmd = ipBuf;
                    break;
                }
            }
            ExecuteEngineClientCommand(connectCmd.c_str());
            App::Get().ShowInfo("Matchmaking LAN", "Conectando con servidor en red local LAN...");
            s_showMatchmakingWidget = false;
            App::Get().CloseOverlay();
        }

        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Action 2: Create LAN Match + Settings Toggle Button next to it
        float createW = cardW - ui::S(36) - ui::S(44);
        if (ui::Clicked(ui::NavButton(i18n::T("Create LAN Match", "Crear Partida (Host)"), ImVec2(createW, ui::S(34))))) {
            std::string cmd = "game_type 0; game_mode 1; sv_lan 1; bot_quota 10; map de_dust2";
            ExecuteEngineClientCommand(cmd.c_str());
            App::Get().ShowInfo("Servidor LAN", "Iniciando servidor de partida local (de_dust2)...");
            s_showMatchmakingWidget = false;
            App::Get().CloseOverlay();
        }

        ImGui::SameLine();
        const char* gearIcon = s_showCustomSettings ? "[-##cfg]" : "[⚙##cfg]";
        if (ui::Clicked(ui::NavButton(gearIcon, ImVec2(ui::S(38), ui::S(34))))) {
            s_showCustomSettings = !s_showCustomSettings;
        }

        // Expandable Custom Settings
        if (s_showCustomSettings) {
            ImGui::Dummy(ImVec2(0, ui::S(4)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, ui::S(4)));

            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Map:", "Mapa:"));
            ImGui::PopFont();
            ImGui::SetNextItemWidth(cardW - ui::S(36));
            if (ImGui::BeginCombo("##toast_map_combo", kOfficialMaps[s_selectedMapIdx])) {
                for (size_t i = 0; i < kNumOfficialMaps; ++i) {
                    bool sel = (s_selectedMapIdx == (int)i);
                    if (ImGui::Selectable(kOfficialMaps[i], sel)) s_selectedMapIdx = (int)i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Mode:", "Modo:"));
            ImGui::PopFont();
            ImGui::SetNextItemWidth(cardW - ui::S(36));
            if (ImGui::BeginCombo("##toast_mode_combo", kGameModes[s_selectedModeIdx])) {
                for (int i = 0; i < 5; ++i) {
                    bool sel = (s_selectedModeIdx == i);
                    if (ImGui::Selectable(kGameModes[i], sel)) s_selectedModeIdx = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox(i18n::T("Bots (5v5 AI)", "Habilitar Bots (5v5 IA)"), &s_cfgBots);
            ImGui::Checkbox(i18n::T("Fast Rounds MR12", "Rondas Rápidas MR12"), &s_cfgFastRounds);
            ImGui::Checkbox(i18n::T("Start Money $16K", "Dinero Inicial $16,000"), &s_cfgCompMoney);

            ImGui::Dummy(ImVec2(0, ui::S(4)));
            if (ui::Clicked(ui::PrimaryButton(i18n::T("Launch Custom Server", "Lanzar Servidor Personalizado"), ImVec2(cardW - ui::S(36), ui::S(32))))) {
                char cmd[256];
                int gType = 0, gMode = 1;
                if (s_selectedModeIdx == 1) { gType = 0; gMode = 0; } // Casual
                else if (s_selectedModeIdx == 2) { gType = 1; gMode = 2; } // Deathmatch
                else if (s_selectedModeIdx == 3) { gType = 0; gMode = 2; } // Wingman
                else if (s_selectedModeIdx == 4) { gType = 1; gMode = 0; } // Arms race

                snprintf(cmd, sizeof(cmd), "game_type %d; game_mode %d; sv_lan 1; %s; %s; %s; map %s",
                         gType, gMode,
                         s_cfgBots ? "bot_quota 10; bot_difficulty 2" : "bot_kick",
                         s_cfgFastRounds ? "mp_maxrounds 24" : "mp_maxrounds 30",
                         s_cfgCompMoney ? "mp_startmoney 16000; mp_maxmoney 16000" : "mp_startmoney 800",
                         kOfficialMaps[s_selectedMapIdx]);

                ExecuteEngineClientCommand(cmd);
                App::Get().ShowInfo("Servidor Personalizado", std::string("Iniciando ") + kOfficialMaps[s_selectedMapIdx]);
                s_showMatchmakingWidget = false;
                App::Get().CloseOverlay();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DrawModsCard(ImVec2 pos, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 max(pos.x + width, pos.y + height);

    ui::Card(dl, pos, max, ui::color::Panel, 14.0f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + ui::S(16), pos.y + ui::S(14)));
    ImGui::BeginGroup();

    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Accent, ui::icon::Store);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("CS:GO Mod & Market", "Mods & Mercado CS:GO"));
    ImGui::PopFont();

    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Muted, i18n::T("Manage inventory items, keys, and LAN matchmaking",
                                              "Gestor de items, cajas, llaves y matchmaking"));
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(4)));

    if (ui::Clicked(ui::PrimaryButton(i18n::T("Community Market & Give##mods_btn", "Mercado y Give Menu##mods_btn"),
                                      ImVec2(ui::S(190), ui::S(28))))) {
        OpenMarketGiveMenu();
    }

    ImGui::SameLine();
    if (ui::Clicked(ui::NavButton(i18n::T("Matchmaking Toast##mods_mm", "Partida LAN##mods_mm"),
                                  ImVec2(ui::S(140), ui::S(28))))) {
        OpenMatchmakingToast();
    }

    ImGui::EndGroup();
}

void DrawModsApprovalModal() {}

} // namespace refix::overlay
