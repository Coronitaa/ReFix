#include "../src/mods/mod_manifest.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace refix::mods;

int main() {
    printf("[*] Running ReFix Mod Manifest Unit Tests (TDD)...\n");

    // 1. Valid Manifest
    const char* valid_json = R"({
        "schema_version": 1,
        "id": "community.test.rebalance",
        "name": "Test Rebalancer",
        "version": "1.0.0",
        "author": "Alice",
        "author_pubkey": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        "entrypoint": "mod.wasm",
        "execution_tier": "sandboxed",
        "phase": "EngineReady",
        "priority": 50,
        "target_games": [
            { "app_id": 1966720, "exe_name": "Lethal Company.exe", "engine": "unity_il2cpp" }
        ],
        "dependencies": {
            "before": [],
            "after": ["refix.steam.lobbies"],
            "requires": [],
            "conflicts": []
        },
        "capabilities": ["log", "config", "patch.declarative"],
        "resource_limits": {
            "max_memory_mb": 16,
            "watchdog_timeout_ms": 15,
            "max_hooks": 8,
            "storage_quota_kb": 512
        },
        "files": {
            "mod.wasm": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        }
    })";

    ModManifest manifest;
    std::string err;
    bool ok = ParseManifest(valid_json, manifest, err);
    assert(ok && "Valid manifest must parse cleanly");
    assert(manifest.id == "community.test.rebalance");
    assert(manifest.execution_tier == ExecutionTier::Sandboxed);
    assert(manifest.capabilities.count(Capability::Log) == 1);
    assert(manifest.capabilities.count(Capability::PatchDeclarative) == 1);
    assert(manifest.files.size() == 1);
    assert(ValidateManifest(manifest, err));
    printf("  [PASS] Valid manifest parsed and validated\n");

    // 2. Canonical JSON Serialization Determinism
    std::string canon1 = ToCanonicalJson(manifest);
    ModManifest manifest2;
    ok = ParseManifest(canon1, manifest2, err);
    assert(ok);
    std::string canon2 = ToCanonicalJson(manifest2);
    assert(canon1 == canon2 && "Canonical JSON output must be 100% deterministic");
    printf("  [PASS] Canonical JSON serialization is strictly deterministic\n");

    // 3. Schema Version Rejection
    const char* bad_schema = R"({ "schema_version": 2, "id": "test.v2" })";
    ModManifest bad_m;
    ok = ParseManifest(bad_schema, bad_m, err);
    assert(!ok && "Unsupported schema_version must be rejected");
    printf("  [PASS] Unsupported schema_version rejected\n");

    // 4. Invalid Mod ID Format (e.g. spaces or path traversal in id)
    ModManifest test_m = manifest;
    test_m.id = "bad id with spaces";
    assert(!ValidateManifest(test_m, err) && "ID with spaces must fail validation");
    test_m.id = "../traversal";
    assert(!ValidateManifest(test_m, err) && "ID with path traversal must fail validation");
    test_m.id = "community.valid_id-123";
    assert(ValidateManifest(test_m, err));
    printf("  [PASS] Strict mod ID syntax validation\n");

    // 5. Unknown Capabilities Rejection
    test_m.capabilities.insert("system.exec_command");
    assert(!ValidateManifest(test_m, err) && "Unknown capability must fail validation");
    test_m.capabilities.erase("system.exec_command");
    assert(ValidateManifest(test_m, err));
    printf("  [PASS] Unknown capabilities strictly rejected\n");

    // 6. High Risk Capabilities Recognition
    assert(Capability::IsHighRisk(Capability::MemoryWrite) == true);
    assert(Capability::IsHighRisk(Capability::PlayersDisplayName) == true);
    assert(Capability::IsHighRisk(Capability::Log) == false);
    printf("  [PASS] High-risk capabilities flagged properly for user approval gate\n");

    // 7. Zip-Slip Path Traversal Protection
    assert(IsSafeRelativePath("mod.wasm") == true);
    assert(IsSafeRelativePath("assets/config.json") == true);
    assert(IsSafeRelativePath("data/sub/item.png") == true);
    assert(IsSafeRelativePath("../escape.dll") == false);
    assert(IsSafeRelativePath("a/../../b") == false);
    assert(IsSafeRelativePath("/root/file.txt") == false);
    assert(IsSafeRelativePath("\\windows\\system32\\calc.exe") == false);
    assert(IsSafeRelativePath("C:\\Windows\\cmd.exe") == false);
    assert(IsSafeRelativePath("D:game.exe") == false);
    assert(IsSafeRelativePath("") == false);
    printf("  [PASS] Zip-Slip path traversal attacks strictly blocked\n");

    // 8. Malformed JSON syntax
    ModManifest malformed_m;
    ok = ParseManifest("{{not json", malformed_m, err);
    assert(!ok && "Malformed JSON must fail gracefully without crash");
    printf("  [PASS] Malformed JSON handled gracefully\n");

    printf("[SUCCESS] All ReFix Mod Manifest unit tests passed!\n");
    return 0;
}
