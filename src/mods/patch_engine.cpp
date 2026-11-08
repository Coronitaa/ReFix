#include "patch_engine.h"
#include "../minhook/MinHook.h"

#include <windows.h>
#include <algorithm>
#include <iostream>

namespace refix::mods {

static const char* s_ForbiddenModules[] = {
    // NT / Kernel / Core System
    "ntdll", "ntdll.dll",
    "kernel32", "kernel32.dll",
    "kernelbase", "kernelbase.dll",
    "user32", "user32.dll",
    "gdi32", "gdi32.dll",
    "advapi32", "advapi32.dll",
    "shell32", "shell32.dll",
    "shlwapi", "shlwapi.dll",
    "ole32", "ole32.dll",
    "oleaut32", "oleaut32.dll",
    "crypt32", "crypt32.dll",
    "secur32", "secur32.dll",
    "ws2_32", "ws2_32.dll",
    "wsock32", "wsock32.dll",
    "iphlpapi", "iphlpapi.dll",
    "mswsock", "mswsock.dll",
    "dnsapi", "dnsapi.dll",
    "msvcrt", "msvcrt.dll",
    "ucrtbase", "ucrtbase.dll",

    // ReFix Internal Modules
    "winmm", "winmm.dll",
    "refix", "refix.dll",
    "refix_lan", "refix_lan.dll",
    "refix_overlay", "refix_overlay.dll",
    "steam_api", "steam_api.dll",
    "steam_api64", "steam_api64.dll",
    "eossdk-win64-shipping", "eossdk-win64-shipping.dll",
    "redboneeos", "redboneeos.dll"
};

bool PatchEngine::IsForbiddenModule(const std::string& module_name) {
    std::string lower = module_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Strip leading path if present
    size_t last_slash = lower.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        lower = lower.substr(last_slash + 1);
    }

    for (const char* forbidden : s_ForbiddenModules) {
        if (lower == forbidden) {
            return true;
        }
    }
    return false;
}

HMODULE PatchEngine::ResolveTargetModule(const std::string& module_name) {
    if (module_name.empty()) {
        return GetModuleHandleA(NULL); // Game EXE
    }
    return GetModuleHandleA(module_name.c_str());
}

