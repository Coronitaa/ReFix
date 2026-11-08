#include "../src/mods/mod_scheduler.h"
#include <iostream>
#include <cassert>

using namespace refix::mods;

int main() {
    std::cout << "Running test_mod_scheduler..." << std::endl;

    std::string err;

    // Test 1: Priority ordering without explicit edges
    {
        ModScheduler sched;
        ModManifest m1;
        m1.id = "mod_p0";
        m1.phase = "EngineReady";
        m1.priority = 0;

        ModManifest m2;
        m2.id = "mod_pneg10";
        m2.phase = "EngineReady";
        m2.priority = -10;

        ModManifest m3;
        m3.id = "mod_p10";
        m3.phase = "EngineReady";
        m3.priority = 10;

        assert(sched.RegisterMod(m1, err));
        assert(sched.RegisterMod(m2, err));
        assert(sched.RegisterMod(m3, err));

        assert(sched.Resolve(err));
        const auto& order = sched.GetExecutionOrder(ModPhase::EngineReady);
        assert(order.size() == 3);
        assert(order[0] == "mod_pneg10");
        assert(order[1] == "mod_p0");
        assert(order[2] == "mod_p10");
        std::cout << "[PASS] Priority-based execution order" << std::endl;
    }

    // Test 2: Explicit before/after edges
    {
        ModScheduler sched;
        ModManifest a;
        a.id = "mod_a";
        a.phase = "EngineReady";
        a.priority = 50; // High priority number, but must run before B
        a.dependencies.before = { "mod_b" };

        ModManifest b;
        b.id = "mod_b";
        b.phase = "EngineReady";
        b.priority = -50; // Normally would run first, but constrained by A

        assert(sched.RegisterMod(a, err));
        assert(sched.RegisterMod(b, err));
        assert(sched.Resolve(err));

        const auto& order = sched.GetExecutionOrder(ModPhase::EngineReady);
        assert(order.size() == 2);
        assert(order[0] == "mod_a");
        assert(order[1] == "mod_b");
        std::cout << "[PASS] Explicit before/after topological sorting" << std::endl;
    }

    // Test 3: Cyclic dependency detection
    {
        ModScheduler sched;
        ModManifest a;
        a.id = "cycle_a";
        a.phase = "PreRefix";
        a.dependencies.before = { "cycle_b" };

        ModManifest b;
        b.id = "cycle_b";
        b.phase = "PreRefix";
        b.dependencies.before = { "cycle_a" };

        assert(sched.RegisterMod(a, err));
        assert(sched.RegisterMod(b, err));

        bool ok = sched.Resolve(err);
        assert(!ok);
        assert(err.find("Cyclic dependency") != std::string::npos);
        std::cout << "[PASS] Cyclic dependency detected: " << err << std::endl;
    }

    // Test 4: Missing requirement detection
    {
        ModScheduler sched;
        ModManifest a;
        a.id = "req_user";
        a.phase = "Boot";
        a.dependencies.requires = { "missing_base_mod" };

        assert(sched.RegisterMod(a, err));
        bool ok = sched.Resolve(err);
        assert(!ok);
        assert(err.find("requires missing mod") != std::string::npos);
        std::cout << "[PASS] Missing requirement detected: " << err << std::endl;
    }

    // Test 5: Conflict detection
    {
        ModScheduler sched;
        ModManifest a;
        a.id = "mod_patch1";
        a.phase = "GameInit";
        a.dependencies.conflicts = { "mod_patch2" };

        ModManifest b;
        b.id = "mod_patch2";
        b.phase = "GameInit";

        assert(sched.RegisterMod(a, err));
        assert(sched.RegisterMod(b, err));
        bool ok = sched.Resolve(err);
        assert(!ok);
        assert(err.find("conflicts with") != std::string::npos);
        std::cout << "[PASS] Conflict detected: " << err << std::endl;
    }

    std::cout << "All test_mod_scheduler tests passed!" << std::endl;
    return 0;
}
