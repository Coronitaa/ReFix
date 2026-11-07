// "Mercado & Skins": the whole item catalogue of the installed CS:GO build (items_game.txt), with item art,
// skin attributes, Prime status and operation passes.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "../overlay_theme.h"
#include "../overlay_i18n.h"
#include "../overlay_app.h"
#include "../overlay_ui_mods.h"
#include "../../csgo_schema.h"
#include "../../steam_gc_emu.h"
#include "csgo_item_icons.h"

namespace refix::overlay {

namespace {

using refix::csgo::MarketItem;
const char* const kPassesCategory = "__prime_passes";

struct MarketState {
    std::string category = "Todos";
    char search[128] = "";
    std::string lastCategory, lastSearch;
    std::vector<MarketItem> filtered;
    int selected = -1;
    float wear = 0.01f;
    int seed = 420;
    bool statTrak = false;
    char nameTag[64] = "";
    int quantity = 1;
    std::string status;
    ULONGLONG statusUntil = 0;
};
MarketState g_market;

ImU32 RarityColor(uint32_t rarity) {
    switch (rarity) {
    case 8: return IM_COL32(228, 174, 57, 255);   // contraband
    case 7: return IM_COL32(255, 215, 0, 255);    // extraordinary (knives, gloves)
    case 6: return IM_COL32(235, 75, 75, 255);    // covert
    case 5: return IM_COL32(211, 44, 230, 255);   // classified
    case 4: return IM_COL32(136, 71, 255, 255);   // restricted
    case 3: return IM_COL32(75, 105, 255, 255);   // mil-spec
    case 2: return IM_COL32(94, 152, 217, 255);   // industrial
    default: return IM_COL32(176, 195, 217, 255); // consumer
    }
}

const char* RarityName(uint32_t rarity) {
    switch (rarity) {
    case 8: return i18n::T("Contraband", "Contrabando");
    case 7: return i18n::T("Extraordinary", "Extraordinario");
    case 6: return i18n::T("Covert", "Encubierto");
    case 5: return i18n::T("Classified", "Clasificado");
    case 4: return i18n::T("Restricted", "Restringido");
    case 3: return i18n::T("Mil-Spec", "Calidad militar");
    case 2: return i18n::T("Industrial Grade", "Calidad industrial");
    default: return i18n::T("Consumer Grade", "Calidad de consumo");
    }
}

struct Condition { const char* tag; const char* es; const char* en; float lo, hi; };
const Condition kConditions[] = {
    { "FN", "Recién fabricado", "Factory New", 0.00f, 0.07f },
    { "MW", "Casi nuevo", "Minimal Wear", 0.07f, 0.15f },
    { "FT", "Algo desgastado", "Field-Tested", 0.15f, 0.38f },
    { "WW", "Bastante desgastado", "Well-Worn", 0.38f, 0.45f },
    { "BS", "Deplorable", "Battle-Scarred", 0.45f, 1.00f },
};

const Condition& ConditionOf(float wear) {
    for (const Condition& c : kConditions)
        if (wear < c.hi) return c;
    return kConditions[4];
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

void SetStatus(const std::string& msg) {
    g_market.status = msg;
    g_market.statusUntil = GetTickCount64() + 3500;
}

void RefreshFiltered(refix::csgo::SchemaManager& schema) {
    if (g_market.category == g_market.lastCategory && g_market.search == g_market.lastSearch) return;
    g_market.lastCategory = g_market.category;
    g_market.lastSearch = g_market.search;
    g_market.filtered.clear();
    g_market.selected = -1;
    std::string q = Lower(g_market.search);
    for (const MarketItem& it : schema.GetMarketItems()) {
        if (g_market.category != "Todos" && it.category != g_market.category) continue;
        if (!q.empty() && Lower(it.name).find(q) == std::string::npos && Lower(it.weaponName).find(q) == std::string::npos)
            continue;
        g_market.filtered.push_back(it);
    }
}

void SelectItem(int idx) {
    g_market.selected = idx;
    const MarketItem& it = g_market.filtered[idx];
    g_market.wear = (std::max)(it.minWear, 0.01f);
    g_market.seed = 420;
    g_market.statTrak = false;
    g_market.nameTag[0] = '\0';
    g_market.quantity = 1;
}

// Rarity-tinted backdrop shared by cards and the detail preview.
void ArtBackdrop(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 rarity, float rounding) {
    dl->AddRectFilled(a, b, ui::color::Row, rounding);
    ImU32 top = ui::Fade(rarity, 0.05f), bottom = ui::Fade(rarity, 0.28f);
    dl->AddRectFilledMultiColor(ImVec2(a.x + 1, a.y + 1), ImVec2(b.x - 1, b.y - 1), top, top, bottom, bottom);
}

void DrawCategoryRail(refix::csgo::SchemaManager& schema, float w, float h) {
    ImGui::BeginChild("##mkt_rail", ImVec2(w, h), false);
    auto railItem = [&](const char* id, const char* label, size_t count, bool special) {
        bool sel = g_market.category == id;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(w - ui::S(6), ui::S(32));
        ImGui::PushID(id);
        bool clicked = ImGui::InvisibleButton("##cat", sz);
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (sel || hov) dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), sel ? ui::color::RowHover : ui::color::Row, ui::S(8));
        if (sel) dl->AddRectFilled(p, ImVec2(p.x + ui::S(3), p.y + sz.y), ui::color::Accent, ui::S(2));
        ImU32 col = special ? ui::color::Star : (sel ? ui::color::Text : ui::color::Muted);
        dl->AddText(ImVec2(p.x + ui::S(12), p.y + (sz.y - ImGui::GetFontSize()) * 0.5f), col, label);
        if (count) {
            char n[16];
            snprintf(n, sizeof(n), "%zu", count);
            ImVec2 ts = ImGui::CalcTextSize(n);
            dl->AddText(ImVec2(p.x + sz.x - ts.x - ui::S(10), p.y + (sz.y - ts.y) * 0.5f), ui::color::Faint, n);
        }
        if (clicked) g_market.category = id;
    };
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Faint, i18n::T("SPECIAL", "ESPECIAL"));
    ImGui::PopFont();
    railItem(kPassesCategory, i18n::T("★ Prime & passes", "★ Prime y pases"), 0, true);
    ImGui::Dummy(ImVec2(0, ui::S(6)));
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Faint, i18n::T("CATEGORIES", "CATEGORÍAS"));
    ImGui::PopFont();
    for (const auto& c : schema.GetCategories())
        if (c.name != "Pases") railItem(c.name.c_str(), c.name.c_str(), c.count, false);
    ImGui::EndChild();
}

