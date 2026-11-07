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

// Exports already declared in steam_gc_emu.h

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

extern "C" {
__declspec(dllexport) void CSGO_OpenMarketHUD() {
    App::Get().OpenMarketHUD();
}
__declspec(dllexport) void CSGO_CloseMarketHUD() {
    App::Get().CloseMarketHUD();
}
__declspec(dllexport) void CSGO_OpenOverlay() {
    App::Get().Post([] { App::Get().OpenOverlay(); });
}
__declspec(dllexport) void CSGO_CloseOverlay() {
    App::Get().Post([] { App::Get().CloseOverlay(); });
}
__declspec(dllexport) void CSGO_ToggleOverlay() {
    App::Get().Post([] {
        if (App::Get().IsOverlayOpen()) App::Get().CloseOverlay();
        else App::Get().OpenOverlay();
    });
}
}

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

// State for Market Give Menu
static std::string s_selectedCategory = "Todos";
static int s_selectedItemIdx = 0;
static float s_detailWear = 0.001f;
static int s_detailSeed = 420;
static bool s_detailStatTrak = false;
static char s_detailCustomName[64] = "";
static int s_detailQuantity = 1;
static std::string s_grantStatusMsg = "";
static DWORD s_grantStatusExpiry = 0;

void DrawMarketGiveModal() {
    if (!App::Get().IsMarketHUDOpen()) return;

    auto& schema = refix::csgo::SchemaManager::Get();
    schema.Load();

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float winW = (std::min)(ui::S(1050), vp.x - ui::S(30));
    float winH = (std::min)(ui::S(680), vp.y - ui::S(20));
    ImGui::SetNextWindowPos(ImVec2((vp.x - winW) * 0.5f, (vp.y - winH) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(18), ui::S(16)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);

    bool isOpen = true;
    if (ImGui::Begin("Mercado ReFix - Gestor de Inventario Local##refix_market", &isOpen,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

        // --- Top Bar: Title & Search ---
        ImGui::PushFont(ui::F().heading);
        ui::TextColored(ui::color::Accent, ui::icon::Store);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Community Market & Item Manager", "Mercado ReFix y Gestor de Inventario"));
        ImGui::PopFont();

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ui::S(32));
        if (ui::IconButton("close_market_x", ui::icon::Close, i18n::T("Close", "Cerrar"), ui::color::Muted, true, ui::S(24))) {
            App::Get().CloseMarketHUD();
        }

        ImGui::PushFont(ui::F().caption);
        char headerSub[128];
        snprintf(headerSub, sizeof(headerSub), "%s (%zu %s)",
                 i18n::T("Official catalogue extracted from game schema", "Catálogo oficial cargado dinámicamente del esquema del juego"),
                 schema.GetTotalGrantableCount(),
                 i18n::T("items available", "ítems disponibles"));
        ui::TextColored(ui::color::Muted, headerSub);
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(6)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(6)));

        // Search Bar with instant filter
        ImGui::PushFont(ui::F().caption);
        ui::TextColored(ui::color::Muted, ui::icon::Search);
        ImGui::SameLine();
        ImGui::PopFont();
        ImGui::SetNextItemWidth(ui::S(380));
        ImGui::InputTextWithHint("##market_search", i18n::T("Search weapon, skin, knife, case, agent...", "Buscar arma, skin, cuchillo, caja, agente..."), s_marketSearch, sizeof(s_marketSearch));
        if (s_marketSearch[0] != '\0') {
            ImGui::SameLine();
            if (ui::Clicked(ui::SecondaryButton(i18n::T("Clear", "Limpiar"), ImVec2(ui::S(65), ui::S(24))))) {
                s_marketSearch[0] = '\0';
            }
        }

        ImGui::Dummy(ImVec2(0, ui::S(6)));

        // --- 3-Column Layout: Sidebar (left) | Grid (center) | Details (right) ---
        float sideW = ui::S(185);
        float detailW = ui::S(270);
        float availW = ImGui::GetContentRegionAvail().x;
        float availH = ImGui::GetContentRegionAvail().y - ui::S(36);
        float gridW = availW - sideW - detailW - ui::S(24);

        // 1. LEFT COLUMN: CATEGORIES SIDEBAR
        ImGui::BeginChild("##cat_sidebar", ImVec2(sideW, availH), true);
        {
            ImGui::PushFont(ui::F().caption);
            ui::TextColored(ui::color::Faint, i18n::T("CATEGORIES", "CATEGORÍAS"));
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, ui::S(6)));

            const auto& categories = schema.GetCategories();
            for (size_t c = 0; c < categories.size(); c++) {
                const auto& cat = categories[c];
                bool isSelected = (s_selectedCategory == cat.name);

                char btnText[96];
                snprintf(btnText, sizeof(btnText), "%s (%zu)", cat.name.c_str(), cat.count);

                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ui::color::RowHover);
                    ImGui::PushStyleColor(ImGuiCol_Text, ui::color::Accent);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ui::color::Text);
                }

                if (ImGui::Button(btnText, ImVec2(sideW - ui::S(16), ui::S(28)))) {
                    s_selectedCategory = cat.name;
                    s_selectedItemIdx = 0;
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::Dummy(ImVec2(0, ui::S(8)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, ui::S(6)));

            bool isPrimeTab = (s_selectedCategory == "Prime & Pases");
            if (isPrimeTab) {
                ImGui::PushStyleColor(ImGuiCol_Button, ui::color::RowHover);
                ImGui::PushStyleColor(ImGuiCol_Text, ui::color::Accent);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, ui::color::Online);
            }
            if (ImGui::Button(i18n::T("★ Prime & Operation Passes", "★ Prime y Operaciones"), ImVec2(sideW - ui::S(16), ui::S(30)))) {
                s_selectedCategory = "Prime & Pases";
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // 2. CENTER COLUMN: VIRTUALIZED ITEM GRID (or Prime & Passes view)
        if (s_selectedCategory == "Prime & Pases") {
            ImGui::BeginChild("##prime_center", ImVec2(gridW + detailW + ui::S(12), availH), true);
            {
                ImGui::Dummy(ImVec2(0, ui::S(10)));
                ImGui::PushFont(ui::F().heading);
                ui::TextColored(ui::color::Online, i18n::T("CS:GO Prime Status Upgrade", "Mejora de Estado Prime Oficial"));
                ImGui::PopFont();
                ImGui::TextWrapped(i18n::T("Prime Status matches you with other Prime Status players, grants access to Prime-exclusive souvenir items, item drops, and weapon cases.",
                                         "El estado Prime te empareja con otros jugadores Prime, otorga acceso a cajas de armas exclusivas, drops semanales e insignias de servicio oficiales (CSOEconGameAccountClient SO tipo 7)."));
                ImGui::Dummy(ImVec2(0, ui::S(10)));

                bool isPrime = CSGO_IsPrimeStatus();
                if (isPrime) {
                    ui::TextColored(ui::color::Online, i18n::T("Prime Status is ACTIVE", "Estado Prime ACTIVO en este perfil"));
                    ImGui::Dummy(ImVec2(0, ui::S(6)));
                    if (ui::Clicked(ui::SecondaryButton(i18n::T("Deactivate Prime Status", "Desactivar Estado Prime"), ImVec2(ui::S(260), ui::S(34))))) {
                        CSGO_SetPrimeStatus(false);
                    }
                } else {
                    if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Prime Status (Free via ReFix)", "Activar Estado Prime (Gratis vía ReFix)"), ImVec2(ui::S(320), ui::S(36))))) {
                        CSGO_SetPrimeStatus(true);
                    }
                }

                ImGui::Dummy(ImVec2(0, ui::S(18)));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, ui::S(14)));

                ImGui::PushFont(ui::F().heading);
                ui::TextColored(ui::color::Accent, i18n::T("Operation Shattered Web (Season 8)", "Operación Shattered Web (Temporada 8)"));
                ImGui::PopFont();
                ImGui::TextWrapped(i18n::T("Activate the Shattered Web Premium Pass to unlock mission progress, weekly rewards, and the Shattered Web Coin.",
                                         "Activa el Pase Premium de Shattered Web para desbloquear el progreso de misiones, recompensas de estrellas y la Moneda de la Operación en tu inventario."));
                ImGui::Dummy(ImVec2(0, ui::S(10)));

                bool hasPass = CSGO_HasOperationPass(8);
                if (hasPass) {
                    ui::TextColored(ui::color::Online, i18n::T("Operation Shattered Web Premium Pass is ACTIVE", "Pase Premium Shattered Web ACTIVO"));
                } else {
                    if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Shattered Web Pass (Free via ReFix)", "Activar Pase Shattered Web (Gratis vía ReFix)"), ImVec2(ui::S(360), ui::S(36))))) {
                        CSGO_ActivateOperationPass(8, 0);
                    }
                }
            }
            ImGui::EndChild();
        } else {
            // Get items for current category and apply search query
            std::vector<refix::csgo::MarketItem> allCatItems = schema.GetMarketItemsByCategory(s_selectedCategory);
            std::vector<refix::csgo::MarketItem> filtered;
            filtered.reserve(allCatItems.size());

            std::string q = s_marketSearch;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);

            for (const auto& itm : allCatItems) {
                if (!q.empty()) {
                    std::string n = itm.name;
                    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                    if (n.find(q) == std::string::npos) {
                        std::string wn = itm.weaponName;
                        std::transform(wn.begin(), wn.end(), wn.begin(), ::tolower);
                        if (wn.find(q) == std::string::npos) {
                            std::string sn = itm.skinName;
                            std::transform(sn.begin(), sn.end(), sn.begin(), ::tolower);
                            if (sn.find(q) == std::string::npos) continue;
                        }
                    }
                }
                filtered.push_back(itm);
            }

            if (s_selectedItemIdx >= (int)filtered.size()) {
                s_selectedItemIdx = 0;
            }

            // Grid Child Window with Clipper
            ImGui::BeginChild("##item_grid", ImVec2(gridW, availH), true);
            {
                float cardW = ui::S(138);
                float cardH = ui::S(115);
                float spaceX = ui::S(8);
                float spaceY = ui::S(8);

                float gridAvailW = ImGui::GetContentRegionAvail().x;
                int cols = (std::max)(1, (int)((gridAvailW + spaceX) / (cardW + spaceX)));
                int numRows = ((int)filtered.size() + cols - 1) / cols;

                ImGuiListClipper clipper;
                clipper.Begin(numRows, cardH + spaceY);

                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        for (int col = 0; col < cols; col++) {
                            int idx = row * cols + col;
                            if (idx >= (int)filtered.size()) break;
                            if (col > 0) ImGui::SameLine(0, spaceX);

                            const auto& item = filtered[idx];
                            bool isSelected = (s_selectedItemIdx == idx);

                            ImGui::PushID(idx);
                            ImVec2 cp = ImGui::GetCursorScreenPos();
                            ImDrawList* dl = ImGui::GetWindowDrawList();

                            ImU32 rCol = GetRarityColor(item.rarity);
                            ImU32 bgCol = isSelected ? ui::color::RowHover : ui::color::Panel;

                            // Card Background & Border
                            dl->AddRectFilled(cp, ImVec2(cp.x + cardW, cp.y + cardH), bgCol, ui::S(6));
                            dl->AddRect(cp, ImVec2(cp.x + cardW, cp.y + cardH),
                                        isSelected ? ui::color::Accent : ui::color::Border, ui::S(6), 0, isSelected ? 2.0f : 1.0f);

                            // Top Rarity Stripe
                            dl->AddRectFilled(ImVec2(cp.x + ui::S(4), cp.y + ui::S(2)),
                                              ImVec2(cp.x + cardW - ui::S(4), cp.y + ui::S(6)), rCol, ui::S(2));

                            // Placeholder Icon Glow Circle
                            ImVec2 iconCenter(cp.x + cardW * 0.5f, cp.y + ui::S(38));
                            dl->AddCircleFilled(iconCenter, ui::S(20), (rCol & 0x00FFFFFF) | 0x22000000);
                            dl->AddCircle(iconCenter, ui::S(20), (rCol & 0x00FFFFFF) | 0x55000000, 0, 1.2f);

                            // Category Tag Initial at Center
                            std::string tag = item.category.substr(0, (std::min)((size_t)3, item.category.size()));
                            ImVec2 tagSz = ImGui::CalcTextSize(tag.c_str());
                            dl->AddText(ImVec2(iconCenter.x - tagSz.x * 0.5f, iconCenter.y - tagSz.y * 0.5f), ui::color::Muted, tag.c_str());

                            // Item Name Lines
                            ImVec2 textPos(cp.x + ui::S(6), cp.y + ui::S(66));
                            std::string line1 = item.name;
                            std::string line2 = "";
                            size_t pipe = line1.find(" | ");
                            if (pipe != std::string::npos) {
                                line2 = line1.substr(pipe + 3);
                                line1 = line1.substr(0, pipe);
                            }

                            ImGui::PushFont(ui::F().strong);
                            std::string el1 = ui::Ellipsize(line1, cardW - ui::S(12));
                            dl->AddText(textPos, ui::color::Text, el1.c_str());
                            ImGui::PopFont();

                            if (!line2.empty()) {
                                ImGui::PushFont(ui::F().caption);
                                std::string el2 = ui::Ellipsize(line2, cardW - ui::S(12));
                                dl->AddText(ImVec2(textPos.x, textPos.y + ui::S(16)), rCol, el2.c_str());
                                ImGui::PopFont();
                            }

                            // Invisible Button for clicking the card
                            if (ImGui::InvisibleButton("##card_btn", ImVec2(cardW, cardH))) {
                                s_selectedItemIdx = idx;
                                s_detailWear = item.wear;
                                s_detailSeed = (item.seed > 0) ? item.seed : 420;
                                s_detailStatTrak = false;
                                s_detailCustomName[0] = '\0';
                                s_detailQuantity = 1;
                                s_grantStatusMsg = "";
                            }
                            ImGui::PopID();
                        }
                    }
                }
                clipper.End();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // 3. RIGHT COLUMN: DETAIL & ATTRIBUTE PANE
            ImGui::BeginChild("##item_details", ImVec2(detailW, availH), true);
            {
                if (!filtered.empty() && s_selectedItemIdx >= 0 && s_selectedItemIdx < (int)filtered.size()) {
                    const auto& selItem = filtered[s_selectedItemIdx];

                    ImGui::PushFont(ui::F().caption);
                    ui::TextColored(ui::color::Faint, i18n::T("ITEM DETAILS", "DETALLES DEL ÍTEM"));
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0, ui::S(6)));

                    ImGui::PushFont(ui::F().strong);
                    std::string titleEl = ui::Ellipsize(selItem.name, detailW - ui::S(24));
                    ui::TextColored(ui::color::Text, titleEl.c_str());
                    ImGui::PopFont();

                    char catRarBuf[128];
                    snprintf(catRarBuf, sizeof(catRarBuf), "[%s] DefIndex: %u", selItem.category.c_str(), selItem.defIndex);
                    ui::TextColored(ui::color::Muted, catRarBuf);

                    ImGui::Dummy(ImVec2(0, ui::S(6)));
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0, ui::S(6)));

                    // 1. Quantity Selection & Primary Grant Action (Always at top of pane)
                    ImGui::PushFont(ui::F().caption);
                    ui::TextColored(ui::color::Text, i18n::T("Quantity", "Cantidad"));
                    ImGui::PopFont();

                    ImGui::SetNextItemWidth(ui::S(70));
                    ImGui::InputInt("##qty_input", &s_detailQuantity);
                    if (s_detailQuantity < 1) s_detailQuantity = 1;
                    if (s_detailQuantity > 100) s_detailQuantity = 100;

                    ImGui::SameLine();
                    if (ui::Clicked(ui::SecondaryButton("+1", ImVec2(ui::S(38), ui::S(24))))) { s_detailQuantity = 1; }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::SecondaryButton("+5", ImVec2(ui::S(38), ui::S(24))))) { s_detailQuantity = 5; }
                    ImGui::SameLine();
                    if (ui::Clicked(ui::SecondaryButton("+20", ImVec2(ui::S(42), ui::S(24))))) { s_detailQuantity = 20; }

                    ImGui::Dummy(ImVec2(0, ui::S(6)));

                    // Primary Button: Añadir al Inventario
                    char grantBtnLabel[96];
                    snprintf(grantBtnLabel, sizeof(grantBtnLabel), "%s (%d)",
                             i18n::T("Add to Inventory", "Añadir al Inventario"), s_detailQuantity);

                    if (ui::Clicked(ui::PrimaryButton(grantBtnLabel, ImVec2(detailW - ui::S(28), ui::S(34))))) {
                        uint32_t finalQual = (s_detailStatTrak && selItem.paintKit > 0) ? 9 : selItem.quality;
                        const char* pCName = (s_detailCustomName[0] != '\0') ? s_detailCustomName : nullptr;
                        CSGO_GrantItemToLocal(selItem.defIndex, s_detailQuantity, finalQual, selItem.rarity,
                                              selItem.paintKit, s_detailWear, (uint32_t)s_detailSeed, pCName);

                        char succMsg[128];
                        snprintf(succMsg, sizeof(succMsg), "%s: %dx %s",
                                 i18n::T("Granted", "¡Añadido con éxito!"), s_detailQuantity, selItem.name.c_str());
                        s_grantStatusMsg = succMsg;
                        s_grantStatusExpiry = GetTickCount() + 3500;
                    }

                    // In-pane feedback message
                    if (GetTickCount() < s_grantStatusExpiry && !s_grantStatusMsg.empty()) {
                        ImGui::Dummy(ImVec2(0, ui::S(6)));
                        ImGui::PushFont(ui::F().caption);
                        ui::TextColored(ui::color::Online, s_grantStatusMsg.c_str());
                        ImGui::PopFont();
                    }

                    ImGui::Dummy(ImVec2(0, ui::S(6)));
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0, ui::S(6)));

                    // 2. Custom Attributes (Wear, Seed, StatTrak, Name tag)
                    if (selItem.paintKit > 0) {
                        ImGui::PushFont(ui::F().caption);
                        ui::TextColored(ui::color::Faint, i18n::T("SKIN ATTRIBUTES", "ATRIBUTOS DE LA SKIN"));
                        ImGui::PopFont();
                        ImGui::Dummy(ImVec2(0, ui::S(2)));

                        // Wear Slider
                        float minW = selItem.minWear;
                        float maxW = selItem.maxWear;
                        if (minW >= maxW) { minW = 0.0f; maxW = 1.0f; }

                        ImGui::PushFont(ui::F().caption);
                        const char* condStr = "Recién fabricado (FN)";
                        if (s_detailWear >= 0.45f) condStr = "Deplorable (BS)";
                        else if (s_detailWear >= 0.38f) condStr = "Bastante desgastado (WW)";
                        else if (s_detailWear >= 0.15f) condStr = "Algo desgastado (FT)";
                        else if (s_detailWear >= 0.07f) condStr = "Casi nuevo (MW)";
                        char wearLabel[96];
                        snprintf(wearLabel, sizeof(wearLabel), "%s: %.4f (%s)",
                                 i18n::T("Float", "Desgaste"), s_detailWear, condStr);
                        ui::TextColored(ui::color::Accent, wearLabel);
                        ImGui::PopFont();

                        ImGui::SetNextItemWidth(detailW - ui::S(28));
                        ImGui::SliderFloat("##wear_slider", &s_detailWear, minW, maxW, "%.4f");

                        ImGui::Dummy(ImVec2(0, ui::S(6)));

                        // Seed / Pattern Input
                        ImGui::PushFont(ui::F().caption);
                        ui::TextColored(ui::color::Text, i18n::T("Pattern / Seed", "Semilla / Patrón"));
                        ImGui::PopFont();

                        ImGui::SetNextItemWidth(detailW - ui::S(110));
                        ImGui::InputInt("##seed_input", &s_detailSeed);
                        if (s_detailSeed < 1) s_detailSeed = 1;
                        if (s_detailSeed > 1000) s_detailSeed = 1000;

                        ImGui::SameLine();
                        if (ui::Clicked(ui::SecondaryButton(i18n::T("Roll", "Azar"), ImVec2(ui::S(65), ui::S(24))))) {
                            s_detailSeed = rand() % 999 + 1;
                        }

                        ImGui::Dummy(ImVec2(0, ui::S(6)));

                        // StatTrak Toggle
                        ImGui::Checkbox(i18n::T("StatTrak™ (Kill Counter)", "StatTrak™ (Contador de Bajas)"), &s_detailStatTrak);

                        ImGui::Dummy(ImVec2(0, ui::S(6)));

                        // Custom Name Tag
                        ImGui::PushFont(ui::F().caption);
                        ui::TextColored(ui::color::Text, i18n::T("Custom Name Tag", "Etiqueta Personalizada"));
                        ImGui::PopFont();
                        ImGui::SetNextItemWidth(detailW - ui::S(28));
                        ImGui::InputTextWithHint("##cname_input", i18n::T("Optional name...", "Nombre opcional..."), s_detailCustomName, sizeof(s_detailCustomName));
                    }
                } else {
                    ImGui::Dummy(ImVec2(0, ui::S(20)));
                    ui::TextColored(ui::color::Muted, i18n::T("Select an item from the catalogue to configure attributes and add to inventory.",
                                                             "Selecciona un ítem del catálogo para configurar sus atributos y añadirlo al inventario."));
                }
            }
            ImGui::EndChild();
        }

        // --- Bottom Bar: Close Button ---
        ImGui::Dummy(ImVec2(0, ui::S(6)));
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Close Menu", "Cerrar Menú"), ImVec2(ui::S(130), ui::S(28))))) {
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

        ImGui::Dummy(ImVec2(0, ui::S(6)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, ui::S(6)));

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
                    uint64_t ownerSteamId = 0;
                    auto pOwner = refix::lan::ILanCore::Get().Peers().FindByPeerId(l.ownerPeerId);
                    if (pOwner && pOwner->externalId.numericId != 0) {
                        ownerSteamId = pOwner->externalId.numericId;
                    } else {
                        auto oIt = l.attributes.find("__steam_owner");
                        if (oIt != l.attributes.end() && !oIt->second.asString.empty()) {
                            try { ownerSteamId = std::stoull(oIt->second.asString); } catch (...) {}
                        }
                    }
                    if (ownerSteamId == 0) continue; // P1.a: forbid peerId.low as SteamID fallback
                    net::RemotePlayer rp;
                    rp.appId = 730;
                    rp.steamId = ownerSteamId;
                    rp.lobbyId = sId;
                    App::Get().Join(rp);
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

        ImGui::Dummy(ImVec2(0, ui::S(6)));

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
            ImGui::Dummy(ImVec2(0, ui::S(6)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, ui::S(6)));

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

            ImGui::Dummy(ImVec2(0, ui::S(6)));
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

