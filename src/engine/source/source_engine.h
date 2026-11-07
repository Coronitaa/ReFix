// Source Engine Adapter: Safe main-thread execution queue (R5) and interface retrieval
// Generic across all Valve Source Engine titles (CS:GO, CS:S, TF2, L4D, L4D2, GMod)
#pragma once

#include "../../include/refix_services.h"
#include <windows.h>

namespace refix::source {

class SourceEngine : public refix::ISourceEngine {
public:
    static SourceEngine& Get();

    SourceEngine();
    virtual ~SourceEngine() override = default;

    virtual void EnqueueCommand(const char* command) override;
    virtual void* FindInterface(const char* module_name, const char* interface_name) override;
};

} // namespace refix::source
