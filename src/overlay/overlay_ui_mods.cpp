// The overlay side of mod panels: the home screen's adaptive grid and the HUD panels drawn over the game.
// Which panels exist is decided by game_actions/mod_panels (game-scoped); this file only lays them out.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "overlay_ui_mods.h"
#include "game_actions/mod_panels.h"
#include "overlay_theme.h"
#include "overlay_i18n.h"
#include "../imgui/imgui_internal.h"
#include "../source_main_thread_queue.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace refix::overlay {

namespace {

using mods::Panel;
using mods::Registry;

constexpr float kCellMinW = 300.0f;        // unscaled; narrower areas get one column
constexpr float kCellPad = 14.0f;          // unscaled, BeginCard scales it
constexpr float kHeaderH = 30.0f;          // title row
constexpr float kSubtitleH = 20.0f;
constexpr float kDefaultContentH = 160.0f; // a panel without preferred_height
constexpr float kFaultedContentH = 64.0f;
constexpr float kGap = 16.0f;

RefixPanelDrawArgs MakeArgs(float w, float h) {
    RefixPanelDrawArgs a{};
    a.struct_size = sizeof(a);
    a.abi_version = REFIX_OVERLAY_ABI_VERSION;
    a.width = w;
    a.height = h;
    a.imgui_context = ImGui::GetCurrentContext();
    ImGuiMemAllocFunc alloc = nullptr;
    ImGuiMemFreeFunc free = nullptr;
    ImGui::GetAllocatorFunctions(&alloc, &free, &a.imgui_alloc_user_data);
    a.imgui_alloc = alloc;
    a.imgui_free = free;
    return a;
}

// Runs a mod's draw; if it crashed mid-frame or left ImGui's stacks unbalanced, puts them back and turns it off.
void GuardedDraw(const Panel& p, float w, float h) {
    ImGuiErrorRecoveryState before;
    ImGui::ErrorRecoveryStoreState(&before);
    const bool ok = Registry::Get().Draw(p, MakeArgs(w, h));
    ImGuiErrorRecoveryState after;
    ImGui::ErrorRecoveryStoreState(&after);
    if (ok && memcmp(&before, &after, sizeof(before)) == 0) return;
    ImGuiIO& io = ImGui::GetIO();
    const bool assertOn = io.ConfigErrorRecoveryEnableAssert;
    io.ConfigErrorRecoveryEnableAssert = false;
    ImGui::ErrorRecoveryTryToRecoverState(&before);
    io.ConfigErrorRecoveryEnableAssert = assertOn;
    if (ok) Registry::Get().Fault(p.owner, "left the overlay's ImGui state unbalanced");
}

float HeaderHeight(const Panel& p) { return ui::S(kHeaderH) + (p.subtitle.empty() ? 0.0f : ui::S(kSubtitleH)); }

float ContentWidth(float cellW) { return cellW - ui::S(kCellPad) * 2; }

float CellHeight(const Panel& p, float cellW) {
    float content = ui::S(kFaultedContentH);
    if (!p.faulted) {
        const float wanted = Registry::Get().PreferredHeight(p, ContentWidth(cellW));
        content = wanted > 0.0f ? wanted : ui::S(kDefaultContentH);
    }
    return HeaderHeight(p) + content + ui::S(kCellPad) * 2;
}

void DrawHeader(const Panel& p) {
    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Accent, ui::icon::Game);
    ImGui::SameLine();
    ImGui::TextUnformatted(p.title.c_str());
    ImGui::PopFont();
    if (!p.subtitle.empty()) ui::TextColored(ui::color::Muted, p.subtitle.c_str());
    ImGui::Dummy(ImVec2(0, ui::S(4)));
}

void DrawCell(const Panel& p, ImVec2 pos, ImVec2 size) {
    const std::string id = "##modcell_" + p.id;
    ui::BeginCard(id.c_str(), pos, size, kCellPad);
    DrawHeader(p);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (p.faulted) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail.x);
        ui::TextColored(ui::color::Muted, i18n::T("This mod stopped working and was turned off. The ReFix log says why.",
                                                  "Este mod dejó de funcionar y se desactivó. El log de ReFix dice por qué."));
        ImGui::PopTextWrapPos();
    } else {
        GuardedDraw(p, avail.x, avail.y);
    }
    ui::EndCard();
}

struct Grid {
    std::vector<Panel> panels;
    std::vector<mods::Cell> cells;
    float height = 0.0f;
};

Grid Layout(float width) {
    Grid g;
    g.panels = Registry::Get().Panels(REFIX_PANEL_SLOT_GRID);
    if (g.panels.empty()) return g;
    const float minW = (std::min)(ui::S(kCellMinW), width);
    const int cols = (std::max)(1, (int)((width + ui::S(kGap)) / (minW + ui::S(kGap))));
    const float cellW = (width - ui::S(kGap) * (cols - 1)) / cols;
    std::vector<float> wanted;
    for (const Panel& p : g.panels) wanted.push_back(CellHeight(p, cellW));
    g.cells = mods::LayoutGrid(width, wanted, minW, ui::S(kGap));
    g.height = g.cells.back().y + g.cells.back().h;
    return g;
}

} // namespace

void ExecuteEngineClientCommand(const char* cmd) {
    if (!cmd || !*cmd) return;
    refix::source::MainThreadQueue::ExecuteClientCmd(cmd);
}

float GetModGridHeight(float width) { return Layout(width).height; }

void DrawModGrid(ImVec2 pos, float width, float height) {
    Grid g = Layout(width);
    if (g.panels.empty() || height <= 0.0f) return;
    const bool oneRow = g.cells.back().y == 0.0f;
    if (g.height <= height || oneRow) {
        for (size_t i = 0; i < g.panels.size(); ++i) {
            const mods::Cell& c = g.cells[i];
            DrawCell(g.panels[i], ImVec2(pos.x + c.x, pos.y + c.y), ImVec2(c.w, (std::min)(c.h, height)));
        }
        return;
    }
    // More rows than room: the grid scrolls, every cell keeps its size.
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginChild("##mod_grid_scroll", ImVec2(width, height), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    for (size_t i = 0; i < g.panels.size(); ++i) {
        const mods::Cell& c = g.cells[i];
        DrawCell(g.panels[i], ImVec2(origin.x + c.x, origin.y + c.y), ImVec2(c.w, c.h));
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + g.height));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::EndChild();
}

void DrawModHuds(ImVec2 viewport) {
    for (const Panel& p : Registry::Get().OpenHuds()) GuardedDraw(p, viewport.x, viewport.y);
}

bool AnyModHudOpen() { return Registry::Get().AnyHudOpen(); }

} // namespace refix::overlay