void DrawItemCard(int idx, const MarketItem& it, float cw, float ch) {
    ImGui::PushID(idx);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##card", ImVec2(cw, ch));
    bool hov = ImGui::IsItemHovered();
    ImGui::PopID();
    if (clicked) SelectItem(idx);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool sel = g_market.selected == idx;
    ImU32 rc = RarityColor(it.rarity);
    float r = ui::S(10);
    dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + ch), hov || sel ? ui::color::RowHover : ui::color::Panel, r);
    ImVec2 artMin(p.x + ui::S(6), p.y + ui::S(6)), artMax(p.x + cw - ui::S(6), p.y + ch * 0.62f);
    ArtBackdrop(dl, artMin, artMax, rc, ui::S(8));
    csgo_icons::DrawItemArt(dl, it, ImVec2(artMin.x + ui::S(6), artMin.y + ui::S(4)), ImVec2(artMax.x - ui::S(6), artMax.y - ui::S(4)), ui::S(8));
    dl->AddRectFilled(ImVec2(artMin.x, artMax.y - ui::S(3)), artMax, rc, ui::S(2));

    std::string weapon = it.name, skin;
    size_t bar = weapon.find(" | ");
    if (bar != std::string::npos) { skin = weapon.substr(bar + 3); weapon = weapon.substr(0, bar); }
    float tx = p.x + ui::S(10), ty = artMax.y + ui::S(6), tw = cw - ui::S(20);
    ImGui::PushFont(ui::F().caption);
    dl->AddText(ImVec2(tx, ty), skin.empty() ? ui::color::Text : ui::color::Muted, ui::Ellipsize(weapon, tw).c_str());
    if (!skin.empty()) dl->AddText(ImVec2(tx, ty + ImGui::GetFontSize() + ui::S(2)), rc, ui::Ellipsize(skin, tw).c_str());
    ImGui::PopFont();
    if (sel) dl->AddRect(p, ImVec2(p.x + cw, p.y + ch), ui::color::Accent, r, 0, ui::S(2));
    else if (hov) dl->AddRect(p, ImVec2(p.x + cw, p.y + ch), ui::Fade(rc, 0.6f), r, 0, ui::S(1));
}

