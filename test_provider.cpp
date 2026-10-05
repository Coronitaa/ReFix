#include <windows.h>
#include <iostream>

typedef bool (*SteamAPI_Init_t)();

int main() {
    HMODULE hMod = LoadLibraryA("steam_api64.dll");
    if (!hMod) {
        std::cerr << "Failed to load steam_api64.dll\n";
        return 1;
    }
    
    SteamAPI_Init_t pInit = (SteamAPI_Init_t)GetProcAddress(hMod, "SteamAPI_Init");
    if (!pInit) {
        std::cerr << "Failed to find SteamAPI_Init\n";
        return 1;
    }
    
    std::cout << "Calling SteamAPI_Init...\n";
    pInit();
    
    return 0;
}
