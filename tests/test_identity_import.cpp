// Tests for src/overlay/identity_import.cpp ("Import from another ReFix game" in the profile editor).
// Builds fake game folders under %TEMP% and checks which name / SteamID / picture is picked, and the errors.
// Usage: test_identity_import.exe [real game folder to print]
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../src/overlay/identity_import.h"
#include "../src/overlay/overlay_i18n.h"

#include <windows.h>
#include <cstdio>
#include <string>

// identity_import.cpp only needs the language, which reads profiles.ini under RootDir().
namespace refix::overlay::profiles {
std::wstring RootDir() { return L"C:\\__refix_test_no_root__"; }
}
namespace refix::overlay::i18n {
Lang Current() { return Lang::En; }
}

using refix::overlay::profiles::ReadReFixGame;

namespace {

int g_failed = 0;
constexpr uint64_t kMachine = 76561198000000777ULL;

void Check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failed;
}

std::wstring g_root;

std::wstring Dir(const std::wstring& rel) {
    std::wstring path = g_root;
    size_t start = 0;
    while (start <= rel.size()) {
        size_t end = rel.find(L'\\', start);
        if (end == std::wstring::npos) end = rel.size();
        path += L"\\" + rel.substr(start, end - start);
        CreateDirectoryW(path.c_str(), nullptr);
        start = end + 1;
    }
    return path;
}

void Write(const std::wstring& path, const std::string& utf8) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD put = 0;
    WriteFile(h, utf8.data(), (DWORD)utf8.size(), &put, nullptr);
    CloseHandle(h);
}

void DeleteTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            const std::wstring p = dir + L"\\" + n;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteTree(p);
            else DeleteFileW(p.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

void TestFixedIni() {
    std::puts("ReFix.ini with a fixed SteamId and a UTF-8 name");
    const std::wstring d = Dir(L"Fixed");
    Write(d + L"\\ReFix.ini", "[User]\r\nIdentityMode = fixed\r\nName = Jos\xC3\xA9\r\nSteamId = 76561198011112222\r\n");
    std::wstring err;
    auto id = ReadReFixGame(d, kMachine, err);
    Check(id.has_value(), "found");
    Check(id && id->name == L"Jos\u00E9", "name decoded as UTF-8");
    Check(id && id->steamId == 76561198011112222ULL, "fixed SteamId");
    Check(id && !id->machineId, "not the machine id");
    Check(id && id->gameLabel == L"Fixed", "label is the folder of ReFix.ini");
}

void TestLogWins() {
    std::puts("ReFix.log beats ReFix.ini, nested three levels down");
    const std::wstring d = Dir(L"Nested\\Game\\Binaries\\Win64");
    Write(d + L"\\ReFix.ini", "[User]\nIdentityMode = auto\nName = FromIni\nSteamId = \n");
    Write(d + L"\\ReFix.log",
          "[x] [ConfigDiag] SteamID Source: 'IdentityMode=auto' -> 76561198000000001\n"
          "[x] [ConfigDiag] Persona Name Source: 'ini' -> 'Old'\n"
          "[x] [ConfigDiag] SteamID Source: 'ini:SteamId' -> 76561198033334444\n"
          "[x] [ConfigDiag] Persona Name Source: 'ini' -> 'Last Name'\n");
    std::wstring err;
    auto id = ReadReFixGame(g_root + L"\\Nested", kMachine, err);
    Check(id.has_value(), "found under the game root");
    Check(id && id->steamId == 76561198033334444ULL, "last SteamID of the log");
    Check(id && id->name == L"Last Name", "last persona name of the log");
    Check(id && id->gameLabel == L"Win64", "label");
}

void TestProfileSourceRefused() {
    std::puts("Game already on ReFix LAN profiles");
    const std::wstring d = Dir(L"OnProfiles");
    Write(d + L"\\ReFix.ini", "[User]\nIdentityMode = auto\nName = Valen\n");
    Write(d + L"\\ReFix.log", "[ConfigDiag] SteamID Source: 'profile' -> 76561198187388298\n");
    std::wstring err;
    auto id = ReadReFixGame(d, kMachine, err);
    Check(!id.has_value(), "refused");
    Check(err.find(L"profiles") != std::wstring::npos, "explains that profiles are shared");
}

void TestAutoMachineId() {
    std::puts("IdentityMode=auto without SteamId or log: this PC's SteamID");
    const std::wstring d = Dir(L"Auto");
    Write(d + L"\\ReFix.ini", "[User]\nIdentityMode = auto\nName = Player_4903\nSteamId = \n");
    std::wstring err;
    auto id = ReadReFixGame(d, kMachine, err);
    Check(id && id->steamId == kMachine, "machine SteamID");
    Check(id && id->machineId, "flagged as machine id");
    Check(id && id->name == L"Player_4903", "name");
}

void TestSteamSettings() {
    std::puts("steam_settings files and account_avatar");
    const std::wstring d = Dir(L"Goldberg");
    const std::wstring s = Dir(L"Goldberg\\steam_settings");
    Write(d + L"\\ReFix.ini", "[User]\nIdentityMode = fixed\nSteamId = \n");
    Write(s + L"\\force_steamid.txt", "76561198055556666\r\n");
    Write(s + L"\\configs.user.ini", "[user::general]\naccount_name=Gold\n");
    Write(s + L"\\account_avatar.png", "png");
    std::wstring err;
    auto id = ReadReFixGame(d, kMachine, err);
    Check(id && id->steamId == 76561198055556666ULL, "force_steamid.txt");
    Check(id && id->name == L"Gold", "configs.user.ini account_name");
    Check(id && id->avatarPath == s + L"\\account_avatar.png", "account_avatar.png");
    Check(id && !id->machineId, "fixed mode never falls back to the machine id");
}

void TestProfileFolder() {
    std::puts("ReFix LAN profile folder (profile.ini + avatar.png)");
    const std::wstring d = Dir(L"Profile");
    Write(d + L"\\profile.ini", "[Profile]\r\nName=Mona\r\nSteamId=76561198077778888\r\nAvatar=custom\r\n");
    Write(d + L"\\avatar.png", "png");
    std::wstring err;
    auto id = ReadReFixGame(d, kMachine, err);
    Check(id && id->name == L"Mona" && id->steamId == 76561198077778888ULL, "name and SteamID");
    Check(id && id->avatarPath == d + L"\\avatar.png", "custom avatar");
}

void TestErrors() {
    std::puts("Errors");
    std::wstring err;
    Check(!ReadReFixGame(Dir(L"Empty"), kMachine, err) && !err.empty(), "folder without ReFix");
    err.clear();
    Check(!ReadReFixGame(g_root + L"\\Missing", kMachine, err) && !err.empty(), "missing folder");
    const std::wstring d = Dir(L"BadId");
    Write(d + L"\\ReFix.ini", "[User]\nIdentityMode = fixed\nName = \nSteamId = 12345\n");
    err.clear();
    Check(!ReadReFixGame(d, kMachine, err), "invalid SteamID and no name");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc > 1) {
        std::wstring err;
        auto id = ReadReFixGame(argv[1], kMachine, err);
        if (!id) { std::wprintf(L"error: %ls\n", err.c_str()); return 1; }
        std::wprintf(L"name='%ls' steamId=%llu machine=%d avatar='%ls' label='%ls'\n", id->name.c_str(),
                     (unsigned long long)id->steamId, id->machineId ? 1 : 0, id->avatarPath.c_str(), id->gameLabel.c_str());
        return 0;
    }
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    g_root = std::wstring(tmp) + L"refix_identity_import_" + std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(g_root.c_str(), nullptr);
    TestFixedIni();
    TestLogWins();
    TestProfileSourceRefused();
    TestAutoMachineId();
    TestSteamSettings();
    TestProfileFolder();
    TestErrors();
    DeleteTree(g_root);
    std::printf(g_failed ? "[FAILED] %d check(s)\n" : "[ALL PASS] identity import\n", g_failed);
    return g_failed ? 1 : 0;
}
