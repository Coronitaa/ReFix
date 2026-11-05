#pragma once
#include "../imgui/imgui.h"

namespace refix::overlay {
#if defined(_M_X64) || defined(__x86_64__)
    void DrawModsCard(ImVec2 pos, float width, float height);
    void DrawModsApprovalModal();
#else
    inline void DrawModsCard(ImVec2, float, float) {}
    inline void DrawModsApprovalModal() {}
#endif
}
