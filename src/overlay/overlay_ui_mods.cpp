#include "overlay_ui_mods.h"
#include "overlay_theme.h"
#include "overlay_i18n.h"
#include "overlay_app.h"
#include "refix_overlay.h"
#include "../mods/mod_host.h"
#include "../steam_gc_emu.h"
#include "../unreal_steam_emu.h"
#include <algorithm>
#include <vector>
#include <string>

extern "C" void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity);

namespace refix::overlay {

static bool s_showModsModal = false;
static bool s_showMarketGiveModal = false;
static bool s_showHostMatchModal = false;

static int s_selectedMapIdx = 0;
static int s_selectedModeIdx = 0;
static bool s_cfgBots = true;
static bool s_cfgFastRounds = false; // MR12 vs MR15
static bool s_cfgCompMoney = true;   // 16K start money
static bool s_cfgNoWarmup = true;
static bool s_cfgBuyAnywhere = false;
static char s_directConnectIp[64] = "127.0.0.1:27015";

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
};

static const MarketItemEntry kCasesMarket[] = {
    { 4001, "CS:GO Weapon Case 1", "Caja", 4, 0 },
    { 4002, "eSports 2013 Case", "Caja", 4, 0 },
    { 4003, "Operation Bravo Case", "Caja", 4, 0 },
    { 4004, "CS:GO Weapon Case 2", "Caja", 4, 0 },
    { 4005, "eSports 2013 Winter Case", "Caja", 4, 0 },
    { 4009, "Winter Offensive Weapon Case", "Caja", 4, 0 },
    { 4010, "CS:GO Weapon Case 3", "Caja", 4, 0 },
    { 4011, "Operation Phoenix Weapon Case", "Caja", 4, 0 },
    { 4017, "Huntsman Weapon Case", "Caja", 4, 0 },
    { 4018, "Operation Breakout Weapon Case", "Caja", 4, 0 },
    { 4019, "eSports 2014 Summer Case", "Caja", 4, 0 },
    { 4029, "Operation Vanguard Weapon Case", "Caja", 4, 0 },
    { 4061, "Chroma Case", "Caja", 4, 0 },
    { 4089, "Chroma 2 Case", "Caja", 4, 0 },
    { 4091, "Falchion Case", "Caja", 4, 0 },
    { 4138, "Shadow Case", "Caja", 4, 0 },
    { 4186, "Revolver Case", "Caja", 4, 0 },
    { 4187, "Operation Wildfire Case", "Caja", 4, 0 },
    { 4233, "Chroma 3 Case", "Caja", 4, 0 },
    { 4236, "Gamma Case", "Caja", 4, 0 },
    { 4281, "Gamma 2 Case", "Caja", 4, 0 },
    { 4288, "Glove Case", "Caja", 4, 0 },
    { 4351, "Spectrum Case", "Caja", 4, 0 },
    { 4352, "Operation Hydra Case", "Caja", 4, 0 },
    { 4353, "Spectrum 2 Case", "Caja", 4, 0 },
    { 4403, "Clutch Case", "Caja", 4, 0 },
    { 4471, "Horizon Case", "Caja", 4, 0 },
    { 4548, "Danger Zone Case", "Caja", 4, 0 },
    { 4598, "Prisma Case", "Caja", 4, 0 },
    { 4620, "CS20 Case", "Caja", 4, 0 },
    { 4669, "Shattered Web Case", "Caja", 4, 0 },
    { 4698, "Fracture Case", "Caja", 4, 0 },
    { 4790, "Snakebite Case", "Caja", 4, 0 },
    { 4846, "Dreams & Nightmares Case", "Caja", 4, 0 },
    { 4874, "Recoil Case", "Caja", 4, 0 },
    { 4901, "Revolution Case", "Caja", 4, 0 },
};

