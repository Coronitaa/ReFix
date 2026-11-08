// Mod Overlay Actions Registry: Dynamic cards, modals, and navigation
#pragma once

#include "../../include/refix_services.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace refix::overlay {

struct RegisteredCard {
    std::string id;
    std::string title;
    std::string subtitle;
    OverlayRenderFn renderFn;
};

struct RegisteredModal {
    std::string id;
    OverlayRenderFn renderFn;
    bool isOpen = false;
};

class OverlayActionsRegistry : public refix::IOverlayActions {
public:
    static OverlayActionsRegistry& Get();

    OverlayActionsRegistry();
    virtual ~OverlayActionsRegistry() override = default;

    virtual void RegisterCard(const char* id, const char* title, const char* subtitle, OverlayRenderFn render_fn) override;
    virtual void RegisterModal(const char* id, OverlayRenderFn render_fn) override;
    virtual void OpenOverlay() override;
    virtual void CloseOverlay() override;
    virtual void ToggleOverlay() override;
    virtual void OpenModal(const char* id) override;
    virtual void CloseModal(const char* id) override;

    std::vector<RegisteredCard> GetCards();
    std::vector<RegisteredModal> GetModals();
    bool IsModalOpen(const char* id);

private:
    std::mutex m_mu;
    std::vector<RegisteredCard> m_cards;
    std::unordered_map<std::string, RegisteredModal> m_modals;
};

} // namespace refix::overlay
