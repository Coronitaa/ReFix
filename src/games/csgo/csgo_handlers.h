#pragma once

#include "../../include/refix_services.h"

namespace refix::csgo {

void RegisterCSGOHandlers(refix::IRefixServices* services);
void OnModPhaseEvent(int event_id);
void ShutdownCSGOHandlers();
void ApplyLanHostConfig(const char* jsonStr);
void EnforceActiveLanHostConfig();

} // namespace refix::csgo
