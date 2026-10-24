#include "refix_il2cpp.h"
#include "minhook/MinHook.h"
#include <cstdio>
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixIL2CPP {

// IL2CPP C API function pointer types
typedef void* (*fn_il2cpp_domain_get)();
typedef void* (*fn_il2cpp_thread_attach)(void* domain);
typedef void** (*fn_il2cpp_domain_get_assemblies)(void* domain, size_t* size);
typedef void* (*fn_il2cpp_assembly_get_image)(void* assembly);
typedef const char* (*fn_il2cpp_image_get_name)(void* image);
typedef void* (*fn_il2cpp_class_from_name)(void* image, const char* namespaze, const char* name);
typedef void* (*fn_il2cpp_class_get_method_from_name)(void* klass, const char* name, int argsCount);
typedef void* (*fn_il2cpp_runtime_invoke)(void* method, void* obj, void** params, void** exc);
typedef void* (*fn_il2cpp_class_get_type)(void* klass);
typedef void* (*fn_il2cpp_type_get_object)(void* type);

// Resolved function pointers
static fn_il2cpp_domain_get g_pfn_domain_get = nullptr;
static fn_il2cpp_thread_attach g_pfn_thread_attach = nullptr;
static fn_il2cpp_domain_get_assemblies g_pfn_domain_get_assemblies = nullptr;
static fn_il2cpp_assembly_get_image g_pfn_assembly_get_image = nullptr;
static fn_il2cpp_image_get_name g_pfn_image_get_name = nullptr;
static fn_il2cpp_class_from_name g_pfn_class_from_name = nullptr;
static fn_il2cpp_class_get_method_from_name g_pfn_class_get_method_from_name = nullptr;
static fn_il2cpp_runtime_invoke g_pfn_runtime_invoke = nullptr;
static fn_il2cpp_class_get_type g_pfn_class_get_type = nullptr;
static fn_il2cpp_type_get_object g_pfn_type_get_object = nullptr;

static std::atomic<bool> g_initialized{ false };
static std::atomic<bool> g_watcherRunning{ false };
static HANDLE g_hWatcherThread = nullptr;
static void* g_il2cppDomain = nullptr;

// Captured instances (thread-safe atomic pointer)
static std::atomic<void*> g_pMenuSceneLoaderInstance{ nullptr };

// Exact enum constants for Recreate.LaunchUiPhase.UiPhase:
// None = 0, Prepare = 2, Splash = 4, Animate = 6, Stay = 8, Departure = 10, EnterNemoLand = 12, Destroy = 14
static const int kUiPhase_EnterNemoLand = 12;

// Original function pointers for hooks
typedef void (*fn_SelectServer_OnApplyButtonClick_t)(void* self, void* method);
static fn_SelectServer_OnApplyButtonClick_t g_orig_SelectServer_OnApplyButtonClick = nullptr;

typedef void (*fn_SelectServer_OnCellViewSelect_t)(void* self, void* cellView, int dataIndex, void* method);
static fn_SelectServer_OnCellViewSelect_t g_orig_SelectServer_OnCellViewSelect = nullptr;

typedef void (*fn_Departure_Depart_t)(void* self, void* onComplete, void* method);
static fn_Departure_Depart_t g_orig_Departure_Depart = nullptr;

typedef void (*fn_MenuSceneLoader_Awake_t)(void* self, void* method);
static fn_MenuSceneLoader_Awake_t g_orig_MenuSceneLoader_Awake = nullptr;

typedef bool (*fn_MenuSceneLoader_get_canEnterNemoLand_t)(void* self, void* method);
static fn_MenuSceneLoader_get_canEnterNemoLand_t g_orig_MenuSceneLoader_get_canEnterNemoLand = nullptr;

typedef bool (*fn_MenuSceneLoader_get_platformInitSuccess_t)(void* self, void* method);
static fn_MenuSceneLoader_get_platformInitSuccess_t g_orig_MenuSceneLoader_get_platformInitSuccess = nullptr;

typedef bool (*fn_MenuSceneLoader_get_isAwakeComplete_t)(void* self, void* method);
static fn_MenuSceneLoader_get_isAwakeComplete_t g_orig_MenuSceneLoader_get_isAwakeComplete = nullptr;

typedef void (*fn_MenuSceneLoader_RequestInterruption_t)(void* self, void* interruption, void* arg2, void* method);
static fn_MenuSceneLoader_RequestInterruption_t g_orig_MenuSceneLoader_RequestInterruption = nullptr;

typedef bool (*fn_P2PRoomUtils_IsP2PMode_t)(void* method);
static fn_P2PRoomUtils_IsP2PMode_t g_orig_P2PRoomUtils_IsP2PMode = nullptr;

// Internal helpers
static bool EnsureThreadAttached() {
    if (!g_pfn_thread_attach || !g_il2cppDomain) return false;
    g_pfn_thread_attach(g_il2cppDomain);
    return true;
}

static std::unordered_map<std::string, void*> s_classCache;
static std::mutex s_classCacheMutex;

// Dynamically searches all loaded assemblies in the IL2CPP domain for a class with thread-safe caching
static void* FindClassAcrossImages(const char* namespaze, const char* name) {
    if (!g_il2cppDomain || !g_pfn_domain_get_assemblies || !g_pfn_assembly_get_image || !g_pfn_class_from_name) {
        return nullptr;
    }
    std::string key = std::string(namespaze ? namespaze : "") + "." + (name ? name : "");
    {
        std::lock_guard<std::mutex> lock(s_classCacheMutex);
        auto it = s_classCache.find(key);
        if (it != s_classCache.end()) return it->second;
    }

    size_t count = 0;
    void** assemblies = g_pfn_domain_get_assemblies(g_il2cppDomain, &count);
    if (!assemblies || count == 0) return nullptr;

    for (size_t i = 0; i < count; i++) {
        void* img = g_pfn_assembly_get_image(assemblies[i]);
        if (!img) continue;
        void* klass = g_pfn_class_from_name(img, namespaze, name);
        if (klass) {
            std::lock_guard<std::mutex> lock(s_classCacheMutex);
            s_classCache[key] = klass;
            return klass;
        }
    }
    return nullptr;
}

static void* GetMenuSceneLoaderInstance() {
    void* inst = g_pMenuSceneLoaderInstance.load();
    if (inst) return inst;

    EnsureThreadAttached();
    void* klassLoader = FindClassAcrossImages("", "MenuSceneLoader");
    if (!klassLoader) klassLoader = FindClassAcrossImages("Recreate", "MenuSceneLoader");
    if (!klassLoader || !g_pfn_class_get_type || !g_pfn_type_get_object) return nullptr;

    void* klassUnityObj = FindClassAcrossImages("UnityEngine", "Object");
    if (!klassUnityObj) return nullptr;

    void* methodFind = g_pfn_class_get_method_from_name(klassUnityObj, "FindObjectOfType", 1);
    if (!methodFind) return nullptr;

    void* type = g_pfn_class_get_type(klassLoader);
    if (!type) return nullptr;
    void* typeObj = g_pfn_type_get_object(type);
    if (!typeObj) return nullptr;

    void* args[1] = { typeObj };
    void* exc = nullptr;
    void* res = g_pfn_runtime_invoke(methodFind, nullptr, args, &exc);
    if (!exc && res) {
        g_pMenuSceneLoaderInstance.store(res);
        ReFixLog("[IL2CPP] Successfully discovered MenuSceneLoader instance via FindObjectOfType: %p", res);
        return res;
    }
    return nullptr;
}

static void* ResolveRealCodePointer(void* pCode) {
    if (!pCode) return nullptr;
    uint8_t* p = (uint8_t*)pCode;
    for (int i = 0; i < 32; i++) {
        if (p[i] != 0xCC && p[i] != 0x90 && p[i] != 0xFA) {
            if (i > 0) {
                ReFixLog("[IL2CPP] Advanced code pointer from %p to %p (+%d bytes padding skipped)", pCode, p + i, i);
            }
            return (void*)(p + i);
        }
    }
    return pCode;
}

static bool ActivateP2PModeInternal() {
    EnsureThreadAttached();

    void* klassUtils = FindClassAcrossImages("Recreate.Scripts.Network.P2PRoom", "P2PRoomUtils");
    if (!klassUtils) return false;

    bool success = false;

    // OpenP2PMode (param_count = 0, static)
    void* methodOpenP2P = g_pfn_class_get_method_from_name(klassUtils, "OpenP2PMode", 0);
    if (methodOpenP2P) {
        void* exc = nullptr;
        g_pfn_runtime_invoke(methodOpenP2P, nullptr, nullptr, &exc);
        if (!exc) {
            ReFixLog("[IL2CPP] P2PRoomUtils.OpenP2PMode() executed successfully");
            success = true;
        } else {
            ReFixLog("[IL2CPP] P2PRoomUtils.OpenP2PMode() exception %p", exc);
        }
    }

    // SwitchP2PFakeData (param_count = 0, static)
    void* methodFakeData = g_pfn_class_get_method_from_name(klassUtils, "SwitchP2PFakeData", 0);
    if (methodFakeData) {
        void* exc = nullptr;
        g_pfn_runtime_invoke(methodFakeData, nullptr, nullptr, &exc);
        if (!exc) {
            ReFixLog("[IL2CPP] P2PRoomUtils.SwitchP2PFakeData() executed successfully");
        }
    }

    // Verify IsP2PMode()
    void* methodIsP2P = g_pfn_class_get_method_from_name(klassUtils, "IsP2PMode", 0);
    if (methodIsP2P) {
        void* exc = nullptr;
        void* resObj = g_pfn_runtime_invoke(methodIsP2P, nullptr, nullptr, &exc);
        if (!exc && resObj) {
            bool isP2P = *(bool*)((uint8_t*)resObj + sizeof(void*) * 2);
            ReFixLog("[IL2CPP] P2PRoomUtils.IsP2PMode() status: %s", isP2P ? "TRUE" : "FALSE");
        }
    }

    return success;
}

static bool SetCanEnterNemoLandOnInstance(void* instance, bool canEnter) {
    if (!instance) return false;
    EnsureThreadAttached();

    void* klassLoader = FindClassAcrossImages("", "MenuSceneLoader");
    if (!klassLoader) klassLoader = FindClassAcrossImages("Recreate", "MenuSceneLoader");
    if (!klassLoader) return false;

    // set_canEnterNemoLand (param_count = 1, instance)
    void* methodSetCanEnter = g_pfn_class_get_method_from_name(klassLoader, "set_canEnterNemoLand", 1);
    if (methodSetCanEnter) {
        void* args[1] = { &canEnter };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(methodSetCanEnter, instance, args, &exc);
            if (!exc) {
                ReFixLog("[IL2CPP] MenuSceneLoader.set_canEnterNemoLand(%s) executed on instance %p", canEnter ? "true" : "false", instance);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.set_canEnterNemoLand");
        }
    }

    // set_platformInitSuccess (param_count = 1, instance)
    void* methodSetPlat = g_pfn_class_get_method_from_name(klassLoader, "set_platformInitSuccess", 1);
    if (methodSetPlat) {
        bool val = true;
        void* args[1] = { &val };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(methodSetPlat, instance, args, &exc);
            if (!exc) {
                ReFixLog("[IL2CPP] MenuSceneLoader.set_platformInitSuccess(true) executed on instance %p", instance);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.set_platformInitSuccess");
        }
    }

    // set_isAwakeComplete (param_count = 1, instance)
    void* methodSetAwake = g_pfn_class_get_method_from_name(klassLoader, "set_isAwakeComplete", 1);
    if (methodSetAwake) {
        bool val = true;
        void* args[1] = { &val };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(methodSetAwake, instance, args, &exc);
            if (!exc) {
                ReFixLog("[IL2CPP] MenuSceneLoader.set_isAwakeComplete(true) executed on instance %p", instance);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.set_isAwakeComplete");
        }
    }

    return true;
}

static bool TransitionToMainMenuInternal() {
    EnsureThreadAttached();

    bool anySuccess = false;

    void* loaderInst = g_pMenuSceneLoaderInstance.load();
    if (!loaderInst) {
        loaderInst = GetMenuSceneLoaderInstance();
    }

    // 1. Invoke MenuSceneLoader.TrySetUiPhase(12) [EnterNemoLand = 12] (targetObj passed when available)
    void* klassLoader = FindClassAcrossImages("", "MenuSceneLoader");
    if (!klassLoader) klassLoader = FindClassAcrossImages("Recreate", "MenuSceneLoader");
    if (klassLoader && loaderInst) {
        void* methodTrySetPhase = g_pfn_class_get_method_from_name(klassLoader, "TrySetUiPhase", 1);
        if (methodTrySetPhase) {
            int phase = kUiPhase_EnterNemoLand;
            void* args[1] = { &phase };
            void* exc = nullptr;
            __try {
                g_pfn_runtime_invoke(methodTrySetPhase, loaderInst, args, &exc);
                if (!exc) {
                    ReFixLog("[IL2CPP] MenuSceneLoader.TrySetUiPhase(12: EnterNemoLand) executed successfully! (targetObj=%p)", loaderInst);
                    anySuccess = true;
                } else {
                    ReFixLog("[IL2CPP] MenuSceneLoader.TrySetUiPhase exception %p", exc);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.TrySetUiPhase");
            }
        }
    }

    // 2. Invoke Recreate.LaunchUiPhase.set_uiPhase(12) [EnterNemoLand = 12] (STATIC METHOD)
    void* klassPhase = FindClassAcrossImages("Recreate", "LaunchUiPhase");
    if (klassPhase) {
        void* methodSetUiPhase = g_pfn_class_get_method_from_name(klassPhase, "set_uiPhase", 1);
        if (methodSetUiPhase) {
            int phase = kUiPhase_EnterNemoLand;
            void* args[1] = { &phase };
            void* exc = nullptr;
            __try {
                g_pfn_runtime_invoke(methodSetUiPhase, nullptr, args, &exc);
                if (!exc) {
                    ReFixLog("[IL2CPP] Recreate.LaunchUiPhase.set_uiPhase(12: EnterNemoLand) invoked successfully -> NemoLand Hall loading!");
                    anySuccess = true;
                } else {
                    ReFixLog("[IL2CPP] Recreate.LaunchUiPhase.set_uiPhase exception %p", exc);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ReFixLog("[IL2CPP] Exception absorbed in Recreate.LaunchUiPhase.set_uiPhase");
            }
        }
    }

    return anySuccess;
}

// Master connection function: called on server click, confirm/apply, or depart
static void ConnectToP2PAndEnterMainMenu(void* selectServerSelf) {
    static std::atomic<bool> s_connecting{ false };
    if (s_connecting.exchange(true)) {
        ReFixLog("[IL2CPP] ConnectToP2PAndEnterMainMenu already in progress; skipping duplicate trigger");
        return;
    }

    ReFixLog("[IL2CPP] ===================================================================");
    ReFixLog("[IL2CPP] Master Connection Triggered: Bypassing AWS cloud -> Steam P2P SDR");
    ReFixLog("[IL2CPP] ===================================================================");

    // 1. Enable local P2P mode & fake data
    ActivateP2PModeInternal();

    // 2. Set MenuSceneLoader flags on captured or discovered instance
    void* loaderInst = g_pMenuSceneLoaderInstance.load();
    if (!loaderInst) {
        loaderInst = GetMenuSceneLoaderInstance();
    }
    if (loaderInst) {
        SetCanEnterNemoLandOnInstance(loaderInst, true);
    }

    // 3. Reset TrailBlazer busy state if present so no spinner hangs
    void* klassTB = FindClassAcrossImages("Recreate.Scripts.Network.Account", "TrailBlazer");
    if (klassTB) {
        void* methodSetBusy = g_pfn_class_get_method_from_name(klassTB, "set_isBusy", 1);
        if (methodSetBusy) {
            bool busy = false;
            void* args[1] = { &busy };
            void* exc = nullptr;
            __try {
                g_pfn_runtime_invoke(methodSetBusy, nullptr, args, &exc);
                ReFixLog("[IL2CPP] TrailBlazer.set_isBusy(false) executed");
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ReFixLog("[IL2CPP] Exception absorbed in TrailBlazer.set_isBusy");
            }
        }
    }

    // 4. Close SelectServerPaneUi cleanly if instance is available
    if (selectServerSelf) {
        void* klassSelectServer = FindClassAcrossImages("HeoUi.Launch.SelectServer", "SelectServerPaneUi");
        if (klassSelectServer) {
            // Call Hide() to dismiss dialog
            void* methodHide = g_pfn_class_get_method_from_name(klassSelectServer, "Hide", 0);
            if (methodHide) {
                void* exc = nullptr;
                __try {
                    g_pfn_runtime_invoke(methodHide, selectServerSelf, nullptr, &exc);
                    ReFixLog("[IL2CPP] SelectServerPaneUi.Hide() executed");
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    ReFixLog("[IL2CPP] Exception absorbed in SelectServerPaneUi.Hide");
                }
            }

            // Call OnInterruptionWithdrawn(true)
            void* methodWithdrawn = g_pfn_class_get_method_from_name(klassSelectServer,
                "MenuSceneLoader.IInterruptCallback.OnInterruptionWithdrawn", 1);
            if (methodWithdrawn) {
                bool ok = true;
                void* args[1] = { &ok };
                void* exc = nullptr;
                __try {
                    g_pfn_runtime_invoke(methodWithdrawn, selectServerSelf, args, &exc);
                    ReFixLog("[IL2CPP] SelectServerPaneUi.OnInterruptionWithdrawn(true) executed");
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    ReFixLog("[IL2CPP] Exception absorbed in SelectServerPaneUi.OnInterruptionWithdrawn");
                }
            }

            // Direct notify phase changed to EnterNemoLand (12)
            void* methodPhaseChanged = g_pfn_class_get_method_from_name(klassSelectServer,
                "Recreate.LaunchUiPhase.IUiPhaseCallback.OnUiPhaseChanged", 1);
            if (methodPhaseChanged) {
                int phase = kUiPhase_EnterNemoLand;
                void* args[1] = { &phase };
                void* exc = nullptr;
                __try {
                    g_pfn_runtime_invoke(methodPhaseChanged, selectServerSelf, args, &exc);
                    ReFixLog("[IL2CPP] Dispatched OnUiPhaseChanged(12: EnterNemoLand) to SelectServerPaneUi");
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    ReFixLog("[IL2CPP] Exception absorbed in SelectServerPaneUi.OnUiPhaseChanged");
                }
            }
        }
    }

    // 5. Transition UiPhase to EnterNemoLand (12) globally across engine
    TransitionToMainMenuInternal();
}

// Hooked MenuSceneLoader.Awake - captures active loader instance
static void Hooked_MenuSceneLoader_Awake(void* self, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    ReFixLog("[IL2CPP] >>> MenuSceneLoader.Awake: Captured instance %p <<<", self);

    if (g_orig_MenuSceneLoader_Awake) {
        __try {
            g_orig_MenuSceneLoader_Awake(self, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.Awake");
        }
    }

    // Ensure flags on the instance
    SetCanEnterNemoLandOnInstance(self, true);
}

// Hooked MenuSceneLoader.get_canEnterNemoLand - always true, captures instance
static bool Hooked_MenuSceneLoader_get_canEnterNemoLand(void* self, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    return true;
}

// Hooked MenuSceneLoader.get_platformInitSuccess - always true, captures instance
static bool Hooked_MenuSceneLoader_get_platformInitSuccess(void* self, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    return true;
}

// Hooked MenuSceneLoader.get_isAwakeComplete - always true, captures instance
static bool Hooked_MenuSceneLoader_get_isAwakeComplete(void* self, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    return true;
}

// Hooked MenuSceneLoader.RequestInterruption - captures instance and forwards to game UI
static void Hooked_MenuSceneLoader_RequestInterruption(void* self, void* interruption, void* arg2, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    ReFixLog("[IL2CPP] MenuSceneLoader.RequestInterruption called on instance %p (interruption=%p) -> dispatching to game UI", self, interruption);
    if (g_orig_MenuSceneLoader_RequestInterruption) {
        __try {
            g_orig_MenuSceneLoader_RequestInterruption(self, interruption, arg2, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.RequestInterruption");
        }
    }
}

// Hooked P2PRoomUtils.IsP2PMode - always true
static bool Hooked_P2PRoomUtils_IsP2PMode(void* method) {
    return true;
}

// Hooked SelectServerPaneUi.OnApplyButtonClick
// When user clicks Confirmar/Apply, DO NOT call official AWS connection. Route straight to P2P!
static void Hooked_SelectServer_OnApplyButtonClick(void* self, void* method) {
    ReFixLog("[IL2CPP] >>> SelectServerPaneUi.OnApplyButtonClick (Confirm) clicked! <<<");
    ConnectToP2PAndEnterMainMenu(self);
}

// Hooked SelectServerPaneUi.OnCellViewSelect
// When user clicks/selects ANY server row in the list, update visual selection, then route to P2P!
static void Hooked_SelectServer_OnCellViewSelect(void* self, void* cellView, int dataIndex, void* method) {
    ReFixLog("[IL2CPP] >>> SelectServerPaneUi.OnCellViewSelect: row %d clicked! <<<", dataIndex);

    // Call original first to let the UI update its radio button / selection
    if (g_orig_SelectServer_OnCellViewSelect) {
        __try {
            g_orig_SelectServer_OnCellViewSelect(self, cellView, dataIndex, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Handled exception in SelectServerPaneUi.OnCellViewSelect");
        }
    }

    // Seamlessly connect to Steam P2P and load NemoLand!
    ConnectToP2PAndEnterMainMenu(self);
}

// Hooked DeparturePaneUi.Depart
static void Hooked_Departure_Depart(void* self, void* onComplete, void* method) {
    ReFixLog("[IL2CPP] >>> DeparturePaneUi.Depart clicked! <<<");
    ConnectToP2PAndEnterMainMenu(nullptr);
    if (g_orig_Departure_Depart) {
        __try {
            g_orig_Departure_Depart(self, onComplete, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in DeparturePaneUi.Depart");
        }
    }
}

static DWORD WINAPI WatcherThreadProc(LPVOID) {
    ReFixLog("[IL2CPP] Watcher thread started, scanning for GameAssembly.dll and IL2CPP runtime...");

    HMODULE hGA = nullptr;
    for (int i = 0; i < 200 && g_watcherRunning.load(); i++) {
        hGA = GetModuleHandleA("GameAssembly.dll");
        if (hGA) break;
        Sleep(50);
    }

    if (!hGA) {
        ReFixLog("[IL2CPP] GameAssembly.dll not found; skipping IL2CPP hook injection (generic mode).");
        return 0;
    }

    g_pfn_domain_get = (fn_il2cpp_domain_get)GetProcAddress(hGA, "il2cpp_domain_get");
    g_pfn_thread_attach = (fn_il2cpp_thread_attach)GetProcAddress(hGA, "il2cpp_thread_attach");
    g_pfn_domain_get_assemblies = (fn_il2cpp_domain_get_assemblies)GetProcAddress(hGA, "il2cpp_domain_get_assemblies");
    g_pfn_assembly_get_image = (fn_il2cpp_assembly_get_image)GetProcAddress(hGA, "il2cpp_assembly_get_image");
    g_pfn_image_get_name = (fn_il2cpp_image_get_name)GetProcAddress(hGA, "il2cpp_image_get_name");
    g_pfn_class_from_name = (fn_il2cpp_class_from_name)GetProcAddress(hGA, "il2cpp_class_from_name");
    g_pfn_class_get_method_from_name = (fn_il2cpp_class_get_method_from_name)GetProcAddress(hGA, "il2cpp_class_get_method_from_name");
    g_pfn_runtime_invoke = (fn_il2cpp_runtime_invoke)GetProcAddress(hGA, "il2cpp_runtime_invoke");
    g_pfn_class_get_type = (fn_il2cpp_class_get_type)GetProcAddress(hGA, "il2cpp_class_get_type");
    g_pfn_type_get_object = (fn_il2cpp_type_get_object)GetProcAddress(hGA, "il2cpp_type_get_object");

    if (!g_pfn_domain_get || !g_pfn_thread_attach || !g_pfn_domain_get_assemblies ||
        !g_pfn_assembly_get_image || !g_pfn_class_from_name || !g_pfn_class_get_method_from_name) {
        ReFixLog("[IL2CPP] Could not resolve required IL2CPP exports from GameAssembly.dll");
        return 0;
    }

    // Wait for the IL2CPP domain to initialize
    void* domain = nullptr;
    for (int i = 0; i < 200 && g_watcherRunning.load(); i++) {
        domain = g_pfn_domain_get();
        if (domain) break;
        Sleep(50);
    }

    if (!domain) {
        ReFixLog("[IL2CPP] IL2CPP domain initialization timed out");
        return 0;
    }

    g_il2cppDomain = domain;
    g_pfn_thread_attach(domain);
    ReFixLog("[IL2CPP] Successfully attached to IL2CPP domain %p", domain);

    // Wait until assemblies are loaded
    bool hasAssemblies = false;
    for (int attempt = 0; attempt < 50 && g_watcherRunning.load(); attempt++) {
        size_t count = 0;
        void** assemblies = g_pfn_domain_get_assemblies(domain, &count);
        if (assemblies && count > 0) {
            hasAssemblies = true;
            break;
        }
        Sleep(100);
    }

    if (!hasAssemblies) {
        ReFixLog("[IL2CPP] No assemblies loaded in domain");
        return 0;
    }

    // Initialize MinHook before creating any hooks
    MH_STATUS mhInit = MH_Initialize();
    ReFixLog("[IL2CPP] MinHook initialization status: %d (0=MH_OK, 1=ALREADY_INIT)", (int)mhInit);

    // 1. Immediately activate native P2P mode at startup
    ActivateP2PModeInternal();

    // 2. Hook P2PRoomUtils.IsP2PMode (param_count = 0)
    void* klassUtils = FindClassAcrossImages("Recreate.Scripts.Network.P2PRoom", "P2PRoomUtils");
    if (klassUtils) {
        void* methodIsP2P = g_pfn_class_get_method_from_name(klassUtils, "IsP2PMode", 0);
        if (methodIsP2P) {
            void* pCode = *(void**)methodIsP2P;
            if (pCode) {
                if (MH_CreateHook(pCode, (void*)Hooked_P2PRoomUtils_IsP2PMode, (void**)&g_orig_P2PRoomUtils_IsP2PMode) == MH_OK) {
                    MH_EnableHook(pCode);
                    ReFixLog("[IL2CPP] Hooked P2PRoomUtils.IsP2PMode at %p", pCode);
                }
            }
        }
    }

    // 3. Hook MenuSceneLoader methods
    void* klassLoader = FindClassAcrossImages("", "MenuSceneLoader");
    if (!klassLoader) klassLoader = FindClassAcrossImages("Recreate", "MenuSceneLoader");
    if (klassLoader) {
        // Awake (param_count = 0)
        void* methodAwake = g_pfn_class_get_method_from_name(klassLoader, "Awake", 0);
        if (methodAwake) {
            void* pCode = *(void**)methodAwake;
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_Awake, (void**)&g_orig_MenuSceneLoader_Awake) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.Awake at %p", pCode);
            }
        }

        // get_canEnterNemoLand (param_count = 0)
        void* methodCanEnter = g_pfn_class_get_method_from_name(klassLoader, "get_canEnterNemoLand", 0);
        if (methodCanEnter) {
            void* pCode = *(void**)methodCanEnter;
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_get_canEnterNemoLand, (void**)&g_orig_MenuSceneLoader_get_canEnterNemoLand) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.get_canEnterNemoLand at %p", pCode);
            }
        }

        // get_platformInitSuccess (param_count = 0)
        void* methodPlat = g_pfn_class_get_method_from_name(klassLoader, "get_platformInitSuccess", 0);
        if (methodPlat) {
            void* pCode = *(void**)methodPlat;
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_get_platformInitSuccess, (void**)&g_orig_MenuSceneLoader_get_platformInitSuccess) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.get_platformInitSuccess at %p", pCode);
            }
        }

        // get_isAwakeComplete (param_count = 0)
        void* methodIsAwake = g_pfn_class_get_method_from_name(klassLoader, "get_isAwakeComplete", 0);
        if (methodIsAwake) {
            void* pCode = *(void**)methodIsAwake;
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_get_isAwakeComplete, (void**)&g_orig_MenuSceneLoader_get_isAwakeComplete) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.get_isAwakeComplete at %p", pCode);
            }
        }

        // RequestInterruption (try 2 args, fallback to 1)
        void* methodReqInterr = g_pfn_class_get_method_from_name(klassLoader, "RequestInterruption", 2);
        if (!methodReqInterr) methodReqInterr = g_pfn_class_get_method_from_name(klassLoader, "RequestInterruption", 1);
        if (methodReqInterr) {
            void* pCode = *(void**)methodReqInterr;
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_RequestInterruption, (void**)&g_orig_MenuSceneLoader_RequestInterruption) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.RequestInterruption at %p", pCode);
            }
        }
    }

    // 4. Hook SelectServerPaneUi.OnApplyButtonClick (param_count = 0)
    void* klassSelectServer = FindClassAcrossImages("HeoUi.Launch.SelectServer", "SelectServerPaneUi");
    if (klassSelectServer) {
        void* methodApply = g_pfn_class_get_method_from_name(klassSelectServer, "OnApplyButtonClick", 0);
        if (methodApply) {
            void* pCode = ResolveRealCodePointer(*(void**)methodApply);
            if (pCode) {
                if (MH_CreateHook(pCode, (void*)Hooked_SelectServer_OnApplyButtonClick, (void**)&g_orig_SelectServer_OnApplyButtonClick) == MH_OK) {
                    MH_EnableHook(pCode);
                    ReFixLog("[IL2CPP] Hooked SelectServerPaneUi.OnApplyButtonClick at %p", pCode);
                }
            }
        }

        // Hook SelectServerPaneUi.OnCellViewSelect (param_count = 2)
        void* methodCellSelect = g_pfn_class_get_method_from_name(klassSelectServer, "OnCellViewSelect", 2);
        if (methodCellSelect) {
            void* pCode = ResolveRealCodePointer(*(void**)methodCellSelect);
            if (pCode) {
                if (MH_CreateHook(pCode, (void*)Hooked_SelectServer_OnCellViewSelect, (void**)&g_orig_SelectServer_OnCellViewSelect) == MH_OK) {
                    MH_EnableHook(pCode);
                    ReFixLog("[IL2CPP] Hooked SelectServerPaneUi.OnCellViewSelect at %p", pCode);
                }
            }
        }
    }

    // 5. Hook DeparturePaneUi.Depart (param_count = 1)
    void* klassDeparture = FindClassAcrossImages("HeoUi.Launch.Departure", "DeparturePaneUi");
    if (klassDeparture) {
        void* methodDepart = g_pfn_class_get_method_from_name(klassDeparture, "Depart", 1);
        if (methodDepart) {
            void* pCode = ResolveRealCodePointer(*(void**)methodDepart);
            if (pCode) {
                if (MH_CreateHook(pCode, (void*)Hooked_Departure_Depart, (void**)&g_orig_Departure_Depart) == MH_OK) {
                    MH_EnableHook(pCode);
                    ReFixLog("[IL2CPP] Hooked DeparturePaneUi.Depart at %p", pCode);
                }
            }
        }
    }

    g_initialized.store(true);
    ReFixLog("[IL2CPP] Initialization and hook installation completed successfully.");
    return 0;
}

