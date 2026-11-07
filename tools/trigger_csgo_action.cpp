#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

DWORD FindCsgoPid() {
    DWORD pid = 0;
    HWND hwnd = FindWindowA("Valve001", NULL);
    if (hwnd) {
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid) return pid;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"csgo.exe") == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

uintptr_t GetRemoteModuleBase(DWORD pid, const wchar_t* modName) {
    uintptr_t base = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = { sizeof(me) };
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, modName) == 0) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
    }
    return base;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: trigger_csgo_action <prime|pass|open_market|close_market|grant|stress_grant> [args...]\n");
        return 1;
    }

    DWORD pid = FindCsgoPid();
    if (!pid) {
        printf("[-] csgo.exe not found!\n");
        return 1;
    }
    printf("[+] Found csgo.exe PID: %lu\n", pid);

    uintptr_t remoteSteamApi = GetRemoteModuleBase(pid, L"steam_api.dll");
    if (!remoteSteamApi) {
        printf("[-] steam_api.dll not found in remote process!\n");
        return 1;
    }
    printf("[+] Remote steam_api.dll base: 0x%08X\n", (unsigned int)remoteSteamApi);

    HMODULE localSteamApi = LoadLibraryExA("D:\\Games\\CSGO\\bin\\steam_api.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!localSteamApi) {
        localSteamApi = LoadLibraryExA("bin\\lan\\x86\\steam_api.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    }
    if (!localSteamApi) {
        printf("[-] Failed to load local steam_api.dll for export resolution!\n");
        return 1;
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("[-] Failed to open remote process! Error: %lu\n", GetLastError());
        return 1;
    }

    unsigned char code[256];
    size_t codeLen = 0;

    if (_stricmp(argv[1], "open_market") == 0) {
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_OpenMarketHUD");
        if (!localFunc) { printf("[-] CSGO_OpenMarketHUD not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);
        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4;
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0; // call eax
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0; // xor eax, eax
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00; // ret 4
    } else if (_stricmp(argv[1], "close_market") == 0) {
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_CloseMarketHUD");
        if (!localFunc) { printf("[-] CSGO_CloseMarketHUD not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);
        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4;
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0;
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0;
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00;
    } else if (_stricmp(argv[1], "prime") == 0) {
        int enable = (argc >= 3) ? atoi(argv[2]) : 1;
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_SetPrimeStatus");
        if (!localFunc) { printf("[-] CSGO_SetPrimeStatus not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);
        code[codeLen++] = 0x6A; code[codeLen++] = (unsigned char)enable; // push enable
        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4; // mov eax, remoteFunc
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0; // call eax
        code[codeLen++] = 0x83; code[codeLen++] = 0xC4; code[codeLen++] = 0x04; // add esp, 4
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0;
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00;
    } else if (_stricmp(argv[1], "pass") == 0) {
        int season = (argc >= 3) ? atoi(argv[2]) : 8;
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_ActivateOperationPass");
        if (!localFunc) { printf("[-] CSGO_ActivateOperationPass not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);
        code[codeLen++] = 0x6A; code[codeLen++] = 0x00; // push 0 (passItemId high)
        code[codeLen++] = 0x6A; code[codeLen++] = 0x00; // push 0 (passItemId low)
        code[codeLen++] = 0x6A; code[codeLen++] = (unsigned char)season; // push season
        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4;
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0;
        code[codeLen++] = 0x83; code[codeLen++] = 0xC4; code[codeLen++] = 0x0C; // add esp, 12
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0;
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00;
    } else if (_stricmp(argv[1], "grant") == 0) {
        uint32_t defIndex = (argc >= 3) ? (uint32_t)atoi(argv[2]) : 7; // AK-47
        uint32_t count = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 1;
        uint32_t paintKit = (argc >= 5) ? (uint32_t)atoi(argv[4]) : 180; // Fire Serpent
        uint32_t rarity = (argc >= 6) ? (uint32_t)atoi(argv[5]) : 6;
        float wear = (argc >= 7) ? (float)atof(argv[6]) : 0.05f;
        uint32_t seed = (argc >= 8) ? (uint32_t)atoi(argv[7]) : 420;
        uint32_t quality = 4;

        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_GrantItemToLocal");
        if (!localFunc) { printf("[-] CSGO_GrantItemToLocal not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);

        // void CSGO_GrantItemToLocal(defIndex, count, quality, rarity, paintKit, wear, seed, customName)
        // cdecl arguments pushed right-to-left:
        code[codeLen++] = 0x6A; code[codeLen++] = 0x00; // push 0 (customName = NULL)
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &seed, 4); codeLen += 4;
        uint32_t uWear = *(uint32_t*)&wear;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &uWear, 4); codeLen += 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &paintKit, 4); codeLen += 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &rarity, 4); codeLen += 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &quality, 4); codeLen += 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &count, 4); codeLen += 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &defIndex, 4); codeLen += 4;

        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4;
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0; // call eax
        code[codeLen++] = 0x83; code[codeLen++] = 0xC4; code[codeLen++] = 0x20; // add esp, 32
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0;
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00;
    } else if (_stricmp(argv[1], "stress_grant") == 0) {
        int totalToGrant = (argc >= 3) ? atoi(argv[2]) : 200;
        printf("[+] Performing stress test: granting %d items sequentially...\n", totalToGrant);

        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_GrantItemToLocal");
        if (!localFunc) { printf("[-] CSGO_GrantItemToLocal not found!\n"); CloseHandle(hProc); return 1; }
        uintptr_t remoteFunc = remoteSteamApi + ((uintptr_t)localFunc - (uintptr_t)localSteamApi);

        // Build shellcode that loops 'totalToGrant' times calling remoteFunc
        // push esi
        code[codeLen++] = 0x56;
        // mov esi, totalToGrant
        code[codeLen++] = 0xBE; memcpy(&code[codeLen], &totalToGrant, 4); codeLen += 4;

        // Loop label:
        size_t loopStart = codeLen;
        code[codeLen++] = 0x6A; code[codeLen++] = 0x00; // push 0 (customName = NULL)
        code[codeLen++] = 0x56; // push esi (seed)
        float w = 0.05f; uint32_t uw = *(uint32_t*)&w;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &uw, 4); codeLen += 4;
        uint32_t pk = 180;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &pk, 4); codeLen += 4;
        uint32_t rar = 6;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &rar, 4); codeLen += 4;
        uint32_t q = 4;
        code[codeLen++] = 0x68; memcpy(&code[codeLen], &q, 4); codeLen += 4;
        code[codeLen++] = 0x6A; code[codeLen++] = 0x01; // push 1 (count)
        code[codeLen++] = 0x6A; code[codeLen++] = 0x07; // push 7 (defIndex: AK-47)

        code[codeLen++] = 0xB8; memcpy(&code[codeLen], &remoteFunc, 4); codeLen += 4;
        code[codeLen++] = 0xFF; code[codeLen++] = 0xD0; // call eax
        code[codeLen++] = 0x83; code[codeLen++] = 0xC4; code[codeLen++] = 0x20; // add esp, 32

        code[codeLen++] = 0x4E; // dec esi
        // jnz loopStart
        int relOffset = (int)loopStart - (int)(codeLen + 2);
        code[codeLen++] = 0x75;
        code[codeLen++] = (unsigned char)(relOffset & 0xFF);

        code[codeLen++] = 0x5E; // pop esi
        code[codeLen++] = 0x31; code[codeLen++] = 0xC0;
        code[codeLen++] = 0xC2; code[codeLen++] = 0x04; code[codeLen++] = 0x00;
    } else {
        printf("[-] Unknown action: %s\n", argv[1]);
        CloseHandle(hProc);
        return 1;
    }

    LPVOID remoteBuf = VirtualAllocEx(hProc, NULL, codeLen, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBuf) {
        printf("[-] VirtualAllocEx failed! Error: %lu\n", GetLastError());
        CloseHandle(hProc);
        return 1;
    }

    WriteProcessMemory(hProc, remoteBuf, code, codeLen, NULL);
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)remoteBuf, NULL, 0, NULL);
    if (!hThread) {
        printf("[-] CreateRemoteThread failed! Error: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 1;
    }

    WaitForSingleObject(hThread, 30000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProc);

    printf("[+] Action executed successfully in CS:GO!\n");
    return 0;
}
