#include "overlay_actions.h"
#include "../overlay_app.h"

namespace refix::overlay {

OverlayActionsRegistry& OverlayActionsRegistry::Get() {
    static OverlayActionsRegistry s_instance;
    return s_instance;
}

OverlayActionsRegistry::OverlayActionsRegistry() {
    refix::GetRefixServices().overlay = this;
}

void OverlayActionsRegistry::RegisterCard(const char* id, const char* title, const char* subtitle, OverlayRenderFn render_fn) {
    if (!id || !render_fn) return;
    std::lock_guard<std::mutex> lock(m_mu);
    RegisteredCard card;
    card.id = id;
    card.title = title ? title : "";
    card.subtitle = subtitle ? subtitle : "";
    card.renderFn = std::move(render_fn);
    m_cards.push_back(std::move(card));
}

void OverlayActionsRegistry::RegisterModal(const char* id, OverlayRenderFn render_fn) {
    if (!id || !render_fn) return;
    std::lock_guard<std::mutex> lock(m_mu);
    RegisteredModal modal;
    modal.id = id;
    modal.renderFn = std::move(render_fn);
    modal.isOpen = false;
    m_modals[id] = std::move(modal);
}

void OverlayActionsRegistry::OpenOverlay() {
    App::Get().Post([] { App::Get().OpenOverlay(); });
}

void OverlayActionsRegistry::CloseOverlay() {
    App::Get().Post([] { App::Get().CloseOverlay(); });
}

void OverlayActionsRegistry::ToggleOverlay() {
    App::Get().Post([] {
        if (App::Get().IsOverlayOpen()) App::Get().CloseOverlay();
        else App::Get().OpenOverlay();
    });
}

void OverlayActionsRegistry::OpenModal(const char* id) {
    if (!id) return;
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_modals.find(id);
    if (it != m_modals.end()) {
        it->second.isOpen = true;
    }
}

void OverlayActionsRegistry::CloseModal(const char* id) {
    if (!id) return;
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_modals.find(id);
    if (it != m_modals.end()) {
        it->second.isOpen = false;
    }
}

std::vector<RegisteredCard> OverlayActionsRegistry::GetCards() {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_cards;
}

std::vector<RegisteredModal> OverlayActionsRegistry::GetModals() {
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<RegisteredModal> res;
    for (const auto& [_, m] : m_modals) {
        if (m.isOpen) res.push_back(m);
    }
    return res;
}

bool OverlayActionsRegistry::IsModalOpen(const char* id) {
    if (!id) return false;
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_modals.find(id);
    if (it != m_modals.end()) {
        return it->second.isOpen;
    }
    return false;
}

} // namespace refix::overlay
