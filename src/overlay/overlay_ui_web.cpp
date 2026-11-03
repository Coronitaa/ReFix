// Browser modal of the overlay: toolbar drawn with ImGui, page area filled by the WebView2 child window
// (overlay_web.cpp), as Steam shows store and community pages inside its overlay.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "overlay_web.h"

#include <algorithm>
#include <cmath>

namespace refix::overlay {
namespace {

constexpr float kWebMarginX = 56.0f;
constexpr float kWebMarginY = 40.0f;
constexpr float kToolbarHeight = 52.0f;
constexpr float kToolButton = 34.0f;
constexpr float kProgressHeight = 2.0f;

void CenteredText(ImDrawList* dl, ImFont* font, ImVec2 center, ImU32 col, const char* text) {
    const ImVec2 sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, text);
    dl->AddText(font, font->FontSize, ImVec2(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f), col, text);
}

} // namespace

void App::DrawWebModal(ImVec2 viewport) {
    if (!webOpen_) return;
    // ImGui drops the popup while the overlay is hidden (no window is submitted then): reopen it whenever
    // the browser is open but its popup is not.
    if (webOpenRequested_ || !ImGui::IsPopupOpen("##web")) {
        ImGui::OpenPopup("##web");
        webOpenRequested_ = false;
    }
    const ImVec2 size(viewport.x - 2 * ui::S(kWebMarginX), viewport.y - 2 * ui::S(kWebMarginY));
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(12));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::Panel);
    const bool open = ImGui::BeginPopupModal("##web", nullptr,
                                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!open) return;

    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetWindowPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    const float bar = ui::S(kToolbarHeight);
    const float btn = ui::S(kToolButton);
    const float cy = p0.y + bar * 0.5f;
    const float pad = ui::S(12);
    const web::State state = web::GetState();
    bool close = false;

    // Toolbar: back, forward, reload/stop | title and address | open in browser, close. Its tooltips open
    // above the buttons: the page below is a separate window that would cover them.
    ui::SetTooltipsAbove(true);
    float x = p0.x + pad;
    auto tool = [&](const char* id, const char* glyph, const char* tip, bool enabled) {
        ImGui::SetCursorScreenPos(ImVec2(x, cy - btn * 0.5f));
        const bool hit = ui::IconButton(id, glyph, tip, ui::color::Text, enabled, btn);
        x += btn + ui::S(4);
        return hit;
    };
    if (tool("back", ui::icon::ChevronLeft, i18n::T("Back", "Atrás"), web::CanGoBack())) web::Back();
    if (tool("fwd", ui::icon::ChevronRight, i18n::T("Forward", "Adelante"), web::CanGoForward())) web::Forward();
    const bool loading = web::Loading();
    if (loading) {
        if (tool("stop", ui::icon::Close, i18n::T("Stop", "Detener"), state == web::State::Ready)) web::Stop();
    } else if (tool("reload", ui::icon::Refresh, i18n::T("Reload", "Recargar"), state == web::State::Ready)) {
        web::Reload();
    }

    float right = p1.x - pad - btn;
    ImGui::SetCursorScreenPos(ImVec2(right, cy - btn * 0.5f));
    if (ui::IconButton("webclose", ui::icon::Close, i18n::T("Close (Esc)", "Cerrar (Esc)"), ui::color::Text, true, btn))
        close = true;
    right -= btn + ui::S(4);
    ImGui::SetCursorScreenPos(ImVec2(right, cy - btn * 0.5f));
    const std::wstring url = web::Url().empty() ? webUrl_ : web::Url();
    if (ui::IconButton("external", ui::icon::OpenExternal, i18n::T("Open in your browser", "Abrir en tu navegador"),
                       ui::color::Text, true, btn)) {
        ui::SetTooltipsAbove(false);
        CloseWeb();
        OpenUrl(url);   // closes the overlay too: the browser comes to the front
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ui::SetTooltipsAbove(false);
    const float textX = x + ui::S(10);
    const float textW = std::max(ui::S(40), right - ui::S(14) - textX);
    std::string title = ToUtf8(web::Title());
    if (title.empty()) title = i18n::T("Loading…", "Cargando…");
    ImGui::PushFont(f.strong);
    const std::string shownTitle = ui::Ellipsize(title, textW);
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    const std::string shownUrl = ui::Ellipsize(ToUtf8(url), textW);
    ImGui::PopFont();
    const float lineY = cy - (f.strong->FontSize + f.caption->FontSize + ui::S(2)) * 0.5f;
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(textX, lineY), ui::color::Text, shownTitle.c_str());
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(textX, lineY + f.strong->FontSize + ui::S(2)), ui::color::Muted,
                shownUrl.c_str());
    dl->AddLine(ImVec2(p0.x, p0.y + bar), ImVec2(p1.x, p0.y + bar), ui::color::Border);

    // Page area. What is drawn here shows only until the web view paints over it.
    const ImVec2 c0(p0.x + 1, p0.y + bar + 1);
    const ImVec2 c1(p1.x - 1, p1.y - ui::S(10));
    if (state == web::State::Failed) {
        CenteredText(dl, f.strong, ImVec2((c0.x + c1.x) * 0.5f, (c0.y + c1.y) * 0.5f - ui::S(30)), ui::color::Text,
                     i18n::T("This page cannot be shown inside the overlay.",
                             "Esta página no se puede mostrar dentro del overlay."));
        const char* label = i18n::T("Open in your browser", "Abrir en tu navegador");
        ImGui::PushFont(f.strong);
        const float bw = ImGui::CalcTextSize(label).x + ui::S(40);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2((c0.x + c1.x - bw) * 0.5f, (c0.y + c1.y) * 0.5f));
        if (ui::PrimaryButton(label, ImVec2(bw, ui::S(40)))) {
            CloseWeb();
            OpenUrl(url);
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
    } else {
        CenteredText(dl, f.caption, ImVec2((c0.x + c1.x) * 0.5f, (c0.y + c1.y) * 0.5f), ui::color::Muted,
                     i18n::T("Loading…", "Cargando…"));
        if (loading) {
            // Indeterminate progress line under the toolbar.
            const float t = (float)fmod(ImGui::GetTime() * 0.8, 1.0);
            const float w = (c1.x - c0.x) * 0.3f;
            const float sx = c0.x + (c1.x - c0.x + w) * t - w;
            dl->AddRectFilled(ImVec2(std::max(c0.x, sx), p0.y + bar - ui::S(kProgressHeight)),
                              ImVec2(std::min(c1.x, sx + w), p0.y + bar), ui::color::Accent);
        }
        const RECT r{ (LONG)std::ceil(c0.x), (LONG)std::ceil(c0.y), (LONG)std::floor(c1.x), (LONG)std::floor(c1.y) };
        web::SetBounds(r);
        webDrawn_ = true;
    }
    ImGui::SetCursorScreenPos(c0);
    ImGui::Dummy(ImVec2(c1.x - c0.x, c1.y - c0.y));

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || web::TakeCloseRequest())
        close = true;
    if (close) {
        CloseWeb();
        webDrawn_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace refix::overlay
