#include "overlay_ui_mods.h"
#include "overlay_theme.h"
#include "overlay_i18n.h"
#include "../mods/mod_host.h"
#include <algorithm>

namespace refix::overlay {

static bool s_showModsModal = false;

void DrawModsCard(ImVec2 pos, float width, float height) {
    ui::BeginCard("##mods_card", pos, ImVec2(width, height));

    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Accent, ui::icon::Workshop);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("ReFix Mods", "Mods de ReFix"));
    ImGui::PopFont();

    auto& host = mods::ModHost::GetInstance();
    const auto& loaded = host.GetLoadedMods();

    int active_count = 0;
    int pending_count = 0;
    int faulted_count = 0;

    for (const auto& [id, mod] : loaded) {
        if (mod.is_active) active_count++;
        if (!mod.is_approved) pending_count++;
        if (mod.is_faulted) faulted_count++;
    }

    ImGui::PushFont(ui::F().caption);
    if (host.IsDevMode()) {
        ui::TextColored(ui::color::Danger, i18n::T("[DEV MODE ENABLED]", "[MODO DESARROLLADOR ACTIVO]"));
    }

    if (faulted_count > 0) {
        ui::TextColored(ui::color::Danger, i18n::T("Fault detected: a mod crashed and was rolled back safely.",
                                                  "Fallo detectado: un mod falló y fue revertido con seguridad."));
    } else if (pending_count > 0) {
        ui::TextColored(ui::color::Star, i18n::T("Mods requiring your approval:", "Mods que requieren tu aprobación:"));
        ImGui::SameLine();
        ImGui::Text("%d", pending_count);
    } else {
        ui::TextColored(ui::color::Muted, i18n::T("Active mods:", "Mods activos:"));
        ImGui::SameLine();
        ImGui::Text("%d", active_count);
    }
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(4)));
    if (ui::Clicked(ui::NavButton(i18n::T("Manage Mods##btn", "Administrar Mods##btn"), ImVec2(width - ui::S(24), ui::S(28))))) {
        s_showModsModal = true;
        ImGui::OpenPopup("##mods_modal");
    }

    ui::EndCard();

    DrawModsApprovalModal();
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

            ImGui::TextUnformatted(i18n::T("Declared Capabilities:", "Capacidades Declaradas:"));
            for (const auto& cap : mod.manifest.capabilities) {
                char cap_buf[256];
                if (mods::Capability::IsHighRisk(cap)) {
                    snprintf(cap_buf, sizeof(cap_buf), i18n::T("  [!] %s (HIGH RISK PERMISSION)", "  [!] %s (PERMISO DE ALTO RIESGO)"), cap.c_str());
                    ui::TextColored(ui::color::Danger, cap_buf);
                } else {
                    snprintf(cap_buf, sizeof(cap_buf), "  [*] %s", cap.c_str());
                    ui::TextColored(ui::color::Muted, cap_buf);
                }
            }
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

} // namespace refix::overlay