static const MarketItemEntry kKeysMarket[] = {
    { 1203, "CS:GO Case Key", "Llave", 4, 0 },
    { 1204, "eSports Key", "Llave", 4, 0 },
    { 1214, "Winter Offensive Key", "Llave", 4, 0 },
    { 1303, "Operation Phoenix Key", "Llave", 4, 0 },
    { 1305, "Huntsman Case Key", "Llave", 4, 0 },
    { 1309, "Operation Breakout Key", "Llave", 4, 0 },
    { 1313, "Operation Vanguard Key", "Llave", 4, 0 },
    { 1322, "Chroma Case Key", "Llave", 4, 0 },
    { 1325, "Chroma 2 Case Key", "Llave", 4, 0 },
    { 1330, "Falchion Case Key", "Llave", 4, 0 },
    { 1333, "Shadow Case Key", "Llave", 4, 0 },
    { 1334, "Revolver Case Key", "Llave", 4, 0 },
    { 7000, "Operation Wildfire Key", "Llave", 4, 0 },
    { 1347, "Chroma 3 Case Key", "Llave", 4, 0 },
    { 1350, "Gamma Case Key", "Llave", 4, 0 },
    { 1351, "Gamma 2 Case Key", "Llave", 4, 0 },
    { 1356, "Glove Case Key", "Llave", 4, 0 },
    { 1364, "Spectrum Case Key", "Llave", 4, 0 },
    { 1365, "Operation Hydra Key", "Llave", 4, 0 },
    { 1366, "Spectrum 2 Case Key", "Llave", 4, 0 },
    { 1373, "Clutch Case Key", "Llave", 4, 0 },
    { 1374, "Horizon Case Key", "Llave", 4, 0 },
    { 1375, "Danger Zone Case Key", "Llave", 4, 0 },
    { 1383, "Prisma Case Key", "Llave", 4, 0 },
    { 1384, "CS20 Case Key", "Llave", 4, 0 },
    { 1385, "Shattered Web Case Key", "Llave", 4, 0 },
};

static const MarketItemEntry kSkinsMarket[] = {
    { 507, "Karambit | Doppler (Phase 2)", "Cuchillo", 4, 6 },
    { 515, "Butterfly Knife | Fade", "Cuchillo", 4, 6 },
    { 508, "M9 Bayonet | Marble Fade", "Cuchillo", 4, 6 },
    { 523, "Talon Knife | Tiger Tooth", "Cuchillo", 4, 6 },
    { 525, "Skeleton Knife | Crimson Web", "Cuchillo", 4, 6 },
    { 500, "Bayonet | Lore", "Cuchillo", 4, 6 },
    { 9,   "AWP | Dragon Lore", "Arma", 4, 6 },
    { 9,   "AWP | Asiimov", "Arma", 4, 5 },
    { 7,   "AK-47 | Fire Serpent", "Arma", 4, 6 },
    { 7,   "AK-47 | Redline", "Arma", 4, 4 },
    { 4,   "M4A4 | Howl (Contraband)", "Arma", 5, 7 },
    { 4,   "M4A4 | Asiimov", "Arma", 4, 5 },
    { 16,  "M4A1-S | Printstream", "Arma", 4, 6 },
    { 16,  "M4A1-S | Hyper Beast", "Arma", 4, 6 },
    { 1,   "Desert Eagle | Blaze", "Arma", 4, 5 },
    { 61,  "USP-S | Kill Confirmed", "Arma", 4, 6 },
};

void OpenMarketGiveMenu() {
    s_showMarketGiveModal = true;
}

void OpenHostMatchModal() {
    s_showHostMatchModal = true;
}

