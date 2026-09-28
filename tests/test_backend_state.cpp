#include "../src/refix_online/refix_backend_state.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <atomic>

using namespace ReFixOnline;

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix Online v2 - Authoritative Backend State Unit Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& server = BackendServerState::Get();
    server.Reset();

    // -------------------------------------------------------------------------
    // TEST 1: Authentication & Token Generation
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] Session Authentication & Validation..." << std::endl;
    std::string userA = "11111111222233334444555555555555";
    std::string tokenA;
    EBackendResult res = server.AuthenticateSession(userA, "Player_A", tokenA);
    assert(res == SUCCESS);
    assert(!tokenA.empty());
    assert(server.ValidateSession(userA, tokenA));
    assert(!server.ValidateSession(userA, "bad_token"));
    std::cout << "  User A authenticated with session token: " << tokenA.substr(0, 16) << "..." << std::endl;

    // -------------------------------------------------------------------------
    // TEST 2: Lobby Creation & Non-Hardcoded MaxMembers
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] Lobby Creation with Custom MaxMembers (6 slots)..." << std::endl;
    std::unordered_map<std::string, std::string> attrsA = {
        { "room_name", "Alpha Room" },
        { "game_filter", "refix_mechachameleon" }
    };
    LobbyData lobbyA;
    res = server.CreateLobby(userA, 6, attrsA, lobbyA);
    assert(res == SUCCESS);
    assert(!lobbyA.lobbyId.empty());
    assert(lobbyA.ownerUserId == userA);
    assert(lobbyA.maxMembers == 6); // Not hardcoded to 4
    assert(lobbyA.members.size() == 1);
    assert(lobbyA.members[0].isOwner == true);
    std::cout << "  Lobby created with ID: " << lobbyA.lobbyId << ", MaxMembers=" << lobbyA.maxMembers << std::endl;

    // -------------------------------------------------------------------------
    // TEST 3: Duplicate Join Rejection
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] Duplicate Join Rejection..." << std::endl;
    LobbyData dupLob;
    res = server.JoinLobby(userA, "Player_A", lobbyA.lobbyId, dupLob);
    assert(res == ALREADY_MEMBER);
    std::cout << "  Duplicate join safely returned ALREADY_MEMBER." << std::endl;

    // -------------------------------------------------------------------------
    // TEST 4: Capacity Enforcement (Lobby Full)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] Lobby Capacity Enforcement..." << std::endl;
    // Fill up remaining 5 slots
    for (int i = 1; i <= 5; ++i) {
        std::string uid = "peer_user_" + std::to_string(i);
        std::string name = "Peer_" + std::to_string(i);
        std::string tok;
        server.AuthenticateSession(uid, name, tok);
        LobbyData joinedLob;
        res = server.JoinLobby(uid, name, lobbyA.lobbyId, joinedLob);
        assert(res == SUCCESS);
    }
    // Attempt 7th member (capacity = 6)
    std::string userOverflow = "peer_user_overflow";
    std::string tokOver;
    server.AuthenticateSession(userOverflow, "Overflow", tokOver);
    LobbyData overflowLob;
    res = server.JoinLobby(userOverflow, "Overflow", lobbyA.lobbyId, overflowLob);
    assert(res == LOBBY_FULL);
    std::cout << "  Capacity limit (6/6) strictly enforced -> LOBBY_FULL!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 5: Member Leave & Ownership Transfer
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 5] Member Leave & Owner Transfer to Oldest Member..." << std::endl;
    std::string newOwnerId;
    res = server.LeaveLobby(userA, lobbyA.lobbyId, newOwnerId);
    assert(res == SUCCESS);
    assert(newOwnerId == "peer_user_1"); // Transferred to oldest peer
    std::cout << "  Owner left -> Ownership transferred to: " << newOwnerId << std::endl;

    // -------------------------------------------------------------------------
    // TEST 6: Lobby Auto-Destruction on Last Member Leave
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 6] Empty Lobby Auto-Destruction..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string uid = "peer_user_" + std::to_string(i);
        std::string dummy;
        server.LeaveLobby(uid, lobbyA.lobbyId, dummy);
    }
    LobbyData nonExistent;
    res = server.ResyncLobby("peer_user_1", lobbyA.lobbyId, nonExistent);
    assert(res == LOBBY_NOT_FOUND);
    std::cout << "  Lobby automatically destroyed upon 0 remaining members." << std::endl;

    // -------------------------------------------------------------------------
    // TEST 7: GOLDEN SCENARIO SIMULATION (Machine A & Machine B)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 7] GOLDEN SCENARIO SIMULATION..." << std::endl;
    server.Reset();

    std::string puidA = "2b8db5d0c5cbd56ff14dff84a61cd9a2";
    std::string puidB = "6e397b6ad903cce2a3bd43df18810fd9";

    std::string tokA, tokB;
    assert(server.AuthenticateSession(puidA, "Player_Alpha", tokA) == SUCCESS);
    assert(server.AuthenticateSession(puidB, "Player_Beta", tokB) == SUCCESS);

    LobbyData goldLob;
    std::unordered_map<std::string, std::string> goldAttrs = { { "game", "chameleon" } };
    assert(server.CreateLobby(puidA, 4, goldAttrs, goldLob) == SUCCESS);

    std::vector<LobbyData> searchRes;
    assert(server.FindLobbies(puidB, 10, { { "game", "chameleon" } }, searchRes) == SUCCESS);
    assert(searchRes.size() == 1);
    assert(searchRes[0].lobbyId == goldLob.lobbyId);

    LobbyData bJoinedLob;
    assert(server.JoinLobby(puidB, "Player_Beta", goldLob.lobbyId, bJoinedLob) == SUCCESS);
    assert(bJoinedLob.members.size() == 2);
    assert(bJoinedLob.members[0].userId == puidA && bJoinedLob.members[0].isOwner);
    assert(bJoinedLob.members[1].userId == puidB && !bJoinedLob.members[1].isOwner);

    std::string bLeftNewOwner;
    assert(server.LeaveLobby(puidB, goldLob.lobbyId, bLeftNewOwner) == SUCCESS);

    LobbyData aFinalLob;
    assert(server.ResyncLobby(puidA, goldLob.lobbyId, aFinalLob) == SUCCESS);
    assert(aFinalLob.members.size() == 1);
    assert(aFinalLob.members[0].userId == puidA);
    std::cout << "  Golden scenario simulation completed with 100% match!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 8: 100+ Concurrent Simulated Users
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 8] 100+ Concurrent Simulated Users Stress Test..." << std::endl;
    constexpr int NUM_USERS = 100;
    std::vector<std::thread> userThreads;
    std::atomic<int> successCount = 0;

    for (int i = 0; i < NUM_USERS; ++i) {
        userThreads.emplace_back([&server, i, &successCount]() {
            std::string u = "stress_user_" + std::to_string(i);
            std::string tok;
            if (server.AuthenticateSession(u, "Stress_" + std::to_string(i), tok) == SUCCESS) {
                if (i % 4 == 0) {
                    LobbyData l;
                    if (server.CreateLobby(u, 4, { {"type", "stress"} }, l) == SUCCESS) {
                        successCount++;
                    }
                } else {
                    successCount++;
                }
            }
        });
    }
    for (auto& th : userThreads) th.join();
    assert(successCount == NUM_USERS);
    std::cout << "  100 concurrent clients processed without deadlocks or races!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Authoritative Backend State Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