float GetModsCardHeight(float width) {
    const float kCardPad = 12.0f; // unscaled points: BeginCard scales with S(kCardPad)
    const float padPx = ui::S(kCardPad);
    const float availW = width - padPx * 2.0f;

    const float titleH = ui::F().strong ? ui::F().strong->FontSize : ui::S(16.0f);
    const float captionH = ui::F().caption ? ui::F().caption->FontSize : ui::S(14.0f);
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const float gapY = ui::S(6.0f);
    const float btnH = ui::S(36.0f);

    const char* label0 = i18n::T("Market & Skins", "Mercado & Skins");
    const char* label1 = i18n::T("Matchmaking LAN", "Matchmaking LAN");

    float minW0 = ImGui::CalcTextSize(label0).x + ui::S(24.0f);
    float minW1 = ImGui::CalcTextSize(label1).x + ui::S(24.0f);
    float spaceX = ImGui::GetStyle().ItemSpacing.x;

    bool fitsOneRow = (minW0 + minW1 + spaceX <= availW);
    float btnAreaH = fitsOneRow ? btnH : (btnH * 2.0f + spacingY);

    return padPx * 2.0f + titleH + spacingY + captionH + gapY + btnAreaH + ui::S(10.0f);
}

void DrawModsCard(ImVec2 pos, float width, float height) {
    if (height <= 0.0f) {
        height = GetModsCardHeight(width);
    }
    const float kCardPad = 12.0f; // unscaled points: BeginCard will call S(kCardPad) internally
    ui::BeginCard("##modscard", pos, ImVec2(width, height), kCardPad);

    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Accent, ui::icon::Game);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Game Actions", "Acciones del juego"));
    ImGui::PopFont();

    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Muted, i18n::T("Quick actions and local LAN features", "Acciones rápidas y funciones de red LAN"));
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(6)));

    const float padPx = ui::S(kCardPad);
    const float availW = width - padPx * 2.0f;
    const float btnH = ui::S(36.0f);

    const char* label0 = i18n::T("Market & Skins", "Mercado & Skins");
    const char* label1 = i18n::T("Matchmaking LAN", "Matchmaking LAN");

    float minW0 = ImGui::CalcTextSize(label0).x + ui::S(24.0f);
    float minW1 = ImGui::CalcTextSize(label1).x + ui::S(24.0f);
    float spaceX = ImGui::GetStyle().ItemSpacing.x;

    bool fitsOneRow = (minW0 + minW1 + spaceX <= availW);

    if (fitsOneRow) {
        float equalW = (availW - spaceX) * 0.5f;
        float w0 = (std::max)(equalW, minW0);
        float w1 = availW - w0 - spaceX;
        if (w1 < minW1) {
            w0 = minW0;
            w1 = availW - w0 - spaceX;
        }

        if (ui::Clicked(ui::PrimaryButton(label0, ImVec2(w0, btnH)))) {
            OpenMarketGiveMenu();
        }
        ImGui::SameLine();
        if (ui::Clicked(ui::SecondaryButton(label1, ImVec2(w1, btnH)))) {
            OpenMatchmakingToast();
        }
    } else {
        // Auto-wrap to next line if width is narrow
        if (ui::Clicked(ui::PrimaryButton(label0, ImVec2(availW, btnH)))) {
            OpenMarketGiveMenu();
        }
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        if (ui::Clicked(ui::SecondaryButton(label1, ImVec2(availW, btnH)))) {
            OpenMatchmakingToast();
        }
    }

    ui::EndCard();
}

void DrawModsApprovalModal() {}

} // namespace refix::overlay
