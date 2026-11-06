#pragma once
#include "../imgui/imgui.h"

namespace refix::overlay {
    void DrawModsCard(ImVec2 pos, float width, float height);
    void DrawModsApprovalModal();
    void DrawMarketGiveModal();
    void DrawMatchmakingToastWidget();
    void OpenMarketGiveMenu();
    void OpenHostMatchModal();
    void OpenMatchmakingToast();
    void ExecuteEngineClientCommand(const char* cmd);
}