void DrawGrid(float w, float h) {
    ImGui::BeginChild("##mkt_grid", ImVec2(w, h), false);
    const float gap = ui::S(10), cw = ui::S(156), ch = ui::S(168);
    int cols = (std::max)(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (cw + gap)));
    int rows = ((int)g_market.filtered.size() + cols - 1) / cols;
    if (g_market.filtered.empty()) {
        ImGui::Dummy(ImVec2(0, ui::S(40)));
        ui::TextColored(ui::color::Muted, i18n::T("No items match your search.", "Ningún objeto coincide con la búsqueda."));
    }
    ImGuiListClipper clipper;
    clipper.Begin(rows, ch + gap);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            for (int col = 0; col < cols; ++col) {
                int idx = row * cols + col;
                if (idx >= (int)g_market.filtered.size()) break;
                if (col) ImGui::SameLine(0, gap);
                DrawItemCard(idx, g_market.filtered[idx], cw, ch);
            }
            ImGui::Dummy(ImVec2(0, gap - ImGui::GetStyle().ItemSpacing.y));
        }
    }
    ImGui::EndChild();
}

void Label(const char* text) {
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(ui::color::Faint, text);
    ImGui::PopFont();
}

void DrawDetail(float w, float h) {
    ImGui::BeginChild("##mkt_detail", ImVec2(w, h), false);
    if (g_market.selected < 0 || g_market.selected >= (int)g_market.filtered.size()) {
        ImGui::Dummy(ImVec2(0, ui::S(60)));
        ImGui::PushTextWrapPos(w - ui::S(12));
        ui::TextColored(ui::color::Muted, i18n::T("Pick an item to preview it and add it to your inventory.",
                                                  "Elige un objeto para verlo y añadirlo a tu inventario."));
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        return;
    }
    const MarketItem& it = g_market.filtered[g_market.selected];
    ImU32 rc = RarityColor(it.rarity);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float artH = ui::S(180);
    ArtBackdrop(dl, p, ImVec2(p.x + w - ui::S(8), p.y + artH), rc, ui::S(12));
    csgo_icons::DrawItemArt(dl, it, ImVec2(p.x + ui::S(14), p.y + ui::S(10)), ImVec2(p.x + w - ui::S(22), p.y + artH - ui::S(10)), ui::S(12));
    dl->AddRectFilled(ImVec2(p.x, p.y + artH - ui::S(4)), ImVec2(p.x + w - ui::S(8), p.y + artH), rc, ui::S(2));
    if (it.paintKit && !csgo_icons::HasRenderedSkin(it)) {
        ImGui::PushFont(ui::F().caption);
        dl->AddText(ImVec2(p.x + ui::S(10), p.y + ui::S(8)), ui::color::Muted, i18n::T("Paint pattern preview", "Vista del patrón de pintura"));
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0, artH + ui::S(8)));

    ImGui::PushTextWrapPos(w - ui::S(12));
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted(it.name.c_str());
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ui::TextColored(rc, RarityName(it.rarity));
    ImGui::SameLine();
    ui::TextColored(ui::color::Faint, ("· " + it.category).c_str());
    ImGui::Dummy(ImVec2(0, ui::S(6)));

    float fullW = w - ui::S(12);
    if (it.paintKit) {
        Label(i18n::T("CONDITION", "ESTADO"));
        float segW = (fullW - ImGui::GetStyle().ItemSpacing.x * 4) / 5.0f;
        for (int i = 0; i < 5; ++i) {
            const Condition& c = kConditions[i];
            bool possible = c.hi > it.minWear && c.lo < it.maxWear;
            bool active = &ConditionOf(g_market.wear) == &c;
            if (i) ImGui::SameLine();
            bool clicked = active ? ui::PrimaryButton(c.tag, ImVec2(segW, ui::S(28)), possible)
                                  : ui::SecondaryButton(c.tag, ImVec2(segW, ui::S(28)), possible);
            if (ImGui::IsItemHovered()) ui::Tooltip(i18n::T(c.en, c.es));
            if (ui::Clicked(clicked)) g_market.wear = (std::max)(c.lo, it.minWear) + ((std::min)(c.hi, it.maxWear) - (std::max)(c.lo, it.minWear)) * 0.5f;
        }
        char wearFmt[64];
        snprintf(wearFmt, sizeof(wearFmt), "%%.4f  %s", i18n::T(ConditionOf(g_market.wear).en, ConditionOf(g_market.wear).es));
        ImGui::SetNextItemWidth(fullW);
        ImGui::SliderFloat("##wear", &g_market.wear, it.minWear, (std::max)(it.maxWear, it.minWear + 0.001f), wearFmt);

        Label(i18n::T("PATTERN SEED", "SEMILLA DEL PATRÓN"));
        ImGui::SetNextItemWidth(fullW - ui::S(84));
        ImGui::InputInt("##seed", &g_market.seed);
        g_market.seed = std::clamp(g_market.seed, 0, 1000);
        ImGui::SameLine();
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Random", "Azar"), ImVec2(ui::S(76), 0)))) g_market.seed = rand() % 1001;
        ImGui::Checkbox("StatTrak™", &g_market.statTrak);
        Label(i18n::T("NAME TAG", "ETIQUETA DE NOMBRE"));
        ImGui::SetNextItemWidth(fullW);
        ImGui::InputTextWithHint("##tag", i18n::T("Optional", "Opcional"), g_market.nameTag, sizeof(g_market.nameTag));
        ImGui::Dummy(ImVec2(0, ui::S(4)));
    }

    Label(i18n::T("QUANTITY", "CANTIDAD"));
    if (ui::Clicked(ui::SecondaryButton("-", ImVec2(ui::S(32), ui::S(30))))) g_market.quantity = (std::max)(1, g_market.quantity - 1);
    ImGui::SameLine();
    char q[16];
    snprintf(q, sizeof(q), "%d", g_market.quantity);
    ImGui::PushFont(ui::F().strong);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(q);
    ImGui::PopFont();
    ImGui::SameLine();
    if (ui::Clicked(ui::SecondaryButton("+", ImVec2(ui::S(32), ui::S(30))))) g_market.quantity = (std::min)(100, g_market.quantity + 1);
    for (int preset : { 5, 20 }) {
        ImGui::SameLine();
        char b[8];
        snprintf(b, sizeof(b), "x%d", preset);
        if (ui::Clicked(ui::SecondaryButton(b, ImVec2(ui::S(42), ui::S(30))))) g_market.quantity = preset;
    }

    ImGui::Dummy(ImVec2(0, ui::S(6)));
    if (ui::Clicked(ui::PrimaryButton(i18n::T("Add to inventory", "Añadir al inventario"), ImVec2(fullW, ui::S(40))))) {
        uint32_t quality = (g_market.statTrak && it.paintKit) ? 9 : it.quality;
        CSGO_GrantItemToLocal(it.defIndex, (uint32_t)g_market.quantity, quality, it.rarity, it.paintKit, g_market.wear,
                              (uint32_t)g_market.seed, g_market.nameTag[0] ? g_market.nameTag : nullptr);
        char msg[160];
        snprintf(msg, sizeof(msg), "%s %dx %s", i18n::T("Added", "Añadido"), g_market.quantity, it.name.c_str());
        SetStatus(msg);
    }
    if (GetTickCount64() < g_market.statusUntil) {
        ImGui::PushTextWrapPos(w - ui::S(12));
        ui::TextColored(ui::color::Online, g_market.status.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
}

void DrawPassCard(const MarketItem& pass, float cw, float ch) {
    ImGui::PushID((int)pass.defIndex);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool active = pass.seasonAccess >= 0 && CSGO_HasOperationPass((uint32_t)pass.seasonAccess);
    ImU32 accent = active ? ui::color::Online : ui::color::Star;
    dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + ch), ui::color::Panel, ui::S(12));
    ImVec2 artMin(p.x + ui::S(8), p.y + ui::S(8)), artMax(p.x + cw - ui::S(8), p.y + ch * 0.55f);
    ArtBackdrop(dl, artMin, artMax, accent, ui::S(10));
    if (ImTextureID t = csgo_icons::BaseImage(pass.imageInventory)) {
        float s = artMax.y - artMin.y - ui::S(8);
        float x = (artMin.x + artMax.x - s) * 0.5f;
        dl->AddImage(t, ImVec2(x, artMin.y + ui::S(4)), ImVec2(x + s, artMin.y + ui::S(4) + s));
    }
    ImGui::Dummy(ImVec2(0, artMax.y - p.y + ui::S(4)));
    ImGui::SetCursorScreenPos(ImVec2(p.x + ui::S(12), artMax.y + ui::S(6)));
    ImGui::PushTextWrapPos(p.x + cw - ui::S(12));
    ImGui::PushFont(ui::F().strong);
    ImGui::TextUnformatted(pass.name.c_str());
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(p.x + ui::S(12), ImGui::GetCursorScreenPos().y));
    char season[48];
    snprintf(season, sizeof(season), i18n::T("Season %d", "Temporada %d"), pass.seasonAccess + 1);
    ui::TextColored(ui::color::Muted, season);
    ImGui::SetCursorScreenPos(ImVec2(p.x + ui::S(12), p.y + ch - ui::S(44)));
    if (active) {
        ui::TextColored(ui::color::Online, (std::string(ui::icon::Check) + " " + i18n::T("Active", "Activo")).c_str());
    } else if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate pass", "Activar pase"), ImVec2(cw - ui::S(24), ui::S(32)),
                                             pass.seasonAccess >= 0))) {
        CSGO_ActivateOperationPass((uint32_t)pass.seasonAccess, 0);
        SetStatus(std::string(i18n::T("Activated: ", "Activado: ")) + pass.name);
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(cw, ch));
    ImGui::PopID();
}