static bool SafeCheckPeHeaders(HMODULE hMod, void* address) {
    __try {
        auto* dos_hdr = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
        if (dos_hdr->e_magic == IMAGE_DOS_SIGNATURE) {
            auto* nt_hdr = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(hMod) + dos_hdr->e_lfanew);
            if (nt_hdr->Signature == IMAGE_NT_SIGNATURE) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(address);
                uintptr_t base = reinterpret_cast<uintptr_t>(hMod);

                // IAT
                uint32_t iat_rva = nt_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
                uint32_t iat_size = nt_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;
                if (iat_rva > 0 && iat_size > 0) {
                    if (addr >= base + iat_rva && addr < base + iat_rva + iat_size) {
                        return true;
                    }
                }

                // Export Table
                uint32_t exp_rva = nt_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                uint32_t exp_size = nt_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
                if (exp_rva > 0 && exp_size > 0) {
                    if (addr >= base + exp_rva && addr < base + exp_rva + exp_size) {
                        return true;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
    return false;
}

bool PatchEngine::IsProtectedMemoryRange(void* address, size_t size) {
    if (!address || size == 0) return true;

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return true;
    }

    // If mapped from an executable image (PE module)
    if (mbi.Type == MEM_IMAGE) {
        HMODULE hMod = reinterpret_cast<HMODULE>(mbi.AllocationBase);
        if (hMod != NULL) {
            char mod_path[MAX_PATH];
            if (GetModuleFileNameA(hMod, mod_path, MAX_PATH)) {
                if (IsForbiddenModule(mod_path)) {
                    return true; // Target is inside a forbidden module
                }
            }

            if (SafeCheckPeHeaders(hMod, address)) {
                return true;
            }
        }
    }

    return false;
}

PatchEngine::PatchEngine() {
}

PatchEngine::~PatchEngine() {
    RollbackAll();
    RemoveAllHooks();
    if (minhook_initialized_) {
        MH_Uninitialize();
        minhook_initialized_ = false;
    }
}

bool PatchEngine::Initialize() {
    if (!minhook_initialized_) {
        MH_STATUS status = MH_Initialize();
        if (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED) {
            minhook_initialized_ = true;
            return true;
        }
        return false;
    }
    return true;
}

bool PatchEngine::ApplyPatch(const std::string& mod_id,
                            const std::string& target_module,
                            uint32_t rva,
                            const std::vector<uint8_t>& expected_orig,
                            const std::vector<uint8_t>& replacement,
                            std::string& out_error) {
    if (IsForbiddenModule(target_module)) {
        out_error = "Patch target module '" + target_module + "' is a forbidden system or ReFix library";
        return false;
    }

    if (replacement.empty()) {
        out_error = "Replacement bytes buffer is empty";
        return false;
    }

    HMODULE hMod = ResolveTargetModule(target_module);
    if (!hMod) {
        out_error = "Target module '" + target_module + "' is not loaded in process";
        return false;
    }

    uint8_t* target_addr = reinterpret_cast<uint8_t*>(hMod) + rva;

    if (IsProtectedMemoryRange(target_addr, replacement.size())) {
        out_error = "Target address is within protected/system memory range";
        return false;
    }

    // Verify expected original bytes
    if (!expected_orig.empty()) {
        if (memcmp(target_addr, expected_orig.data(), expected_orig.size()) != 0) {
            out_error = "Original bytes mismatch at RVA 0x" + std::to_string(rva) + " (game version mismatch)";
            return false;
        }
    }

    // Apply patch with VirtualProtect
    DWORD old_protect = 0;
    if (!VirtualProtect(target_addr, replacement.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
        out_error = "VirtualProtect PAGE_EXECUTE_READWRITE failed";
        return false;
    }

    DeclarativePatch patch;
    patch.mod_id = mod_id;
    patch.target_module = target_module;
    patch.rva = rva;
    patch.expected_original_bytes = expected_orig;
    patch.replacement_bytes = replacement;
    patch.applied_address = target_addr;
    patch.backed_up_bytes.assign(target_addr, target_addr + replacement.size());

    memcpy(target_addr, replacement.data(), replacement.size());

    DWORD dummy = 0;
    VirtualProtect(target_addr, replacement.size(), old_protect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target_addr, replacement.size());

    patch.is_applied = true;
    patches_.push_back(patch);
    return true;
}

void PatchEngine::RollbackModPatches(const std::string& mod_id) {
    for (auto& patch : patches_) {
        if (patch.mod_id == mod_id && patch.is_applied && patch.applied_address != nullptr) {
            DWORD old_protect = 0;
            if (VirtualProtect(patch.applied_address, patch.backed_up_bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
                memcpy(patch.applied_address, patch.backed_up_bytes.data(), patch.backed_up_bytes.size());
                DWORD dummy = 0;
                VirtualProtect(patch.applied_address, patch.backed_up_bytes.size(), old_protect, &dummy);
                FlushInstructionCache(GetCurrentProcess(), patch.applied_address, patch.backed_up_bytes.size());
            }
            patch.is_applied = false;
        }
    }
}

void PatchEngine::RollbackAll() {
    for (auto& patch : patches_) {
        if (patch.is_applied && patch.applied_address != nullptr) {
            DWORD old_protect = 0;
            if (VirtualProtect(patch.applied_address, patch.backed_up_bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
                memcpy(patch.applied_address, patch.backed_up_bytes.data(), patch.backed_up_bytes.size());
                DWORD dummy = 0;
                VirtualProtect(patch.applied_address, patch.backed_up_bytes.size(), old_protect, &dummy);
                FlushInstructionCache(GetCurrentProcess(), patch.applied_address, patch.backed_up_bytes.size());
            }
            patch.is_applied = false;
        }
    }
    patches_.clear();
}

bool PatchEngine::RegisterHook(const std::string& mod_id,
                              const std::string& target_module,
                              uint32_t rva,
                              void* detour_func,
                              void** out_orig_trampoline,
                              std::string& out_error) {
    if (!Initialize()) {
        out_error = "MinHook initialization failed";
        return false;
    }

    if (IsForbiddenModule(target_module)) {
        out_error = "Hook target module '" + target_module + "' is forbidden";
        return false;
    }

    HMODULE hMod = ResolveTargetModule(target_module);
    if (!hMod) {
        out_error = "Module '" + target_module + "' is not loaded";
        return false;
    }

    void* target_func = reinterpret_cast<uint8_t*>(hMod) + rva;
    if (IsProtectedMemoryRange(target_func, 1)) {
        out_error = "Hook target is within protected/system memory range";
        return false;
    }

    void* orig_trampoline = nullptr;
    MH_STATUS status = MH_CreateHook(target_func, detour_func, &orig_trampoline);
    if (status != MH_OK) {
        out_error = "MH_CreateHook failed: " + std::to_string(status);
        return false;
    }

    status = MH_EnableHook(target_func);
    if (status != MH_OK) {
        MH_RemoveHook(target_func);
        out_error = "MH_EnableHook failed: " + std::to_string(status);
        return false;
    }

    if (out_orig_trampoline) {
        *out_orig_trampoline = orig_trampoline;
    }

    MediatedHook hook;
    hook.mod_id = mod_id;
    hook.target_module = target_module;
    hook.rva = rva;
    hook.target_func = target_func;
    hook.original_trampoline = orig_trampoline;
    hook.is_enabled = true;
    hooks_.push_back(hook);

    return true;
}

void PatchEngine::RemoveModHooks(const std::string& mod_id) {
    for (auto it = hooks_.begin(); it != hooks_.end(); ) {
        if (it->mod_id == mod_id) {
            if (it->is_enabled && it->target_func != nullptr) {
                MH_DisableHook(it->target_func);
                MH_RemoveHook(it->target_func);
            }
            it = hooks_.erase(it);
        } else {
            ++it;
        }
    }
}

void PatchEngine::RemoveAllHooks() {
    for (auto& hook : hooks_) {
        if (hook.is_enabled && hook.target_func != nullptr) {
            MH_DisableHook(hook.target_func);
            MH_RemoveHook(hook.target_func);
            hook.is_enabled = false;
        }
    }
    hooks_.clear();
}

bool PatchEngine::GuardedMemoryWrite(const std::string& mod_id,
                                    void* target_address,
                                    const void* data,
                                    size_t size,
                                    std::string& out_error) {
    if (!target_address || !data || size == 0) {
        out_error = "Invalid arguments for memory write";
        return false;
    }

    if (IsProtectedMemoryRange(target_address, size)) {
        out_error = "Memory write rejected: address is inside protected system/IAT/EAT range";
        return false;
    }

    // Anti-hijacking heuristic: check if any 4/8 byte value being written is a pointer into system code
    if (size >= sizeof(void*)) {
        size_t words = size / sizeof(void*);
        const uintptr_t* ptr_val = reinterpret_cast<const uintptr_t*>(data);
        for (size_t i = 0; i < words; ++i) {
            uintptr_t val = ptr_val[i];
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(reinterpret_cast<void*>(val), &mbi, sizeof(mbi))) {
                if ((mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0) {
                    HMODULE hMod = reinterpret_cast<HMODULE>(mbi.AllocationBase);
                    if (hMod != NULL) {
                        char mod_name[MAX_PATH];
                        if (GetModuleFileNameA(hMod, mod_name, MAX_PATH)) {
                            if (IsForbiddenModule(mod_name)) {
                                out_error = "Memory write rejected: attempted to write pointer to system executable code (ROP mitigation)";
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target_address, size, PAGE_READWRITE, &old_protect)) {
        out_error = "VirtualProtect failed for memory write";
        return false;
    }

    memcpy(target_address, data, size);

    DWORD dummy = 0;
    VirtualProtect(target_address, size, old_protect, &dummy);
    return true;
}

} // namespace refix::mods
