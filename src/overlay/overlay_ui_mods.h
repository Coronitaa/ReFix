#pragma once
#include "../imgui/imgui.h"

namespace refix::overlay {
    float GetModsCardHeight(float width);
    void DrawModsCard(ImVec2 pos, float width, float height = 0.0f);
    void DrawModsApprovalModal();
    void DrawMarketGiveModal();
    void DrawMatchmakingToastWidget();
    void OpenMarketGiveMenu();
    void SetHostMatchMode(const char* modeId);
    void OpenHostMatchModal(const char* modeId = nullptr);
    void OpenMatchmakingToast();
    void ExecuteEngineClientCommand(const char* cmd);
}
