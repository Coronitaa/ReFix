#pragma once
#include "../imgui/imgui.h"

namespace refix::overlay {
    void DrawModsCard(ImVec2 pos, float width, float height);
    void DrawModsApprovalModal();
    void DrawMarketGiveModal();
    void DrawHostMatchModal();
    void OpenMarketGiveMenu();
    void OpenHostMatchModal();
}
