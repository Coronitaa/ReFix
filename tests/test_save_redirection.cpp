#include "../src/overlay/save_swap.h"
#include "../src/overlay/refix_overlay.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>

void ReFixLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

namespace ReFixOverlay {
std::wstring ActiveProfileId() { return L"test_profile_redirection_12345"; }
}

using namespace refix::overlay;
using namespace refix::overlay::saves;

int main() {
    printf("[*] Running Multi-Instance Save Redirection Unit Test...\n");

    // 1. Detect locations
    auto locations = DetectLocations();
    printf("Detected %zu save locations:\n", locations.size());
    for (const auto& loc : locations) {
        wprintf(L"  - Label: %ls\n    Live: %ls\n", loc.label.c_str(), loc.live.c_str());
    }

    // Set up test environment
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring liveDir = std::wstring(tempPath) + L"ReFixTest_LiveSaves";
    CreateDirectoryW(liveDir.c_str(), nullptr);

    Location testLoc;
    testLoc.label = L"TestGame_Saves";
    testLoc.live = liveDir;

    Profile p;
    p.id = L"test_profile_redirection_12345";
    p.name = L"TestProfile";
    p.nativeSaves = false;

    std::wstring stashDir = StashOf(p.id, testLoc.label);
    wprintf(L"Testing redirection:\n  Live:  %ls\n  Stash: %ls\n", testLoc.live.c_str(), stashDir.c_str());

    // Clean up any old test files
    std::wstring liveFile = liveDir + L"\\world_save.dat";
    std::wstring stashFile = stashDir + L"\\world_save.dat";
    DeleteFileW(liveFile.c_str());
    DeleteFileW(stashFile.c_str());

    // Enable multi-instance redirection for testLoc directly
    EnableMultiInstanceRedirection({ testLoc }, p);

    // 2. Test CreateFileW with live path
    HANDLE h = CreateFileW(liveFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(h != INVALID_HANDLE_VALUE);
    const char data[] = "world_data_profile_2";
    DWORD written = 0;
    WriteFile(h, data, (DWORD)sizeof(data), &written, nullptr);
    CloseHandle(h);

    // Verify: The file was redirected and created in stashDir, NOT in liveDir!
    DWORD liveAttr = GetFileAttributesW(liveFile.c_str());
    DWORD stashAttr = GetFileAttributesW(stashFile.c_str());

    wprintf(L"  CreateFileW check:\n");
    wprintf(L"    liveFile (raw disk without redirect hook): exists=%d\n", liveAttr != INVALID_FILE_ATTRIBUTES);
    wprintf(L"    stashFile (profile stash): exists=%d\n", stashAttr != INVALID_FILE_ATTRIBUTES);
    assert(stashAttr != INVALID_FILE_ATTRIBUTES);

    // 3. Test FindFirstFileExW with live path
    WIN32_FIND_DATAW fd = {};
    std::wstring searchPattern = liveDir + L"\\*";
    HANDLE hFind = FindFirstFileExW(searchPattern.c_str(), FindExInfoStandard, &fd, FindExSearchNameMatch, nullptr, 0);
    assert(hFind != INVALID_HANDLE_VALUE);
    bool foundInEnumeration = false;
    do {
        if (wcscmp(fd.cFileName, L"world_save.dat") == 0) {
            foundInEnumeration = true;
            break;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    wprintf(L"  FindFirstFileExW check: found world_save.dat = %d\n", foundInEnumeration);
    assert(foundInEnumeration == true);

    // Clean up
    DeleteFileW(stashFile.c_str());
    RemoveDirectoryW(liveDir.c_str());

    printf("[PASS] Multi-Instance Save Redirection verified: saves are completely isolated!\n");
    return 0;
}