static bool GetEnableIL2CPPBridgeConfig() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    std::string ini = (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
    char val[32] = { 0 };
    GetPrivateProfileStringA("P2P", "EnableIL2CPPBridge", "true", val, sizeof(val), ini.c_str());
    return (_stricmp(val, "true") == 0 || strcmp(val, "1") == 0 || _stricmp(val, "yes") == 0);
}

void Initialize() {
    if (!GetEnableIL2CPPBridgeConfig()) {
        ReFixLog("[IL2CPP] EnableIL2CPPBridge is disabled in ReFix.ini; skipping watcher thread.");
        return;
    }
    if (g_watcherRunning.exchange(true)) return;
    g_hWatcherThread = CreateThread(NULL, 0, WatcherThreadProc, NULL, 0, NULL);
}

void Shutdown() {
    g_watcherRunning.store(false);
    if (g_hWatcherThread) {
        WaitForSingleObject(g_hWatcherThread, 1000);
        CloseHandle(g_hWatcherThread);
        g_hWatcherThread = nullptr;
    }
}

bool IsAvailable() {
    return g_initialized.load();
}

bool ActivateP2PMode() {
    return ActivateP2PModeInternal();
}

bool TransitionToMainMenu() {
    return TransitionToMainMenuInternal();
}

} // namespace ReFixIL2CPP
