#include "../src/overlay/profile_store.h"
#include <cassert>
#include <cstdio>
#include <vector>

void ReFixLog(const char* fmt, ...) {}

using namespace refix::overlay;
using namespace refix::overlay::profiles;

int main() {
    printf("[*] Running ReFix LAN Profiles Unit Tests...\n");

    // 1. UsedIn behavior
    Profile p1;
    p1.id = L"00000001";
    p1.name = L"Player1";
    p1.steamId = 76561198000000001ULL;
    p1.originGame = L"game_a_exe";
    p1.games = { { L"game_a_exe", L"Game A" } };

    assert(UsedIn(p1, L"game_a_exe") == true);
    assert(UsedIn(p1, L"game_b_exe") == false);
    assert(UsedIn(p1, L"") == false);
    printf("  [PASS] Profile UsedIn scoped to its origin game\n");

    // Legacy profile with no originGame and no games
    Profile p2;
    p2.id = L"00000002";
    p2.name = L"LegacyPlayer";
    p2.steamId = 76561198000000002ULL;
    p2.originGame = L"";
    p2.games = {};

    assert(UsedIn(p2, L"game_a_exe") == false);
    assert(UsedIn(p2, L"game_b_exe") == false);
    printf("  [PASS] Legacy profile with no games does not belong to all games\n");

    // Profile used in game A, then imported into game B
    Profile p3 = p1;
    p3.id = L"00000003";
    bool added = AddGame(p3, L"game_b_exe", L"Game B");
    assert(added == true);
    assert(UsedIn(p3, L"game_a_exe") == true);
    assert(UsedIn(p3, L"game_b_exe") == true);
    assert(UsedIn(p3, L"game_c_exe") == false);
    printf("  [PASS] Imported profile is recognized in imported game and keeps isolation from other games\n");

    // 2. ForGame and FromOtherGames
    std::vector<Profile> all = { p1, p2, p3 };

    // For game_a: p1 and p3 belong to game_a; p2 does not
    auto forA = ForGame(all, L"game_a_exe");
    assert(forA.size() == 2);
    assert(forA[0].id == L"00000001" || forA[1].id == L"00000001");
    assert(forA[0].id == L"00000003" || forA[1].id == L"00000003");

    auto otherA = FromOtherGames(all, L"game_a_exe");
    assert(otherA.size() == 1);
    assert(otherA[0].id == L"00000002");
    printf("  [PASS] ForGame and FromOtherGames properly filter profiles for existing game\n");

    // For a brand new game: NONE belong to it; all are available to import
    auto forNew = ForGame(all, L"brand_new_game_exe");
    assert(forNew.empty());

    auto otherNew = FromOtherGames(all, L"brand_new_game_exe");
    assert(otherNew.size() == 3);
    printf("  [PASS] Brand new game has 0 active profiles and all existing profiles available in import grid\n");

    // 3. LastProfileId per game isolation
    SetLastProfileId(L"00000001", L"game_a_exe");
    SetLastProfileId(L"00000003", L"game_b_exe");

    assert(LastProfileId(L"game_a_exe") == L"00000001");
    assert(LastProfileId(L"game_b_exe") == L"00000003");
    assert(LastProfileId(L"brand_new_game_exe") == L"");
    printf("  [PASS] LastProfileId is isolated per game and empty for new games\n");

    printf("[OK] All ReFix LAN profile isolation tests passed successfully!\n");
    return 0;
}
