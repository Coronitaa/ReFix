#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace refix::mods {

enum class ExecutionTier {
    Sandboxed,  // WebAssembly runtime (WAMR / wasm3). ONLY tier for community mods.
    Native      // Native PE DLL. ONLY permitted if signed by ReFix Official Root/Release key.
};

enum class TrustLevel {
    Revoked,              // Blacklisted in revocation list. Never runs.
    Untrusted,            // Unsigned or signature check failed. Never runs (except DevMode).
    UnverifiedCommunity,  // Self-signed author key. Requires explicit manual approval (TOFU).
    VerifiedCommunity,    // Author key certified by ReFix Developer CA. Prompts on new caps.
    Official              // Signed directly by ReFix Official Release key. Automatic trust.
};

// Capabilities declared in manifest and approved by user
namespace Capability {
    constexpr const char* Log                = "log";
    constexpr const char* Config             = "config";
    constexpr const char* Storage            = "storage";
    constexpr const char* Events             = "events";
    constexpr const char* OverlayPanel       = "overlay.panel";
    constexpr const char* PatchDeclarative   = "patch.declarative";
    constexpr const char* HooksGame          = "hooks.game";
    constexpr const char* MemoryWrite        = "memory.write";           // High Risk!
    constexpr const char* NetInspectChannel  = "net.inspect_channel";
    constexpr const char* NetOverrideChannel = "net.override_channel";
    constexpr const char* PlayersDisplayName = "players.display_name";    // High Risk!

    inline bool IsHighRisk(const std::string& cap) {
        return cap == MemoryWrite || cap == PlayersDisplayName;
    }

    inline bool IsValidCapability(const std::string& cap) {
        return cap == Log || cap == Config || cap == Storage || cap == Events ||
               cap == OverlayPanel || cap == PatchDeclarative || cap == HooksGame ||
               cap == MemoryWrite || cap == NetInspectChannel || cap == NetOverrideChannel ||
               cap == PlayersDisplayName;
    }
}

struct ResourceLimits {
    uint32_t max_memory_mb = 16;
    uint32_t watchdog_timeout_ms = 15;
    uint32_t max_hooks = 16;
    uint32_t storage_quota_kb = 1024;
};

struct TargetGame {
    uint32_t app_id = 0;
    std::string exe_name;
    std::string engine;
};

struct ModDependencies {
    std::vector<std::string> before;
    std::vector<std::string> after;
    std::vector<std::string> requires;
    std::vector<std::string> conflicts;
};

struct ModManifest {
    uint32_t schema_version = 1;
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string author_pubkey;        // Hex-encoded 32-byte Ed25519 public key
    std::string developer_cert;       // Optional: Ed25519 signature by ReFix CA over author_pubkey
    std::vector<TargetGame> target_games;
    std::string entrypoint;
    ExecutionTier execution_tier = ExecutionTier::Sandboxed;
    std::string phase = "EngineReady";
    int32_t priority = 0;
    ModDependencies dependencies;
    std::set<std::string> capabilities;
    ResourceLimits resource_limits;
    std::map<std::string, std::string> files; // relative_path -> sha256_hex
};

} // namespace refix::mods