void ExecuteEngineCmd(const char* cmd) {
    if (!cmd || !*cmd) return;
    HMODULE hEngine = GetModuleHandleW(L"engine.dll");
    if (!hEngine) return;
    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    CreateInterfaceFn createIface = (CreateInterfaceFn)GetProcAddress(hEngine, "CreateInterface");
    if (!createIface) return;
    void* engineClient = createIface("VEngineClient014", nullptr);
    if (!engineClient) return;
    void** vtable = *(void***)engineClient;
    if (vtable && vtable[108]) {
        typedef void (__thiscall* CmdFn)(void*, const char*, bool);
        CmdFn cmdFn = (CmdFn)vtable[108];
        cmdFn(engineClient, cmd, false);
    }
}

void DrawModsCard(ImVec2 pos, float width, float height) {
    ui::BeginCard("##mods_card", pos, ImVec2(width, height));

    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Accent, ui::icon::Workshop);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("ReFix CS:GO LAN Suite", "Suite ReFix CS:GO LAN"));
    ImGui::PopFont();

    auto& host = mods::ModHost::GetInstance();
    const auto& loaded = host.GetLoadedMods();

    int active_count = 0;
    for (const auto& [id, mod] : loaded) {
        if (mod.is_active) active_count++;
    }

    ImGui::PushFont(ui::F().caption);
    char modCountBuf[64];
    snprintf(modCountBuf, sizeof(modCountBuf), i18n::T("Active mods: %d", "Mods activos: %d"), active_count);
    ui::TextColored(ui::color::Muted, modCountBuf);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(4)));
    if (ui::Clicked(ui::NavButton(i18n::T("Manage Mods##btn", "Administrar Mods##btn"), ImVec2(width - ui::S(24), ui::S(26))))) {
        s_showModsModal = true;
    }

    if (UnrealSteamEmu::GetAppID() == 730) {
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(2)));

        if (ui::Clicked(ui::PrimaryButton(i18n::T("Community Market & Give Menu", "Mercado y Give Menu CS:GO"), ImVec2(width - ui::S(24), ui::S(30))))) {
            s_showMarketGiveModal = true;
        }

        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::NavButton(i18n::T("Host P2P / LAN Match", "Crear Partida / Servidor LAN"), ImVec2(width - ui::S(24), ui::S(30))))) {
            s_showHostMatchModal = true;
        }
    }

    ui::EndCard();

    DrawModsApprovalModal();
    DrawMarketGiveModal();
    DrawHostMatchModal();
}

