#include "../src/eos/eos_types.h"
#include "../src/eos/eos_identity.h"
#include "../src/eos/eos_connect.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void CreateMockProfile(const std::string& path, const std::string& accountUuid, const std::string& name, uint64_t steamId) {
    std::ofstream ofs(path);
    ofs << "{\n";
    ofs << "  \"account_uuid\": \"" << accountUuid << "\",\n";
    ofs << "  \"display_name\": \"" << name << "\",\n";
    ofs << "  \"steam_id\": \"" << steamId << "\"\n";
    ofs << "}\n";
    ofs.close();
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Two-Machine Identity Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    fs::create_directories("build/test_profiles");

    std::string profileA = "build/test_profiles/profile_machine_a.json";
    std::string profileB = "build/test_profiles/profile_machine_b.json";

    std::string uuidA = "11111111-2222-3333-4444-555555555555";
    std::string uuidB = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

    CreateMockProfile(profileA, uuidA, "Player_Alpha", 76561198000000001ULL);
    CreateMockProfile(profileB, uuidB, "Player_Beta", 76561198000000002ULL);

    auto& idMgr = ReFixEOS::IdentityManager::Get();

    // -------------------------------------------------------------------------
    // MACHINE A: First Boot
    // -------------------------------------------------------------------------
    std::cout << "\n[MACHINE A] Booting with profile A..." << std::endl;
    idMgr.LoadFromProfilePath(profileA);
    std::string loadedUuidA = idMgr.GetLocalAccountUuid();
    EOS_ProductUserId puidA = idMgr.GetLocalProductUserId();
    std::string puidStrA = idMgr.GetLocalProductUserIdString();

    std::cout << "  Machine A AccountUUID: " << loadedUuidA << std::endl;
    std::cout << "  Machine A PUID:        " << puidStrA << std::endl;

    assert(loadedUuidA == uuidA);
    assert(EOS_ProductUserId_IsValid(puidA) == 1);
    assert(!puidStrA.empty());

    // -------------------------------------------------------------------------
    // MACHINE B: Independent Machine Boot
    // -------------------------------------------------------------------------
    std::cout << "\n[MACHINE B] Booting independent Machine B with profile B..." << std::endl;
    idMgr.LoadFromProfilePath(profileB);
    std::string loadedUuidB = idMgr.GetLocalAccountUuid();
    EOS_ProductUserId puidB = idMgr.GetLocalProductUserId();
    std::string puidStrB = idMgr.GetLocalProductUserIdString();

    std::cout << "  Machine B AccountUUID: " << loadedUuidB << std::endl;
    std::cout << "  Machine B PUID:        " << puidStrB << std::endl;

    assert(loadedUuidB == uuidB);
    assert(EOS_ProductUserId_IsValid(puidB) == 1);
    assert(!puidStrB.empty());

    // -------------------------------------------------------------------------
    // VERIFY INDEPENDENCE & UNIQUENESS
    // -------------------------------------------------------------------------
    std::cout << "\n[VERIFICATION] Machine A vs Machine B Uniqueness..." << std::endl;
    assert(loadedUuidA != loadedUuidB);
    assert(puidStrA != puidStrB);
    assert(puidA != puidB);
    std::cout << "  [PASS] AccountUUID_A != AccountUUID_B" << std::endl;
    std::cout << "  [PASS] PUID_A != PUID_B (both handle pointer and 32-char string)" << std::endl;

    // -------------------------------------------------------------------------
    // MACHINE A: Process Restart Simulation
    // -------------------------------------------------------------------------
    std::cout << "\n[RESTART SIMULATION] Restarting Machine A..." << std::endl;
    idMgr.LoadFromProfilePath(profileA);
    assert(idMgr.GetLocalAccountUuid() == uuidA);
    assert(idMgr.GetLocalProductUserIdString() == puidStrA);
    std::cout << "  [PASS] Machine A identity 100% persisted across simulated restart." << std::endl;

    // -------------------------------------------------------------------------
    // MACHINE B: Process Restart Simulation
    // -------------------------------------------------------------------------
    std::cout << "\n[RESTART SIMULATION] Restarting Machine B..." << std::endl;
    idMgr.LoadFromProfilePath(profileB);
    assert(idMgr.GetLocalAccountUuid() == uuidB);
    assert(idMgr.GetLocalProductUserIdString() == puidStrB);
    std::cout << "  [PASS] Machine B identity 100% persisted across simulated restart." << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Two-Machine Identity Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
