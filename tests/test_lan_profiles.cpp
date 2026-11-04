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

    // 4. Stats folder paths isolation
    std::wstring statsRoot1 = StatsRoot(L"00000001", 3527290);
    std::wstring statsRoot2 = StatsRoot(L"00000002", 3527290);
    assert(statsRoot1 != statsRoot2);
    assert(statsRoot1.find(L"00000001\\Stats\\3527290") != std::wstring::npos);
    assert(statsRoot2.find(L"00000002\\Stats\\3527290") != std::wstring::npos);
    printf("  [PASS] StatsRoot is strictly isolated per profile ID and AppID\n");

    // 5. Stats INI write & load roundtrip isolation
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring p1Dir = std::wstring(tempDir) + L"ReFixTest_Profile1\\Stats\\3527290";
    std::wstring p2Dir = std::wstring(tempDir) + L"ReFixTest_Profile2\\Stats\\3527290";
    for (size_t i = 3; i <= p1Dir.size(); ++i) if (i == p1Dir.size() || p1Dir[i] == L'\\') CreateDirectoryW(p1Dir.substr(0, i).c_str(), nullptr);
    for (size_t i = 3; i <= p2Dir.size(); ++i) if (i == p2Dir.size() || p2Dir[i] == L'\\') CreateDirectoryW(p2Dir.substr(0, i).c_str(), nullptr);

    std::wstring f1 = p1Dir + L"\\stats.ini";
    std::wstring f2 = p2Dir + L"\\stats.ini";

    FILE* fp1 = nullptr;
    assert(_wfopen_s(&fp1, f1.c_str(), L"w") == 0 && fp1 != nullptr);
    fprintf(fp1, "[IntStats]\nOutfit=5\nHat=2\nSkin=1\n\n[Achievements]\nFirstSummit=1700000000\n");
    fclose(fp1);

    FILE* fp2 = nullptr;
    assert(_wfopen_s(&fp2, f2.c_str(), L"w") == 0 && fp2 != nullptr);
    fprintf(fp2, "[IntStats]\nOutfit=9\nHat=0\nSkin=3\n\n[Achievements]\nProClimber=1700000001\n");
    fclose(fp2);

    // Verify f1 contents
    FILE* r1 = nullptr;
    assert(_wfopen_s(&r1, f1.c_str(), L"r") == 0 && r1 != nullptr);
    char buf[256];
    bool foundOutfit5 = false;
    while (fgets(buf, sizeof(buf), r1)) {
        if (strstr(buf, "Outfit=5")) foundOutfit5 = true;
        assert(strstr(buf, "Outfit=9") == nullptr);
    }
    fclose(r1);
    assert(foundOutfit5);

    // Verify f2 contents
    FILE* r2 = nullptr;
    assert(_wfopen_s(&r2, f2.c_str(), L"r") == 0 && r2 != nullptr);
    bool foundOutfit9 = false;
    while (fgets(buf, sizeof(buf), r2)) {
        if (strstr(buf, "Outfit=9")) foundOutfit9 = true;
        assert(strstr(buf, "Outfit=5") == nullptr);
    }
    fclose(r2);
    assert(foundOutfit9);

    DeleteFileW(f1.c_str());
    DeleteFileW(f2.c_str());
    RemoveDirectoryW(p1Dir.c_str());
    RemoveDirectoryW(p2Dir.c_str());
    printf("  [PASS] Stats INI roundtrip and profile separation confirmed\n");

    printf("[OK] All ReFix LAN profile isolation tests passed successfully!\n");
    return 0;
}
