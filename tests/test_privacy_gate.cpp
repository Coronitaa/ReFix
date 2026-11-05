#include "../src/mods/privacy_gate.h"
#include <iostream>
#include <cassert>

using namespace refix::mods;

int main() {
    std::cout << "Running test_privacy_gate..." << std::endl;

    PrivacyGate gate;
    gate.InitializeSession();

    // Register a remote player
    std::string real_steam_id = "76561198012345678";
    std::string ip = "192.168.1.50";
    uint16_t port = 27015;
    std::string name = "TargetGamer";
    gate.RegisterPeer(real_steam_id, ip, port, name);

    // 1. Mod A and Mod B must receive different PeerHandle for the same player
    PeerHandle h_modA = gate.GetPeerHandleForMod("com.refix.modA", real_steam_id);
    PeerHandle h_modB = gate.GetPeerHandleForMod("com.refix.modB", real_steam_id);

    assert(h_modA != 0);
    assert(h_modB != 0);
    assert(h_modA != h_modB); // Must be strictly uncorrelated!
    std::cout << "[PASS] Cross-mod uncorrelated PeerHandles (ModA: 0x" << std::hex << h_modA << ", ModB: 0x" << h_modB << ")" << std::dec << std::endl;

    // 2. Cross-session uncorrelated PeerHandles for the same mod
    PrivacyGate gate2;
    gate2.InitializeSession();
    gate2.RegisterPeer(real_steam_id, ip, port, name);
    PeerHandle h_modA_session2 = gate2.GetPeerHandleForMod("com.refix.modA", real_steam_id);
    assert(h_modA != h_modA_session2); // Cross-session correlation prevented!
    std::cout << "[PASS] Cross-session uncorrelated PeerHandles (Session1: 0x" << std::hex << h_modA << ", Session2: 0x" << h_modA_session2 << ")" << std::dec << std::endl;

    // 3. Display Name Privacy Filtering
    std::set<std::string> caps_unprivileged = { Capability::Log };
    std::string display_name;
    bool ok = gate.GetPeerDisplayName("com.refix.modA", h_modA, caps_unprivileged, display_name);
    assert(ok);
    assert(display_name.find("Player_") == 0);
    assert(display_name.find("TargetGamer") == std::string::npos); // Real name NOT leaked!
    std::cout << "[PASS] Display name redacted to anonymous alias without capability: " << display_name << std::endl;

    std::set<std::string> caps_approved = { Capability::PlayersDisplayName };
    ok = gate.GetPeerDisplayName("com.refix.modA", h_modA, caps_approved, display_name);
    assert(ok);
    assert(display_name == "TargetGamer"); // Real name allowed only with approval
    std::cout << "[PASS] Display name exposed when capability explicitly approved" << std::endl;

    // 4. Channel Isolation & Access Control
    gate.RegisterModChannel("com.refix.modA", 1); // Channel 1 declared
    // Channel 1 without capability -> blocked
    assert(!gate.CanModInspectChannel("com.refix.modA", 1, caps_unprivileged));

    // Channel 1 with NetInspectChannel -> allowed
    std::set<std::string> caps_inspect = { Capability::NetInspectChannel };
    assert(gate.CanModInspectChannel("com.refix.modA", 1, caps_inspect));

    // Channel 2 (undeclared) -> blocked even with capability
    assert(!gate.CanModInspectChannel("com.refix.modA", 2, caps_inspect));

    // Channel 1 with NetOverrideChannel -> allowed
    std::set<std::string> caps_override = { Capability::NetOverrideChannel };
    assert(gate.CanModOverrideChannel("com.refix.modA", 1, caps_override));

    std::cout << "[PASS] Network channel isolation and capability filtering functional" << std::endl;

    std::cout << "All test_privacy_gate tests passed!" << std::endl;
    return 0;
}
