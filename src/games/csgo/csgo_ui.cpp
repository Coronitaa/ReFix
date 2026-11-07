#include "csgo_handlers.h"
#include "../../imgui/imgui.h"
#include "../../csgo_schema.h"

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

namespace refix::csgo {

static refix::IRefixServices* s_uiServices = nullptr;
static std::string s_selectedCat = "Todos";
static int s_selectedIdx = 0;
static float s_wear = 0.001f;
static int s_seed = 420;
static bool s_statTrak = false;
static char s_search[64] = "";
static std::string s_statusMsg = "";
static DWORD s_statusExp = 0;

void RegisterCSGOUi(refix::IRefixServices* services) {
    s_uiServices = services;
    if (!services || !services->overlay) return;

    // 1. Register Card in Main Overlay
    services->overlay->RegisterCard("csgo_actions", "CS:GO LAN Actions", "Acciones rápidas y funciones de red LAN",
        [](float availW, float height) {
            float spaceX = ImGui::GetStyle().ItemSpacing.x;
            float btnH = 36.0f;
            float btnW = (availW - spaceX) * 0.5f;

            if (ImGui::Button("Mercado & Skins", ImVec2(btnW, btnH))) {
                s_uiServices->overlay->OpenModal("csgo_give_menu");
            }
            ImGui::SameLine();
            if (ImGui::Button("Crear Servidor LAN", ImVec2(btnW, btnH))) {
                s_uiServices->engine->EnqueueCommand("map de_dust2");
            }

            ImGui::Dummy(ImVec2(0, 4.0f));

            if (ImGui::Button("Dar Caja Shattered Web + Llave", ImVec2(btnW, btnH))) {
                uint64_t steamId = s_uiServices->gc->GetActiveSteamID();
                s_uiServices->inventory->GrantItem(steamId, 4697, 1, 4, 1, 0, 0.001f, 0); // Shattered Web Case
                s_uiServices->inventory->GrantItem(steamId, 1203, 1, 4, 1, 0, 0.001f, 0); // Shattered Web Key
                s_uiServices->engine->EnqueueCommand("econ_clear_inventory_images");
                if (s_uiServices->Log) s_uiServices->Log("[CSGO Mod] Awarded Shattered Web Case + Key to SteamID %llu", steamId);
            }
            ImGui::SameLine();
            bool isPrime = s_uiServices->entitlements->HasEntitlement(s_uiServices->gc->GetActiveSteamID(), 54029);
            std::string primeLabel = isPrime ? "Prime Status ACTIVO" : "Activar Prime Status (Gratis)";
            if (ImGui::Button(primeLabel.c_str(), ImVec2(btnW, btnH))) {
                uint64_t steamId = s_uiServices->gc->GetActiveSteamID();
                s_uiServices->entitlements->GrantEntitlement(steamId, 54029);
                s_uiServices->entitlements->GrantEntitlement(steamId, 10001);
                if (s_uiServices->Log) s_uiServices->Log("[CSGO Mod] Activated Prime Status for SteamID %llu", steamId);
            }
        });

    // 2. Register Give Menu Modal
    services->overlay->RegisterModal("csgo_give_menu",
        [](float width, float height) {
            auto& schema = refix::csgo::SchemaManager::Get();
            schema.Load();

            ImVec2 vp = ImGui::GetIO().DisplaySize;
            float winW = (std::min)(1050.0f, vp.x - 30.0f);
            float winH = (std::min)(680.0f, vp.y - 20.0f);
            ImGui::SetNextWindowPos(ImVec2((vp.x - winW) * 0.5f, (vp.y - winH) * 0.5f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Appearing);

            bool isOpen = true;
            if (ImGui::Begin("Mercado ReFix - Gestor de Inventario Local##csgo_mod_market", &isOpen,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

                ImGui::Text("Mercado ReFix y Gestor de Inventario Oficial (CS:GO)");
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 32.0f);
                if (ImGui::Button("X", ImVec2(24.0f, 24.0f))) {
                    isOpen = false;
                }

                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 6.0f));

                ImGui::InputTextWithHint("##m_search", "Buscar arma, skin, cuchillo, caja...", s_search, sizeof(s_search));
                ImGui::Dummy(ImVec2(0, 6.0f));

                float sideW = 185.0f;
                float detailW = 270.0f;
                float availW = ImGui::GetContentRegionAvail().x;
                float availH = ImGui::GetContentRegionAvail().y - 36.0f;
                float gridW = availW - sideW - detailW - 24.0f;

                // Categories Sidebar
                ImGui::BeginChild("##cat_side", ImVec2(sideW, availH), true);
                const auto& categories = schema.GetCategories();
                for (const auto& cat : categories) {
                    bool sel = (s_selectedCat == cat.name);
                    char lbl[96];
                    snprintf(lbl, sizeof(lbl), "%s (%zu)", cat.name.c_str(), cat.count);
                    if (ImGui::Selectable(lbl, sel)) {
                        s_selectedCat = cat.name;
                        s_selectedIdx = 0;
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Items Grid
                std::vector<refix::csgo::MarketItem> visibleItems;
                const auto& all = schema.GetMarketItems();
                if (s_search[0] != '\0') {
                    std::string q = s_search;
                    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                    for (const auto& itm : all) {
                        std::string lowerName = itm.name;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                        if (lowerName.find(q) != std::string::npos) {
                            visibleItems.push_back(itm);
                        }
                    }
                } else if (s_selectedCat == "Todos") {
                    visibleItems = all;
                } else {
                    visibleItems = schema.GetMarketItemsByCategory(s_selectedCat);
                }

                ImGui::BeginChild("##grid_view", ImVec2(gridW, availH), true);
                if (visibleItems.empty()) {
                    ImGui::Text("No se encontraron ítems.");
                } else {
                    for (size_t i = 0; i < visibleItems.size(); ++i) {
                        const auto& itm = visibleItems[i];
                        bool sel = ((int)i == s_selectedIdx);
                        char itemLbl[128];
                        snprintf(itemLbl, sizeof(itemLbl), "%s##item_%zu", itm.name.c_str(), i);
                        if (ImGui::Selectable(itemLbl, sel)) {
                            s_selectedIdx = (int)i;
                        }
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Item Details & Controls
                ImGui::BeginChild("##detail_view", ImVec2(detailW, availH), true);
                if (!visibleItems.empty() && s_selectedIdx >= 0 && s_selectedIdx < (int)visibleItems.size()) {
                    const auto& cur = visibleItems[s_selectedIdx];
                    ImGui::TextWrapped("%s", cur.name.c_str());
                    ImGui::TextDisabled("Def: %u | Paint: %u", cur.defIndex, cur.paintKit);

                    ImGui::Separator();
                    ImGui::SliderFloat("Desgaste", &s_wear, 0.0001f, 1.0f, "%.4f");
                    ImGui::SliderInt("Seed", &s_seed, 1, 1000);
                    ImGui::Checkbox("StatTrak", &s_statTrak);

                    ImGui::Dummy(ImVec2(0, 10.0f));
                    if (ImGui::Button("Añadir al inventario", ImVec2(detailW - 16.0f, 36.0f))) {
                        uint64_t steamId = s_uiServices->gc->GetActiveSteamID();
                        uint32_t qual = s_statTrak ? 9 : cur.quality;
                        uint64_t id = s_uiServices->inventory->GrantItem(steamId, cur.defIndex, 1, qual, cur.rarity, cur.paintKit, s_wear, s_seed);
                        s_uiServices->engine->EnqueueCommand("econ_clear_inventory_images");

                        s_statusMsg = "Ítem añadido con éxito!";
                        s_statusExp = GetTickCount() + 3000;
                        if (s_uiServices->Log) {
                            s_uiServices->Log("[CSGO Mod] Granted item def %u, id %llu to SteamID %llu", cur.defIndex, id, steamId);
                        }
                    }

                    if (GetTickCount() < s_statusExp && !s_statusMsg.empty()) {
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "%s", s_statusMsg.c_str());
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();

            if (!isOpen) {
                s_uiServices->overlay->CloseModal("csgo_give_menu");
            }
        });

    if (services->Log) {
        services->Log("[CSGO Mod] Registered overlay action card and give menu modal");
    }
}

} // namespace refix::csgo