void DrawPrimeAndPasses(refix::csgo::SchemaManager& schema, float w, float h) {
    ImGui::BeginChild("##mkt_passes", ImVec2(w, h), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float bannerH = ui::S(118);
    bool prime = CSGO_IsPrimeStatus();
    ImU32 pc = prime ? ui::color::Online : ui::color::Star;
    dl->AddRectFilled(p, ImVec2(p.x + w - ui::S(8), p.y + bannerH), ui::color::Panel, ui::S(14));
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + (w - ui::S(8)) * 0.5f, p.y + bannerH), ui::Fade(pc, 0.22f), ui::Fade(pc, 0.0f),
                                ui::Fade(pc, 0.0f), ui::Fade(pc, 0.22f));
    ImGui::SetCursorScreenPos(ImVec2(p.x + ui::S(20), p.y + ui::S(16)));
    ImGui::BeginGroup();
    ImGui::PushFont(ui::F().heading);
    ui::TextColored(pc, i18n::T("Prime Status", "Estado Prime"));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(p.x + w - ui::S(280));
    ui::TextColored(ui::color::Muted, i18n::T("Prime matchmaking, drops and Prime-only rewards for this profile.",
                                              "Emparejamiento Prime, drops y recompensas exclusivas para este perfil."));
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - ui::S(250), p.y + (bannerH - ui::S(40)) * 0.5f));
    if (prime) {
        if (ui::Clicked(ui::SecondaryButton(i18n::T("Turn Prime off", "Desactivar Prime"), ImVec2(ui::S(220), ui::S(40))))) CSGO_SetPrimeStatus(false);
    } else if (ui::Clicked(ui::PrimaryButton(i18n::T("Activate Prime", "Activar Prime"), ImVec2(ui::S(220), ui::S(40))))) {
        CSGO_SetPrimeStatus(true);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + bannerH + ui::S(16)));

    ui::SectionLabel(i18n::T("Operation passes in this build", "Pases de operación de esta versión"));
    std::vector<MarketItem> passes = schema.GetMarketItemsByCategory("Pases");
    std::sort(passes.begin(), passes.end(), [](const MarketItem& a, const MarketItem& b) { return a.seasonAccess > b.seasonAccess; });
    if (passes.empty())
        ui::TextColored(ui::color::Muted, i18n::T("This build's item schema has no operation passes.",
                                                  "El esquema de esta versión no tiene pases de operación."));
    const float gap = ui::S(12), cw = ui::S(210), ch = ui::S(250);
    int cols = (std::max)(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (cw + gap)));
    for (size_t i = 0; i < passes.size(); ++i) {
        if (i % cols) ImGui::SameLine(0, gap);
        DrawPassCard(passes[i], cw, ch);
    }
    if (GetTickCount64() < g_market.statusUntil) ui::TextColored(ui::color::Online, g_market.status.c_str());
    ImGui::EndChild();
}

} // namespace

