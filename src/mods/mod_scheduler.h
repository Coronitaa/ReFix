#pragma once

#include "mod_types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace refix::mods {

// The 11 lifecycle phases in exact sequential order
enum class ModPhase {
    Boot = 0,
    PreRefix,
    PostRefix,
    GameInit,
    EngineReady,
    NetworkReady,
    PreFrame,
    PostFrame,
    LobbyJoined,
    MatchStart,
    Shutdown,
    Count
};

const char* ModPhaseToString(ModPhase phase);
bool StringToModPhase(const std::string& str, ModPhase& out_phase);

struct ScheduledMod {
    ModManifest manifest;
    ModPhase target_phase = ModPhase::EngineReady;
    bool is_enabled = true;
};

class ModScheduler {
public:
    ModScheduler();

    // Register a mod for scheduling
    bool RegisterMod(const ModManifest& manifest, std::string& out_error);

    // Resolve dependencies, conflicts, and topological sort for all phases.
    // Returns false if there are missing requirements, conflicts, or cyclic dependencies.
    bool Resolve(std::string& out_error);

    // Get the sorted list of mod IDs for a specific phase
    const std::vector<std::string>& GetExecutionOrder(ModPhase phase) const;

    // Execute callback on all mods scheduled for a given phase in order
    void DispatchPhase(ModPhase phase, const std::function<void(const std::string& mod_id)>& callback);

    // Clear all registrations
    void Clear();

    // Check if mod is registered
    bool HasMod(const std::string& mod_id) const;

private:
    std::map<std::string, ScheduledMod> mods_;
    std::map<ModPhase, std::vector<std::string>> execution_order_;

    bool TopologicalSortPhase(ModPhase phase,
                              const std::vector<std::string>& mod_ids_in_phase,
                              std::vector<std::string>& out_sorted,
                              std::string& out_error);
};

} // namespace refix::mods