void DrawModsApprovalModal() {
    if (!s_showModsModal) return;

    ImGui::SetNextWindowSize(ImVec2(ui::S(520), ui::S(400)), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##mods_modal", &s_showModsModal, ImGuiWindowFlags_NoResize)) {
        ImGui::PushFont(ui::F().title);
        ImGui::TextUnformatted(i18n::T("Mod Manager & Security Approval", "Administrador de Mods y Aprobación"));
        ImGui::PopFont();
        ImGui::Separator();

        auto& host = mods::ModHost::GetInstance();
        const auto& loaded = host.GetLoadedMods();

        if (loaded.empty()) {
            ui::TextColored(ui::color::Muted, i18n::T("No mods installed in refix_mods folder.",
                                                      "No hay mods instalados en la carpeta refix_mods."));
        }

        for (const auto& [id, mod] : loaded) {
            ImGui::PushID(id.c_str());
            ImGui::PushFont(ui::F().strong);
            ImGui::Text("%s (v%s)", mod.manifest.name.c_str(), mod.manifest.version.c_str());
            ImGui::PopFont();
            ImGui::SameLine();

            if (mod.trust_level == mods::TrustLevel::Official) {
                ui::TextColored(ui::color::Accent, i18n::T("[Official]", "[Oficial]"));
            } else if (mod.is_approved) {
                ui::TextColored(ui::color::Online, i18n::T("[Approved]", "[Aprobado]"));
            } else {
                ui::TextColored(ui::color::Star, i18n::T("[Pending Approval]", "[Pendiente de Aprobación]"));
            }

            ImGui::PushFont(ui::F().caption);
            ImGui::Text(i18n::T("Author: %s | Tier: %s", "Autor: %s | Nivel: %s"),
                        mod.manifest.author.c_str(),
                        mod.manifest.execution_tier == mods::ExecutionTier::Sandboxed ? "Sandboxed (WASM)" : "Native (DLL)");

            ImGui::PopFont();

            if (!mod.is_approved) {
                if (ui::Clicked(ui::NavButton(i18n::T("Approve Mod", "Aprobar Mod"), ImVec2(ui::S(130), ui::S(26))))) {
                    host.ApproveMod(id, true);
                }
                ImGui::SameLine();
                if (ui::Clicked(ui::NavButton(i18n::T("Reject Mod", "Rechazar Mod"), ImVec2(ui::S(130), ui::S(26))))) {
                    host.ApproveMod(id, false);
                }
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (ui::Clicked(ui::NavButton(i18n::T("Close", "Cerrar"), ImVec2(ui::S(100), ui::S(30))))) {
            s_showModsModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DrawMarketGiveModal() {
    if (!s_showMarketGiveModal) return;

    ImGui::SetNextWindowSize(ImVec2(ui::S(680), ui::S(520)), ImGuiCond_Appearing);
    if (ImGui::Begin("Mercado ReFix / Give Menu CS:GO##modal", &s_showMarketGiveModal,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::PushFont(ui::F().title);
        ui::TextColored(ui::color::Accent, ui::icon::Store);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("ReFix Market & Inventory Give Menu", "Mercado ReFix y Give Menu de Inventario"));
        ImGui::PopFont();
        ImGui::Separator();

        if (ImGui::BeginTabBar("##market_tabs")) {
            // TAB 1: WEAPON CASES
            if (ImGui::BeginTabItem(i18n::T("Weapon Cases (36)", "Cajas de Armas (36)"))) {
                ImGui::BeginChild("##cases_list", ImVec2(0, ui::S(360)), true);
                for (size_t i = 0; i < sizeof(kCasesMarket) / sizeof(kCasesMarket[0]); ++i) {
                    ImGui::PushID((int)i);
                    ImGui::Text("%s", kCasesMarket[i].name);
                    ImGui::SameLine(ui::S(380));
                    if (ui::Clicked(ui::NavButton("+1##case", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 1, 4, 0);
                        App::Get().ShowInfo("Caja Añadida", std::string("Se ha añadido 1x ") + kCasesMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::NavButton("+5##case", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 5, 4, 0);
                        App::Get().ShowInfo("Cajas Añadidas", std::string("Se han añadido 5x ") + kCasesMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::PrimaryButton("+10##case", ImVec2(ui::S(65), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kCasesMarket[i].defIndex, 10, 4, 0);
                        App::Get().ShowInfo("Cajas Añadidas", std::string("Se han añadido 10x ") + kCasesMarket[i].name);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // TAB 2: KEYS
            if (ImGui::BeginTabItem(i18n::T("Case Keys", "Llaves de Cajas"))) {
                ImGui::BeginChild("##keys_list", ImVec2(0, ui::S(360)), true);
                for (size_t i = 0; i < sizeof(kKeysMarket) / sizeof(kKeysMarket[0]); ++i) {
                    ImGui::PushID((int)i + 1000);
                    ImGui::Text("%s", kKeysMarket[i].name);
                    ImGui::SameLine(ui::S(380));
                    if (ui::Clicked(ui::NavButton("+5##key", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 5, 4, 0);
                        App::Get().ShowInfo("Llaves Añadidas", std::string("Se han añadido 5x ") + kKeysMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::NavButton("+10##key", ImVec2(ui::S(60), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 10, 4, 0);
                        App::Get().ShowInfo("Llaves Añadidas", std::string("Se han añadido 10x ") + kKeysMarket[i].name);
                    }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::PrimaryButton("+20##key", ImVec2(ui::S(65), ui::S(22))))) {
                        CSGO_GrantItemToLocal(kKeysMarket[i].defIndex, 20, 4, 0);
                        App::Get().ShowInfo("Llaves Añadidas", std::string("Se han añadido 20x ") + kKeysMarket[i].name);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // TAB 3: SKINS & KNIVES
            if (ImGui::BeginTabItem(i18n::T("Skins & Knives", "Skins y Cuchillos"))) {
                ImGui::BeginChild("##skins_list", ImVec2(0, ui::S(360)), true);
                for (size_t i = 0; i < sizeof(kSkinsMarket) / sizeof(kSkinsMarket[0]); ++i) {
                    ImGui::PushID((int)i + 2000);
                    ImGui::Text("%s", kSkinsMarket[i].name);
                    ImGui::SameLine();
                    char catBuf[64];
                    snprintf(catBuf, sizeof(catBuf), "[%s]", kSkinsMarket[i].category);
                    ui::TextColored(ui::color::Accent, catBuf);
                    ImGui::SameLine(ui::S(480));
                    if (ui::Clicked(ui::PrimaryButton(i18n::T("Add to Inventory", "Añadir al Inventario"), ImVec2(ui::S(160), ui::S(24))))) {
                        CSGO_GrantItemToLocal(kSkinsMarket[i].defIndex, 1, kSkinsMarket[i].quality, kSkinsMarket[i].rarity);
                        App::Get().ShowInfo("Item Añadido", std::string("Se ha otorgado ") + kSkinsMarket[i].name);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // TAB 4: PRIME STATUS
            if (ImGui::BeginTabItem(i18n::T("Prime Status Upgrade", "Estado Prime Oficial"))) {
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

        ImGui::Dummy(ImVec2(0, ui::S(8)));
        if (ui::Clicked(ui::NavButton(i18n::T("Close", "Cerrar"), ImVec2(ui::S(120), ui::S(28))))) {
            s_showMarketGiveModal = false;
        }

        ImGui::End();
    }
}

void DrawHostMatchModal() {
    if (!s_showHostMatchModal) return;

    ImGui::SetNextWindowSize(ImVec2(ui::S(600), ui::S(480)), ImGuiCond_Appearing);
    if (ImGui::Begin("Crear Servidor / Partida LAN Personalizada##modal", &s_showHostMatchModal,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::PushFont(ui::F().title);
        ui::TextColored(ui::color::Accent, ui::icon::Game);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Custom LAN Match Host (P2P)", "Crear Partida Personalizada LAN (P2P)"));
        ImGui::PopFont();
        ImGui::Separator();

        ImGui::Text(i18n::T("Select Map:", "Seleccionar Mapa:"));
        if (ImGui::BeginCombo("##host_map", kOfficialMaps[s_selectedMapIdx])) {
            for (size_t i = 0; i < kNumOfficialMaps; ++i) {
                bool isSelected = (s_selectedMapIdx == (int)i);
                if (ImGui::Selectable(kOfficialMaps[i], isSelected)) s_selectedMapIdx = (int)i;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Text(i18n::T("Game Mode:", "Modo de Juego:"));
        if (ImGui::BeginCombo("##host_mode", kGameModes[s_selectedModeIdx])) {
            for (int i = 0; i < 5; ++i) {
                bool isSelected = (s_selectedModeIdx == i);
                if (ImGui::Selectable(kGameModes[i], isSelected)) s_selectedModeIdx = i;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::PushFont(ui::F().strong);
        ImGui::TextUnformatted(i18n::T("Custom Match Settings:", "Configuración de Partida Personalizada:"));
        ImGui::PopFont();

        ImGui::Checkbox(i18n::T("Enable Bots (5v5 with AI)", "Habilitar Bots (5v5 con IA)"), &s_cfgBots);
        ImGui::Checkbox(i18n::T("Fast MR12 Rounds (24 rounds)", "Rondas Rápidas MR12 (24 rondas)"), &s_cfgFastRounds);
        ImGui::Checkbox(i18n::T("Competitive $16,000 Start Money", "Dinero Inicial $16,000"), &s_cfgCompMoney);
        ImGui::Checkbox(i18n::T("Skip Warmup (Immediate Start)", "Sin Warmup (Inicio Inmediato)"), &s_cfgNoWarmup);
        ImGui::Checkbox(i18n::T("Buy Anywhere Anytime", "Comprar en Cualquier Lugar"), &s_cfgBuyAnywhere);

        ImGui::Dummy(ImVec2(0, ui::S(8)));
        if (ui::Clicked(ui::PrimaryButton(i18n::T("START LAN SERVER / HOST MATCH", "INICIAR SERVIDOR LAN / HOSTAR PARTIDA"), ImVec2(ui::S(400), ui::S(38))))) {
            int gType = 0, gMode = 1;
            if (s_selectedModeIdx == 0) { gType = 0; gMode = 1; }      // Competitive
            else if (s_selectedModeIdx == 1) { gType = 0; gMode = 0; } // Casual
            else if (s_selectedModeIdx == 2) { gType = 1; gMode = 2; } // Deathmatch
            else if (s_selectedModeIdx == 3) { gType = 0; gMode = 2; } // Wingman
            else if (s_selectedModeIdx == 4) { gType = 1; gMode = 0; } // Arms Race

            char cmdBuf[512];
            snprintf(cmdBuf, sizeof(cmdBuf),
                "game_type %d; game_mode %d; sv_lan 1; sv_cheats 0; mp_autoteambalance 0; "
                "%s; %s; %s; %s; %s; map %s",
                gType, gMode,
                s_cfgBots ? "bot_quota 10; bot_difficulty 2" : "bot_kick",
                s_cfgFastRounds ? "mp_maxrounds 24" : "mp_maxrounds 30",
                s_cfgCompMoney ? "mp_startmoney 16000; mp_maxmoney 16000" : "mp_startmoney 800",
                s_cfgNoWarmup ? "mp_warmup_end 1" : "mp_warmup_pausetimer 0",
                s_cfgBuyAnywhere ? "mp_buy_anywhere 1" : "mp_buy_anywhere 0",
                kOfficialMaps[s_selectedMapIdx]);

            ExecuteEngineCmd(cmdBuf);
            s_showHostMatchModal = false;

            uint64_t actLobby = UnrealSteamEmu::GetActiveLobbyID();
            if (actLobby) {
                UnrealSteamEmu::Matchmaking_SetLobbyGameServer(actLobby, 0x0100007F, 27015, UnrealSteamEmu::GetLocalSteamID());
            }
            App::Get().ShowInfo("Servidor LAN Iniciado", "Partida hosteada y transmitida en LAN.");
        }

        ImGui::Separator();
        ImGui::PushFont(ui::F().strong);
        ImGui::TextUnformatted(i18n::T("Direct Connect to LAN Peer:", "Conexión Directa a Servidor LAN:"));
        ImGui::PopFont();
        ImGui::InputText("##direct_ip", s_directConnectIp, sizeof(s_directConnectIp));
        ImGui::SameLine();
        if (ui::Clicked(ui::NavButton(i18n::T("Connect##btn", "Conectar##btn"), ImVec2(ui::S(120), ui::S(26))))) {
            char connCmd[128];
            snprintf(connCmd, sizeof(connCmd), "connect %s", s_directConnectIp);
            ExecuteEngineCmd(connCmd);
            s_showHostMatchModal = false;
        }

        ImGui::Dummy(ImVec2(0, ui::S(6)));
        if (ui::Clicked(ui::NavButton(i18n::T("Close", "Cerrar"), ImVec2(ui::S(100), ui::S(26))))) {
            s_showHostMatchModal = false;
        }

        ImGui::End();
    }
}

} // namespace refix::overlay
