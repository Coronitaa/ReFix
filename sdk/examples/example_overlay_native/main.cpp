// Minimal native mod with one overlay panel: a cell in the overlay home's mod grid.
// Build (x64 game; use vcvars32 and /MACHINE:X86 for a 32-bit game), with Dear ImGui 1.91.x sources:
//   cl /nologo /O2 /EHsc /std:c++17 /LD /I<imgui> main.cpp <imgui>\imgui*.cpp /Feexample_overlay.dll
#include "../../include/refix_overlay_api.h"
#include "imgui.h"

namespace {

int g_clicks = 0;

void DrawPanel(void*, const RefixPanelDrawArgs* a) {
    // This DLL has its own ImGui copy: draw into ReFix's context with ReFix's allocator.
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(a->imgui_context));
    ImGui::SetAllocatorFunctions(a->imgui_alloc, a->imgui_free, a->imgui_alloc_user_data);
    ImGui::TextWrapped("Hello from a mod. This cell is %.0f px wide.", a->width);
    if (ImGui::Button("Click me", ImVec2(a->width, 0))) ++g_clicks;
    ImGui::Text("Clicked %d times", g_clicks);
}

float PanelHeight(void*, float) { return 90.0f; }

} // namespace

extern "C" {

__declspec(dllexport) int refix_mod_init(const void*) { return 0; }
__declspec(dllexport) void refix_mod_shutdown(void) {}

REFIX_OVERLAY_EXPORT int refix_mod_register_overlay(const RefixOverlayHost* host) {
    if (!host || host->struct_size < sizeof(RefixOverlayHost)) return -1;
    RefixPanelDesc panel{};
    panel.struct_size = sizeof(panel);
    panel.abi_version = REFIX_OVERLAY_ABI_VERSION;
    panel.id = "example.hello";
    panel.title = "Hello mod";
    panel.slot = REFIX_PANEL_SLOT_GRID;
    panel.draw = DrawPanel;
    panel.preferred_height = PanelHeight;
    return host->register_panel(host->token, &panel) == REFIX_OK ? 0 : -2;
}

} // extern "C"
