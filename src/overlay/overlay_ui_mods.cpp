#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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
#include "csgo_schema.h"
#include "../lan_core/refix_lan_core.h"
#include "../steam_lobby_mapping.h"
#include <algorithm>
#include <vector>
#include <string>
#include "../source_main_thread_queue.h"

extern "C" {
void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed);
bool CSGO_IsPrimeStatus();
void CSGO_SetPrimeStatus(bool enabled);
bool CSGO_HasOperationPass(uint32_t season);
void CSGO_ActivateOperationPass(uint32_t season, uint64_t passItemId);
}

namespace refix::overlay {

void ExecuteEngineClientCommand(const char* cmd) {
    if (!cmd || !*cmd) return;
    refix::source::MainThreadQueue::ExecuteClientCmd(cmd);
}

static bool s_showCustomSettings = false;
static int s_selectedMapIdx = 0;
static int s_selectedModeIdx = 0;
static int s_cfgSlotsIdx = 0;        // 10, 16, 20, 32
static int s_cfgMaxRoundsIdx = 0;    // MR12 (24), MR15 (30), 16, 0 (Sin límite)
static int s_cfgRoundTimeIdx = 1;    // 1.75, 1.92, 2.0, 3.0
static bool s_cfgBots = true;
static bool s_cfgCompMoney = true;   // 16K start money
static bool s_cfgStatTrak = false;
static char s_marketSearch[128] = "";

static const char* kSlotsOptions[] = {
    "10 Jugadores (5v5 Clásico)", "16 Jugadores (8v8 Casual)", "20 Jugadores (10v10 Completo)", "32 Jugadores (16v16 Caos)"
};
static const int kSlotsValues[] = { 10, 16, 20, 32 };

static const char* kMaxRoundsOptions[] = {
    "MR12 (24 Rondas - Actual)", "MR15 (30 Rondas - Clásico)", "16 Rondas", "0 (Sin límite / Ilimitado)"
};
static const int kMaxRoundsValues[] = { 24, 30, 16, 0 };

static const char* kRoundTimeOptions[] = {
    "1:45 min (1.75 Oficial Comp)", "1:55 min (1.92 Oficial Casual)", "2:00 min (2.0 Estándar)", "3:00 min (3.0 Extendido)"
};
static const float kRoundTimeValues[] = { 1.75f, 1.92f, 2.0f, 3.0f };

static const char* kOfficialMaps[] = {
    "de_dust2", "de_mirage", "de_inferno", "de_nuke",
    "de_overpass", "de_vertigo", "de_ancient", "de_cache", "cs_office"
};
static const size_t kNumOfficialMaps = sizeof(kOfficialMaps) / sizeof(kOfficialMaps[0]);

static const char* kGameModes[] = {
    "Competitive (5v5 Clásico)", "Casual (10v10)", "Deathmatch", "Wingman (2v2 Compañero)", "Arms Race"
};

void OpenMarketGiveMenu() {
    App::Get().CloseOverlay();
    App::Get().OpenMarketHUD();
}

void OpenHostMatchModal() {
    App::Get().CloseOverlay();
    App::Get().OpenMatchmakingHUD();
}

void OpenMatchmakingToast() {
    App::Get().CloseOverlay();
    App::Get().OpenMatchmakingHUD();
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

static void RenderItemsTable(const std::vector<refix::csgo::MarketItem>& items, int idOffset) {
    std::string filter = s_marketSearch;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    if (ImGui::BeginTable("##market_table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, ui::S(360)))) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Acción", ImGuiTableColumnFlags_WidthFixed, ui::S(130));

        for (size_t i = 0; i < items.size(); ++i) {
            const auto& itm = items[i];
            if (!filter.empty()) {
                std::string n = itm.name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                if (n.find(filter) == std::string::npos) continue;
            }

            ImGui::TableNextRow(0, ui::S(28));
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID((int)i + idOffset);

            // Rarity color bar
            ImU32 rCol = GetRarityColor(itm.rarity);
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(cp.x, cp.y + ui::S(2)),
                ImVec2(cp.x + ui::S(4), cp.y + ui::S(24)),
                rCol, ui::S(2));

            ImGui::Dummy(ImVec2(ui::S(10), 0));
            ImGui::SameLine();
            ImGui::TextUnformatted(itm.name.c_str());
            ImGui::SameLine();
            char catBuf[64];
            snprintf(catBuf, sizeof(catBuf), "[%s]", itm.category.c_str());
            ui::TextColored(ui::color::Muted, catBuf);

            ImGui::TableSetColumnIndex(1);
            uint32_t finalQual = (s_cfgStatTrak && itm.paintKit > 0) ? 9 : itm.quality;
            const char* btnLabel = (finalQual == 9) ? "+ StatTrak" : "Añadir";
            if (ui::Clicked(ui::PrimaryButton(btnLabel, ImVec2(ui::S(120), ui::S(24))))) {
                CSGO_GrantItemToLocal(itm.defIndex, 1, finalQual, itm.rarity, itm.paintKit, itm.wear, itm.seed);
                App::Get().ShowInfo("Inventario Actualizado", std::string("Se ha otorgado ") + itm.name);
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

static void RenderCasesOrKeysTable(const std::vector<refix::csgo::MarketItem>& items, bool isCase, int idOffset) {
    std::string filter = s_marketSearch;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    if (ImGui::BeginTable("##cases_keys_table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, ui::S(360)))) {
        ImGui::TableSetupColumn("Contenedor / Llave", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Opciones de Cantidad", ImGuiTableColumnFlags_WidthFixed, ui::S(185));

        for (size_t i = 0; i < items.size(); ++i) {
            const auto& itm = items[i];
            if (!filter.empty()) {
                std::string n = itm.name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                if (n.find(filter) == std::string::npos) continue;
            }

            ImGui::TableNextRow(0, ui::S(28));
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID((int)i + idOffset);

            ImGui::Dummy(ImVec2(ui::S(4), 0));
            ImGui::SameLine();
            ImGui::TextUnformatted(itm.name.c_str());

            ImGui::TableSetColumnIndex(1);
            if (ui::Clicked(ui::SecondaryButton("+1##qty", ImVec2(ui::S(52), ui::S(24))))) {
                CSGO_GrantItemToLocal(itm.defIndex, 1, 4, 0, 0, 0.0f, 0);
                App::Get().ShowInfo("Inventario Actualizado", std::string("Se ha añadido 1x ") + itm.name);
            }
            ImGui::SameLine();
            if (ui::Clicked(ui::SecondaryButton("+5##qty", ImVec2(ui::S(52), ui::S(24))))) {
                CSGO_GrantItemToLocal(itm.defIndex, 5, 4, 0, 0, 0.0f, 0);
                App::Get().ShowInfo("Inventario Actualizado", std::string("Se han añadido 5x ") + itm.name);
            }
            ImGui::SameLine();
            if (ui::Clicked(ui::PrimaryButton("+20##qty", ImVec2(ui::S(56), ui::S(24))))) {
                CSGO_GrantItemToLocal(itm.defIndex, 20, 4, 0, 0, 0.0f, 0);
                App::Get().ShowInfo("Inventario Actualizado", std::string("Se han añadido 20x ") + itm.name);
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void DrawMarketGiveModal() {
    if (!App::Get().IsMarketHUDOpen()) return;

    refix::csgo::SchemaManager::Get().Load();

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float winW = (std::min)(ui::S(860), vp.x - ui::S(40));
    float winH = (std::min)(ui::S(590), vp.y - ui::S(40));
    ImGui::SetNextWindowPos(ImVec2((vp.x - winW) * 0.5f, (vp.y - winH) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(20), ui::S(18)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);

    bool isOpen = true;
    if (ImGui::Begin("Mercado ReFix - Gestor de Inventario Local##refix_market", &isOpen,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

        // Header Title
        ImGui::PushFont(ui::F().heading);
        ui::TextColored(ui::color::Accent, ui::icon::Store);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Community Market & Inventory Manager", "Mercado ReFix y Gestor de Inventario"));
        ImGui::PopFont();

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ui::S(36));
        if (ui::IconButton("close_market_x", ui::icon::Close, i18n::T("Close", "Cerrar"), ui::color::Muted, true, ui::S(26))) {
            App::Get().CloseMarketHUD();
        }

        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, i18n::T("Add official cases, keys, knives, and custom skins dynamically loaded from the game.",
                                                  "Añade cajas, llaves, cuchillos y skins oficiales leídos dinámicamente de esta versión del juego."));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(4)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Search Bar & StatTrak Toggle
        ImGui::SetNextItemWidth(ui::S(440));
        ImGui::InputTextWithHint("##market_search", "Buscar arma, cuchillo, caja o pegatina...", s_marketSearch, sizeof(s_marketSearch));

        ImGui::SameLine();
        ImGui::Checkbox("StatTrak (Contador de Bajas)", &s_cfgStatTrak);

        ImGui::Dummy(ImVec2(0, ui::S(4)));

        auto& schema = refix::csgo::SchemaManager::Get();

        if (ImGui::BeginTabBar("##market_tabs")) {
            // TAB 1: WEAPON CASES
            auto cases = schema.GetMarketItemsByCategory("Caja");
            char caseTabTitle[64];
            snprintf(caseTabTitle, sizeof(caseTabTitle), "%s (%zu)", i18n::T("Weapon Cases", "Cajas de Armas"), cases.size());
            if (ImGui::BeginTabItem(caseTabTitle)) {
                RenderCasesOrKeysTable(cases, true, 100);
                ImGui::EndTabItem();
            }

            // TAB 2: CASE KEYS
            auto keys = schema.GetMarketItemsByCategory("Llave");
            char keyTabTitle[64];
            snprintf(keyTabTitle, sizeof(keyTabTitle), "%s (%zu)", i18n::T("Case Keys", "Llaves de Cajas"), keys.size());
            if (ImGui::BeginTabItem(keyTabTitle)) {
                RenderCasesOrKeysTable(keys, false, 500);
                ImGui::EndTabItem();
            }

            // TAB 3: RIFLES & SNIPERS
            auto rifles = schema.GetMarketItemsByCategory("Rifle");
            char rifleTabTitle[64];
            snprintf(rifleTabTitle, sizeof(rifleTabTitle), "%s (%zu)", i18n::T("Rifles & Snipers", "Rifles & Snipers"), rifles.size());
            if (ImGui::BeginTabItem(rifleTabTitle)) {
                RenderItemsTable(rifles, 1000);
                ImGui::EndTabItem();
            }

            // TAB 4: PISTOLS
            auto pistols = schema.GetMarketItemsByCategory("Pistola");
            char pistolTabTitle[64];
            snprintf(pistolTabTitle, sizeof(pistolTabTitle), "%s (%zu)", i18n::T("Pistols", "Pistolas"), pistols.size());
            if (ImGui::BeginTabItem(pistolTabTitle)) {
                RenderItemsTable(pistols, 2000);
                ImGui::EndTabItem();
            }

            // TAB 5: KNIVES
            auto knives = schema.GetMarketItemsByCategory("Cuchillo");
            char knifeTabTitle[64];
            snprintf(knifeTabTitle, sizeof(knifeTabTitle), "%s (%zu)", i18n::T("Knives", "Cuchillos"), knives.size());
            if (ImGui::BeginTabItem(knifeTabTitle)) {
                RenderItemsTable(knives, 3000);
                ImGui::EndTabItem();
            }

            // TAB 6: GLOVES
            auto gloves = schema.GetMarketItemsByCategory("Guantes");
            char gloveTabTitle[64];
            snprintf(gloveTabTitle, sizeof(gloveTabTitle), "%s (%zu)", i18n::T("Gloves", "Guantes"), gloves.size());
            if (ImGui::BeginTabItem(gloveTabTitle)) {
                RenderItemsTable(gloves, 4000);
                ImGui::EndTabItem();
            }

            // TAB 7: AGENTS
            auto agents = schema.GetMarketItemsByCategory("Agentes");
            char agentTabTitle[64];
            snprintf(agentTabTitle, sizeof(agentTabTitle), "%s (%zu)", i18n::T("Agents", "Agentes"), agents.size());
            if (ImGui::BeginTabItem(agentTabTitle)) {
                RenderItemsTable(agents, 4500);
                ImGui::EndTabItem();
            }

            // TAB 7: STICKERS
            auto stickers = schema.GetMarketItemsByCategory("Pegatina");
            char stickerTabTitle[64];
            snprintf(stickerTabTitle, sizeof(stickerTabTitle), "%s (%zu)", i18n::T("Stickers", "Pegatinas"), stickers.size());
            if (ImGui::BeginTabItem(stickerTabTitle)) {
                RenderItemsTable(stickers, 5000);
                ImGui::EndTabItem();
            }

            // TAB 8: PRIME & OPERATIONS
            if (ImGui::BeginTabItem(i18n::T("Prime & Operations", "Prime y Operaciones"))) {
                ImGui::Dummy(ImVec2(0, ui::S(10)));
                ImGui::PushFont(ui::F().strong);
                ui::TextColored(ui::color::Online, i18n::T("CS:GO Prime Status Upgrade", "Mejora de Estado Prime Oficial"));
                ImGui::PopFont();
                ImGui::TextWrapped(i18n::T("Prime Status matches you with other Prime Status players, grants access to Prime-exclusive souvenir items, item drops, and weapon cases.",
                                         "El estado Prime te empareja con otros jugadores Prime, otorga acceso a cajas de armas exclusivas, drops semanales e insignias de servicio."));
                ImGui::Dummy(ImVec2(0, ui::S(12)));

                bool isPrime = CSGO_IsPrimeStatus();
                if (isPrime) {
                    ui::TextColored(ui::color::Online, i18n::T("Prime Status is ACTIVE", "Estado Prime ACTIVO en este perfil"));
                    ImGui::Dummy(ImVec2(0, ui::S(8)));
                    if (ui::Clicked(ui::SecondaryButton(i18n::T("Deactivate Prime Status", "Desactivar Estado Prime"), ImVec2(ui::S(260), ui::S(34))))) {
                        CSGO_SetPrimeStatus(false);
                    }
                } else {
                    if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Prime Status (Free via ReFix)", "Activar Estado Prime (Gratis vía ReFix)"), ImVec2(ui::S(320), ui::S(36))))) {
                        CSGO_SetPrimeStatus(true);
                    }
                }

                ImGui::Dummy(ImVec2(0, ui::S(16)));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, ui::S(12)));

                ImGui::PushFont(ui::F().strong);
                ui::TextColored(ui::color::Accent, i18n::T("Operation Shattered Web (Season 8)", "Operación Shattered Web (Temporada 8)"));
                ImGui::PopFont();
                ImGui::TextWrapped(i18n::T("Activate the Shattered Web Premium Pass to unlock mission progress, weekly rewards, and the Shattered Web Coin.",
                                         "Activa el Pase Premium de Shattered Web para desbloquear el progreso de misiones, recompensas y la Moneda de la Operación."));
                ImGui::Dummy(ImVec2(0, ui::S(12)));

                bool hasPass = CSGO_HasOperationPass(8);
                if (hasPass) {
                    ui::TextColored(ui::color::Online, i18n::T("Operation Shattered Web Premium Pass is ACTIVE", "Pase Premium Shattered Web ACTIVO"));
                } else {
                    if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Shattered Web Pass (Free via ReFix)", "Activar Pase Shattered Web (Gratis vía ReFix)"), ImVec2(ui::S(360), ui::S(36))))) {
                        CSGO_ActivateOperationPass(8, 0);
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Dummy(ImVec2(0, ui::S(6)));
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Close Menu", "Cerrar Menú"), ImVec2(ui::S(140), ui::S(28))))) {
            App::Get().CloseMarketHUD();
        }
    }
    if (!isOpen) {
        App::Get().CloseMarketHUD();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DrawMatchmakingToastWidget() {
    if (!App::Get().IsMatchmakingHUDOpen()) return;

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float cardW = ui::S(380);
    float margin = ui::S(20);
    float maxCardH = vp.y - margin * 2.0f;
    float desiredH = s_showCustomSettings ? ui::S(530) : ui::S(215);
    float cardH = (std::min)(maxCardH, desiredH);

    ImGui::SetNextWindowPos(ImVec2(vp.x - cardW - margin, vp.y - cardH - margin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(18), ui::S(16)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);

    bool isOpen = true;
    if (ImGui::Begin("##matchmaking_hud_widget", &isOpen,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        // Header Title
        ImGui::PushFont(ui::F().strong);
        ui::TextColored(ui::color::Accent, ui::icon::Game);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Matchmaking LAN / Local Network", "Partida LAN / Matchmaking"));
        ImGui::PopFont();

        ImGui::SameLine(cardW - ui::S(46));
        if (ui::IconButton("close_mm_toast", ui::icon::Close, i18n::T("Close", "Cerrar"), ui::color::Muted, true, ui::S(24))) {
            App::Get().CloseMatchmakingHUD();
        }

        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, i18n::T("Play with nearby friends on your local network", "Juega con amigos en tu red local"));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(4)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Real-time network telemetry: Online players & Available games
        auto remotePlayers = net::Players();
        size_t onlineCount = remotePlayers.size() + 1; // including self
        auto knownLobbies = refix::lan::ILanCore::Get().Lobby().GetAllKnownLobbies();
        size_t availableGames = knownLobbies.size();
        for (const auto& p : remotePlayers) {
            if (p.appId == 730 && (p.lobbyId != 0 || !p.connect.empty() || p.ip != 0)) {
                availableGames++;
            }
        }

        ui::TextColored(ui::color::Online, ui::icon::People);
        ImGui::SameLine();
        ImGui::Text(i18n::T("Online Players: %zu", "Jugadores en línea: %zu"), onlineCount);

        ImGui::SameLine(ui::S(210));
        ui::TextColored(availableGames > 0 ? ui::color::Accent : ui::color::Muted, ui::icon::Lan);
        ImGui::SameLine();
        ImGui::Text(i18n::T("Available Matches: %zu", "Partidas disponibles: %zu"), availableGames);

        ImGui::Dummy(ImVec2(0, ui::S(6)));

        // Option 1: Search Match
        if (ui::Clicked(ui::PrimaryButton(i18n::T("Search LAN Match", "Buscar partida"), ImVec2(cardW - ui::S(36), ui::S(36))))) {
            refix::lan::ILanCore::Get().Matchmaking().RefreshLobbyList();
            auto freshLobbies = refix::lan::ILanCore::Get().Lobby().GetAllKnownLobbies();
            bool found = false;

            // Check LAN Core lobbies first
            for (const auto& l : freshLobbies) {
                uint64_t sId = refix::steam::SteamLobbyRegistry::Get().GetSteamLobbyId(l.lobbyId);
                if (sId == 0) sId = refix::steam::SteamLobbyRegistry::Get().EnsureSteamLobbyID(l.lobbyId);
                if (sId != 0) {
                    net::RemotePlayer rp;
                    rp.appId = 730;
                    rp.steamId = l.ownerPeerId.low;
                    rp.lobbyId = sId;
                    App::Get().Join(rp);
                    App::Get().ShowInfo("Matchmaking LAN", "Partida LAN encontrada en red local. Uniéndose...");
                    found = true;
                    App::Get().CloseMatchmakingHUD();
                    break;
                }
            }

            // Fallback to presence remote players
            if (!found) {
                for (const auto& p : remotePlayers) {
                    if (p.appId == 730) {
                        if (p.lobbyId != 0 || !p.connect.empty() || p.ip != 0) {
                            App::Get().Join(p);
                            App::Get().ShowInfo("Matchmaking LAN", "Servidor o sala LAN encontrada. Conectando...");
                            found = true;
                            App::Get().CloseMatchmakingHUD();
                            break;
                        }
                    }
                }
            }

            if (!found) {
                App::Get().ShowInfo("Matchmaking LAN", "No se encontraron partidas LAN disponibles en tu red local.");
                App::Get().CloseMatchmakingHUD();
            }
        }

        ImGui::Dummy(ImVec2(0, ui::S(4)));

        // Option 2: Host Match + adjacent Settings button
        float hostBtnWidth = cardW - ui::S(36) - ui::S(75);
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Host Match", "Hostear partida"), ImVec2(hostBtnWidth, ui::S(34))))) {
            UnrealSteamEmu::Matchmaking_CreateLobby(2 /* k_ELobbyTypePublic */, 10);
            std::string cmd = "maxplayers 10; game_type 0; game_mode 1; sv_lan 1; bot_quota 10; map de_dust2";
            ExecuteEngineClientCommand(cmd.c_str());
            App::Get().ShowInfo("Servidor LAN", "Iniciando servidor de partida local (de_dust2)...");
            App::Get().CloseMatchmakingHUD();
        }

        ImGui::SameLine();
        const char* cfgLabel = s_showCustomSettings ? "[-] Config" : "[+] Config";
        if (ui::Clicked(ui::SecondaryButton(cfgLabel, ImVec2(ui::S(68), ui::S(34))))) {
            s_showCustomSettings = !s_showCustomSettings;
        }

        // Custom Match Settings Panel (expanded when clicking [Config])
        if (s_showCustomSettings) {
            ImGui::Dummy(ImVec2(0, ui::S(4)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, ui::S(4)));

            float childH = cardH - ui::S(205);
            if (childH < ui::S(80)) childH = ui::S(80);
            ImGui::BeginChild("##custom_cfg_scroll", ImVec2(0, childH), false, ImGuiWindowFlags_None);

            ImGui::PushFont(ui::F().strong);
            ui::TextColored(ui::color::Accent, i18n::T("Custom Match Settings", "Ajustes de partida personalizada:"));
            ImGui::PopFont();

            // Map selection
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

            // Game Mode selection
            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Game Mode:", "Modo de Juego:"));
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

            // Slots selection
            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Max Players (Slots):", "Slots de Jugadores:"));
            ImGui::PopFont();
            ImGui::SetNextItemWidth(cardW - ui::S(36));
            if (ImGui::BeginCombo("##toast_slots_combo", kSlotsOptions[s_cfgSlotsIdx])) {
                for (int i = 0; i < 4; ++i) {
                    bool sel = (s_cfgSlotsIdx == i);
                    if (ImGui::Selectable(kSlotsOptions[i], sel)) s_cfgSlotsIdx = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Max Rounds selection
            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Max Rounds:", "Rondas Máximas:"));
            ImGui::PopFont();
            ImGui::SetNextItemWidth(cardW - ui::S(36));
            if (ImGui::BeginCombo("##toast_rounds_combo", kMaxRoundsOptions[s_cfgMaxRoundsIdx])) {
                for (int i = 0; i < 4; ++i) {
                    bool sel = (s_cfgMaxRoundsIdx == i);
                    if (ImGui::Selectable(kMaxRoundsOptions[i], sel)) s_cfgMaxRoundsIdx = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Round Time selection
            ImGui::PushFont(ui::F().caption);
            ImGui::TextUnformatted(i18n::T("Round Time:", "Tiempo de Ronda:"));
            ImGui::PopFont();
            ImGui::SetNextItemWidth(cardW - ui::S(36));
            if (ImGui::BeginCombo("##toast_rtime_combo", kRoundTimeOptions[s_cfgRoundTimeIdx])) {
                for (int i = 0; i < 4; ++i) {
                    bool sel = (s_cfgRoundTimeIdx == i);
                    if (ImGui::Selectable(kRoundTimeOptions[i], sel)) s_cfgRoundTimeIdx = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Dummy(ImVec2(0, ui::S(2)));
            ImGui::Checkbox(i18n::T("Bots (5v5 AI)", "Habilitar Bots (5v5 IA)"), &s_cfgBots);
            ImGui::SameLine();
            ImGui::Checkbox(i18n::T("Start $16K", "Dinero $16,000"), &s_cfgCompMoney);

            ImGui::Dummy(ImVec2(0, ui::S(4)));
            if (ui::Clicked(ui::PrimaryButton(i18n::T("Launch Custom Server", "Crear Servidor Personalizado"), ImVec2(cardW - ui::S(36), ui::S(34))))) {
                int slots = kSlotsValues[s_cfgSlotsIdx];
                int maxRounds = kMaxRoundsValues[s_cfgMaxRoundsIdx];
                float rTime = kRoundTimeValues[s_cfgRoundTimeIdx];

                UnrealSteamEmu::Matchmaking_CreateLobby(2 /* k_ELobbyTypePublic */, slots);

                char cmd[384];
                int gType = 0, gMode = 1;
                if (s_selectedModeIdx == 1) { gType = 0; gMode = 0; } // Casual
                else if (s_selectedModeIdx == 2) { gType = 1; gMode = 2; } // Deathmatch
                else if (s_selectedModeIdx == 3) { gType = 0; gMode = 2; } // Wingman
                else if (s_selectedModeIdx == 4) { gType = 1; gMode = 0; } // Arms race

                char rTimeBuf[16];
                snprintf(rTimeBuf, sizeof(rTimeBuf), "%.2f", rTime);

                snprintf(cmd, sizeof(cmd),
                         "maxplayers %d; game_type %d; game_mode %d; sv_lan 1; mp_maxrounds %d; mp_roundtime %s; mp_roundtime_defuse %s; mp_roundtime_hostage %s; %s; %s; map %s",
                         slots, gType, gMode, maxRounds, rTimeBuf, rTimeBuf, rTimeBuf,
                         s_cfgBots ? "bot_quota 10; bot_difficulty 2" : "bot_kick",
                         s_cfgCompMoney ? "mp_startmoney 16000; mp_maxmoney 16000" : "mp_startmoney 800",
                         kOfficialMaps[s_selectedMapIdx]);

                ExecuteEngineClientCommand(cmd);
                App::Get().ShowInfo("Servidor Personalizado", std::string("Iniciando ") + kOfficialMaps[s_selectedMapIdx]);
                App::Get().CloseMatchmakingHUD();
            }
            ImGui::EndChild();
        }
    }
    if (!isOpen) {
        App::Get().CloseMatchmakingHUD();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DrawModsCard(ImVec2 pos, float width, float height) {
    ui::BeginCard("##modscard", pos, ImVec2(width, height), ui::S(12));

    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Accent, ui::icon::Game);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("CS:GO LAN & Inventory", "CS:GO LAN e Inventario"));
    ImGui::PopFont();

    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Muted, i18n::T("Community market, cases, unboxing and matchmaking.", "Mercado, apertura de cajas y matchmaking LAN."));
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(6)));

    float btnW = (width - ui::S(28)) * 0.5f;
    if (ui::Clicked(ui::PrimaryButton(i18n::T("Market & Items", "Mercado / Skins"), ImVec2(btnW, ui::S(28))))) {
        OpenMarketGiveMenu();
    }
    ImGui::SameLine();
    if (ui::Clicked(ui::SecondaryButton(i18n::T("Matchmaking", "Matchmaking LAN"), ImVec2(btnW, ui::S(28))))) {
        OpenMatchmakingToast();
    }

    ui::EndCard();
}

void DrawModsApprovalModal() {}

} // namespace refix::overlay
