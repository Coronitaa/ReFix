// IGameTypes::GetMaxPlayersForTypeAndMode moves between CS:GO builds (vtable slot 30 in March 2018, 31 in 2019+).
// Used by the CS:GO mod (csgo_handlers.cpp).
#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace refix::csgo {

// Address of the "skirmish" literal in the module's data sections, or null.
inline const char* FindSkirmishLiteral(HMODULE hMod) {
    static const char kLit[] = "\0skirmish"; // leading NUL: a standalone string, not the tail of another
    const auto* base = reinterpret_cast<const uint8_t*>(hMod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if ((sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) || !(sec->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA))
            continue;
        const uint8_t* p = base + sec->VirtualAddress;
        const size_t size = (std::min)(sec->Misc.VirtualSize, sec->SizeOfRawData);
        for (size_t j = 0; j + sizeof(kLit) <= size; ++j) {
            if (memcmp(p + j, kLit, sizeof(kLit)) == 0) return reinterpret_cast<const char*>(p + j + 1);
        }
    }
    return nullptr;
}

// IGameTypes::GetMaxPlayersForTypeAndMode is not at a fixed vtable slot (30 in the March 2018 build, 31 in
// 2019+). It is the only method that compares against "skirmish", so find the slot whose code references it.
inline int FindGetMaxPlayersSlot(HMODULE hMod, void** vtable) {
    const char* lit = FindSkirmishLiteral(hMod);
    if (!lit) return -1;
    const auto* base = reinterpret_cast<const uint8_t*>(hMod);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const uint8_t* end = base + nt->OptionalHeader.SizeOfImage;
    const uintptr_t needle = reinterpret_cast<uintptr_t>(lit);
    int found = -1;
    for (int k = 20; k < 48; ++k) {
        const auto* fn = reinterpret_cast<const uint8_t*>(vtable[k]);
        if (fn < base || fn + 244 > end) continue;
        for (int i = 0; i < 240; ++i) {
            if (fn[i] == 0xCC && fn[i + 1] == 0xCC) break;
            if (memcmp(fn + i, &needle, sizeof(needle)) == 0) {
                if (found >= 0 && found != k) return -1; // ambiguous: do not guess
                found = k;
                break;
            }
        }
    }
    return found;
}

} // namespace refix::csgo
