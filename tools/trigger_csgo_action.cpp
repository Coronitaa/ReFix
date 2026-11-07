#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

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
        printf("Usage: trigger_csgo_action <prime|pass|grant> [args...]\n");
        printf("  prime <0|1>\n");
        printf("  pass <season>\n");
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

    unsigned char code[64];
    size_t codeLen = 0;

    if (_stricmp(argv[1], "prime") == 0) {
        int enable = (argc >= 3) ? atoi(argv[2]) : 1;
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_SetPrimeStatus");
        if (!localFunc) {
            printf("[-] CSGO_SetPrimeStatus export not found!\n");
            CloseHandle(hProc);
            return 1;
        }
        uintptr_t rva = (uintptr_t)localFunc - (uintptr_t)localSteamApi;
        uintptr_t remoteFunc = remoteSteamApi + rva;
        printf("[+] Remote CSGO_SetPrimeStatus: 0x%08X (enable=%d)\n", (unsigned int)remoteFunc, enable);

        // push enable
        code[codeLen++] = 0x6A;
        code[codeLen++] = (unsigned char)enable;
        // mov eax, remoteFunc
        code[codeLen++] = 0xB8;
        memcpy(&code[codeLen], &remoteFunc, 4);
        codeLen += 4;
        // call eax
        code[codeLen++] = 0xFF;
        code[codeLen++] = 0xD0;
        // add esp, 4
        code[codeLen++] = 0x83;
        code[codeLen++] = 0xC4;
        code[codeLen++] = 0x04;
        // xor eax, eax
        code[codeLen++] = 0x31;
        code[codeLen++] = 0xC0;
        // ret 4
        code[codeLen++] = 0xC2;
        code[codeLen++] = 0x04;
        code[codeLen++] = 0x00;
    } else if (_stricmp(argv[1], "pass") == 0) {
        int season = (argc >= 3) ? atoi(argv[2]) : 8;
        FARPROC localFunc = GetProcAddress(localSteamApi, "CSGO_ActivateOperationPass");
        if (!localFunc) {
            printf("[-] CSGO_ActivateOperationPass export not found!\n");
            CloseHandle(hProc);
            return 1;
        }
        uintptr_t rva = (uintptr_t)localFunc - (uintptr_t)localSteamApi;
        uintptr_t remoteFunc = remoteSteamApi + rva;
        printf("[+] Remote CSGO_ActivateOperationPass: 0x%08X (season=%d)\n", (unsigned int)remoteFunc, season);

        // void CSGO_ActivateOperationPass(uint32_t season, uint64_t passItemId)
        // cdecl: push passItemId high (0), push passItemId low (0), push season
        // push 0 (passItemId high)
        code[codeLen++] = 0x6A;
        code[codeLen++] = 0x00;
        // push 0 (passItemId low)
        code[codeLen++] = 0x6A;
        code[codeLen++] = 0x00;
        // push season
        code[codeLen++] = 0x6A;
        code[codeLen++] = (unsigned char)season;
        // mov eax, remoteFunc
        code[codeLen++] = 0xB8;
        memcpy(&code[codeLen], &remoteFunc, 4);
        codeLen += 4;
        // call eax
        code[codeLen++] = 0xFF;
        code[codeLen++] = 0xD0;
        // add esp, 12
        code[codeLen++] = 0x83;
        code[codeLen++] = 0xC4;
        code[codeLen++] = 0x0C;
        // xor eax, eax
        code[codeLen++] = 0x31;
        code[codeLen++] = 0xC0;
        // ret 4
        code[codeLen++] = 0xC2;
        code[codeLen++] = 0x04;
        code[codeLen++] = 0x00;
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

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProc);

    printf("[+] Action executed successfully in CS:GO!\n");
    return 0;
}