void DrawMarketGiveModal() {
    if (!App::Get().IsMarketHUDOpen()) return;
    auto& schema = refix::csgo::SchemaManager::Get();
    schema.Load();
    csgo_icons::BeginFrame();

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImVec2 size((std::min)(ui::S(1220), vp.x - ui::S(40)), (std::min)(ui::S(760), vp.y - ui::S(40)));
    ImGui::SetNextWindowPos(ImVec2((vp.x - size.x) * 0.5f, (vp.y - size.y) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::S(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(22), ui::S(18)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::color::BackdropSolid);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);
    if (ImGui::Begin("##refix_csgo_market", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        // Header: title, catalogue size, profile status, close.
        ImGui::PushFont(ui::F().title);
        ui::TextColored(ui::color::Accent, ui::icon::Store);
        ImGui::SameLine();
        ImGui::TextUnformatted(i18n::T("Market & Skins", "Mercado & Skins"));
        ImGui::PopFont();
        ImGui::SameLine();
        char sub[96];
        snprintf(sub, sizeof(sub), i18n::T("%zu items from your game version", "%zu objetos de tu versión del juego"), schema.GetTotalGrantableCount());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ui::S(8));
        ui::TextColored(ui::color::Faint, sub);

        float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(right - ui::S(250));
        bool prime = CSGO_IsPrimeStatus();
        ui::TextColored(prime ? ui::color::Online : ui::color::Faint,
                        prime ? (std::string(ui::icon::Check) + " Prime").c_str() : i18n::T("No Prime", "Sin Prime"));
        ImGui::SameLine(right - ui::S(30));
        if (ui::IconButton("mkt_close", ui::icon::Close, i18n::T("Close", "Cerrar"), ui::color::Muted, true, ui::S(26)))
            App::Get().CloseMarketHUD();

        ImGui::Dummy(ImVec2(0, ui::S(4)));
        ImGui::SetNextItemWidth(ui::S(420));
        ImGui::InputTextWithHint("##mkt_search", i18n::T("Search weapon, skin, knife, case…", "Buscar arma, skin, cuchillo, caja…"),
                                 g_market.search, sizeof(g_market.search));
        ImGui::Dummy(ImVec2(0, ui::S(8)));

        float railW = ui::S(200), detailW = ui::S(320), gap = ui::S(16);
        float bodyH = ImGui::GetContentRegionAvail().y;
        float centerW = ImGui::GetContentRegionAvail().x - railW - gap * 2 - detailW;
        DrawCategoryRail(schema, railW, bodyH);
        ImGui::SameLine(0, gap);
        if (g_market.category == kPassesCategory) {
            DrawPrimeAndPasses(schema, centerW + gap + detailW, bodyH);
        } else {
            RefreshFiltered(schema);
            DrawGrid(centerW, bodyH);
            ImGui::SameLine(0, gap);
            DrawDetail(detailW, bodyH);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace refix::overlay
