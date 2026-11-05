#include "mod_scheduler.h"
#include <algorithm>
#include <queue>
#include <sstream>
#include <set>

namespace refix::mods {

static const char* s_PhaseNames[] = {
    "Boot",
    "PreRefix",
    "PostRefix",
    "GameInit",
    "EngineReady",
    "NetworkReady",
    "PreFrame",
    "PostFrame",
    "LobbyJoined",
    "MatchStart",
    "Shutdown"
};

const char* ModPhaseToString(ModPhase phase) {
    size_t idx = static_cast<size_t>(phase);
    if (idx < static_cast<size_t>(ModPhase::Count)) {
        return s_PhaseNames[idx];
    }
    return "Unknown";
}

bool StringToModPhase(const std::string& str, ModPhase& out_phase) {
    for (size_t i = 0; i < static_cast<size_t>(ModPhase::Count); ++i) {
        if (_stricmp(str.c_str(), s_PhaseNames[i]) == 0) {
            out_phase = static_cast<ModPhase>(i);
            return true;
        }
    }
    return false;
}

ModScheduler::ModScheduler() {
    Clear();
}

void ModScheduler::Clear() {
    mods_.clear();
    execution_order_.clear();
    for (size_t i = 0; i < static_cast<size_t>(ModPhase::Count); ++i) {
        execution_order_[static_cast<ModPhase>(i)] = {};
    }
}

bool ModScheduler::HasMod(const std::string& mod_id) const {
    return mods_.find(mod_id) != mods_.end();
}

bool ModScheduler::RegisterMod(const ModManifest& manifest, std::string& out_error) {
    if (manifest.id.empty()) {
        out_error = "Cannot register mod with empty ID";
        return false;
    }
    if (mods_.find(manifest.id) != mods_.end()) {
        out_error = "Mod with ID '" + manifest.id + "' is already registered";
        return false;
    }

    ModPhase phase = ModPhase::EngineReady;
    if (!manifest.phase.empty() && !StringToModPhase(manifest.phase, phase)) {
        out_error = "Invalid phase '" + manifest.phase + "' in mod manifest: " + manifest.id;
        return false;
    }

    ScheduledMod sm;
    sm.manifest = manifest;
    sm.target_phase = phase;
    sm.is_enabled = true;

    mods_[manifest.id] = sm;
    return true;
}

bool ModScheduler::TopologicalSortPhase(ModPhase phase,
                                       const std::vector<std::string>& mod_ids,
                                       std::vector<std::string>& out_sorted,
                                       std::string& out_error) {
    out_sorted.clear();
    if (mod_ids.empty()) return true;

    std::set<std::string> id_set(mod_ids.begin(), mod_ids.end());
    std::map<std::string, std::set<std::string>> adj;
    std::map<std::string, int> in_degree;

    for (const auto& id : mod_ids) {
        adj[id] = {};
        in_degree[id] = 0;
    }

    for (const auto& id : mod_ids) {
        const auto& m = mods_[id].manifest;

        // before: this mod must execute BEFORE target
        for (const auto& before_id : m.dependencies.before) {
            if (id_set.find(before_id) != id_set.end()) {
                if (adj[id].insert(before_id).second) {
                    in_degree[before_id]++;
                }
            }
        }

        // after: this mod must execute AFTER target
        for (const auto& after_id : m.dependencies.after) {
            if (id_set.find(after_id) != id_set.end()) {
                if (adj[after_id].insert(id).second) {
                    in_degree[id]++;
                }
            }
        }
    }

    // Custom comparator for deterministic tie-breaking:
    // 1. Lower priority executes earlier.
    // 2. Smaller lexicographical ID executes earlier.
    auto comp = [this](const std::string& a, const std::string& b) {
        int pa = this->mods_[a].manifest.priority;
        int pb = this->mods_[b].manifest.priority;
        if (pa != pb) return pa > pb; // Priority queue is max-heap by default; > puts lower priority on top
        return a > b;
    };

    std::priority_queue<std::string, std::vector<std::string>, decltype(comp)> pq(comp);

    for (const auto& id : mod_ids) {
        if (in_degree[id] == 0) {
            pq.push(id);
        }
    }

    while (!pq.empty()) {
        std::string curr = pq.top();
        pq.pop();
        out_sorted.push_back(curr);

        for (const auto& neighbor : adj[curr]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                pq.push(neighbor);
            }
        }
    }

    if (out_sorted.size() != mod_ids.size()) {
        std::ostringstream ss;
        ss << "Cyclic dependency detected in phase " << ModPhaseToString(phase) << " among mods: ";
        bool first = true;
        for (const auto& id : mod_ids) {
            if (in_degree[id] > 0) {
                if (!first) ss << ", ";
                ss << id;
                first = false;
            }
        }
        out_error = ss.str();
        return false;
    }

    return true;
}

bool ModScheduler::Resolve(std::string& out_error) {
    for (size_t i = 0; i < static_cast<size_t>(ModPhase::Count); ++i) {
        execution_order_[static_cast<ModPhase>(i)].clear();
    }

    // 1. Check requirements and conflicts
    for (const auto& [id, sm] : mods_) {
        // Requirements
        for (const auto& req : sm.manifest.dependencies.requires) {
            if (mods_.find(req) == mods_.end()) {
                out_error = "Mod '" + id + "' requires missing mod: '" + req + "'";
                return false;
            }
        }

        // Conflicts
        for (const auto& conf : sm.manifest.dependencies.conflicts) {
            if (mods_.find(conf) != mods_.end()) {
                out_error = "Mod conflict: '" + id + "' conflicts with '" + conf + "'";
                return false;
            }
        }
    }

    // 2. Group mods by phase
    std::map<ModPhase, std::vector<std::string>> phase_mods;
    for (const auto& [id, sm] : mods_) {
        if (sm.is_enabled) {
            phase_mods[sm.target_phase].push_back(id);
        }
    }

    // 3. Perform topological sort for each phase
    for (size_t i = 0; i < static_cast<size_t>(ModPhase::Count); ++i) {
        ModPhase phase = static_cast<ModPhase>(i);
        std::vector<std::string> sorted;
        if (!TopologicalSortPhase(phase, phase_mods[phase], sorted, out_error)) {
            return false;
        }
        execution_order_[phase] = sorted;
    }

    return true;
}

const std::vector<std::string>& ModScheduler::GetExecutionOrder(ModPhase phase) const {
    auto it = execution_order_.find(phase);
    if (it != execution_order_.end()) {
        return it->second;
    }
    static const std::vector<std::string> s_empty;
    return s_empty;
}

void ModScheduler::DispatchPhase(ModPhase phase, const std::function<void(const std::string& mod_id)>& callback) {
    const auto& order = GetExecutionOrder(phase);
    for (const auto& mod_id : order) {
        callback(mod_id);
    }
}

} // namespace refix::mods
