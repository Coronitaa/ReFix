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
typedef void* (*fn_il2cpp_class_get_field_from_name)(void* klass, const char* name);
typedef void (*fn_il2cpp_field_static_get_value)(void* field, void* value);
typedef void (*fn_il2cpp_field_static_set_value)(void* field, void* value);
typedef void (*fn_il2cpp_field_get_value)(void* obj, void* field, void* value);
typedef void (*fn_il2cpp_field_set_value)(void* obj, void* field, void* value);
typedef int (*fn_il2cpp_field_get_offset)(void* field);
typedef void* (*fn_il2cpp_object_get_class)(void* obj);
typedef void (*fn_il2cpp_runtime_class_init)(void* klass);
typedef bool (*fn_il2cpp_class_is_assignable_from)(void* targetClass, void* fromClass);
typedef void (*fn_il2cpp_format_exception)(void* exc, char* message, int message_size);
typedef void* (*fn_il2cpp_string_new)(const char* str);

typedef void* (*fn_il2cpp_object_get_virtual_method)(void* obj, void* method);
typedef void* (*fn_il2cpp_class_get_methods)(void* klass, void** iter);
typedef const char* (*fn_il2cpp_method_get_name)(void* method);

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
static fn_il2cpp_class_get_field_from_name g_pfn_class_get_field_from_name = nullptr;
static fn_il2cpp_field_static_get_value g_pfn_field_static_get_value = nullptr;
static fn_il2cpp_field_static_set_value g_pfn_field_static_set_value = nullptr;
static fn_il2cpp_field_get_value g_pfn_field_get_value = nullptr;
static fn_il2cpp_field_set_value g_pfn_field_set_value = nullptr;
static fn_il2cpp_field_get_offset g_pfn_field_get_offset = nullptr;
static fn_il2cpp_object_get_class g_pfn_object_get_class = nullptr;
static fn_il2cpp_runtime_class_init g_pfn_runtime_class_init = nullptr;
static fn_il2cpp_class_is_assignable_from g_pfn_class_is_assignable_from = nullptr;
static fn_il2cpp_format_exception g_pfn_format_exception = nullptr;
static fn_il2cpp_string_new g_pfn_string_new = nullptr;
static fn_il2cpp_object_get_virtual_method g_pfn_object_get_virtual_method = nullptr;
static fn_il2cpp_class_get_methods g_pfn_class_get_methods = nullptr;
static fn_il2cpp_method_get_name g_pfn_method_get_name = nullptr;

static std::atomic<bool> g_initialized{ false };
static std::atomic<bool> g_watcherRunning{ false };
static HANDLE g_hWatcherThread = nullptr;
static void* g_il2cppDomain = nullptr;

// Captured instances and state
static std::atomic<void*> g_pMenuSceneLoaderInstance{ nullptr };
static std::atomic<void*> g_pSelectServerPaneUiInstance{ nullptr };
static std::atomic<void*> g_pWallBreakerInstance{ nullptr };
static std::atomic<bool> s_hasTransitioned{ false };
static std::atomic<bool> s_interruptionSeen{ false };
static std::atomic<DWORD> s_interruptionTime{ 0 };
static std::atomic<bool> s_pendingMainThreadTransition{ false };
static HANDLE s_hIL2CPPMasterMutex = nullptr;

// Original function pointers for hooks
typedef void (*fn_SelectServer_OnApplyButtonClick_t)(void* self, void* method);
static fn_SelectServer_OnApplyButtonClick_t g_orig_SelectServer_OnApplyButtonClick = nullptr;

typedef void (*fn_SelectServer_OnCellViewSelect_t)(void* self, void* cellView, int dataIndex, void* method);
static fn_SelectServer_OnCellViewSelect_t g_orig_SelectServer_OnCellViewSelect = nullptr;

typedef void (*fn_SoundManager_FixedUpdate_t)(void* self, void* method);
static fn_SoundManager_FixedUpdate_t g_orig_SoundManager_FixedUpdate = nullptr;
static std::atomic<void*> g_pSoundManagerInstance{ nullptr };

typedef void (*fn_MenuSceneLoader_Awake_t)(void* self, void* method);
static fn_MenuSceneLoader_Awake_t g_orig_MenuSceneLoader_Awake = nullptr;

typedef void (*fn_MenuSceneLoader_RequestInterruption_t)(void* self, void* interruption, void* arg2, void* method);
static fn_MenuSceneLoader_RequestInterruption_t g_orig_MenuSceneLoader_RequestInterruption = nullptr;

typedef bool (*fn_P2PRoomUtils_IsP2PMode_t)(void* method);
static fn_P2PRoomUtils_IsP2PMode_t g_orig_P2PRoomUtils_IsP2PMode = nullptr;

typedef void (*fn_WallBreaker_Awake_t)(void* self, void* method);
static fn_WallBreaker_Awake_t g_orig_WallBreaker_Awake = nullptr;

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

static void* FindLiveObjectInScene(const char* namespaze, const char* className) {
    EnsureThreadAttached();
    void* klass = FindClassAcrossImages(namespaze, className);
    if (!klass || !g_pfn_class_get_type || !g_pfn_type_get_object) return nullptr;

    void* klassUnityObj = FindClassAcrossImages("UnityEngine", "Object");
    if (!klassUnityObj) return nullptr;

    // Safety check: FindObjectOfType requires klass to derive from UnityEngine.Object
    if (g_pfn_class_is_assignable_from && !g_pfn_class_is_assignable_from(klassUnityObj, klass)) {
        return nullptr;
    }

    void* type = g_pfn_class_get_type(klass);
    if (!type) return nullptr;
    void* typeObj = g_pfn_type_get_object(type);
    if (!typeObj) return nullptr;

    void* methodFind = g_pfn_class_get_method_from_name(klassUnityObj, "FindObjectOfType", 1);
    if (methodFind) {
        void* args[1] = { typeObj };
        void* exc = nullptr;
        __try {
            void* res = g_pfn_runtime_invoke(methodFind, nullptr, args, &exc);
            if (!exc && res) return res;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void* methodFind2 = g_pfn_class_get_method_from_name(klassUnityObj, "FindObjectOfType", 2);
    if (methodFind2) {
        bool includeInactive = true;
        void* args2[2] = { typeObj, &includeInactive };
        void* exc2 = nullptr;
        __try {
            void* res2 = g_pfn_runtime_invoke(methodFind2, nullptr, args2, &exc2);
            if (!exc2 && res2) return res2;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return nullptr;
}

static bool SetObjectActive(void* unityComponentOrGameObject, bool active) {
    if (!unityComponentOrGameObject) return false;
    EnsureThreadAttached();

    void* klassGO = FindClassAcrossImages("UnityEngine", "GameObject");
    void* klassComp = FindClassAcrossImages("UnityEngine", "Component");
    if (!klassGO || !klassComp) return false;

    void* gameObject = unityComponentOrGameObject;
    void* methodGetGO = g_pfn_class_get_method_from_name(klassComp, "get_gameObject", 0);
    if (methodGetGO) {
        void* exc = nullptr;
        __try {
            void* go = g_pfn_runtime_invoke(methodGetGO, unityComponentOrGameObject, nullptr, &exc);
            if (!exc && go) gameObject = go;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void* methodSetActive = g_pfn_class_get_method_from_name(klassGO, "SetActive", 1);
    if (methodSetActive) {
        void* args[1] = { &active };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(methodSetActive, gameObject, args, &exc);
            return (!exc);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    return false;
}

static void* GetMenuSceneLoaderInstance() {
    void* inst = g_pMenuSceneLoaderInstance.load();
    if (inst) return inst;

    inst = FindLiveObjectInScene("", "MenuSceneLoader");
    if (!inst) inst = FindLiveObjectInScene("Recreate", "MenuSceneLoader");
    if (inst) {
        g_pMenuSceneLoaderInstance.store(inst);
        ReFixLog("[IL2CPP] Successfully discovered MenuSceneLoader instance via FindObjectOfType: %p", inst);
        return inst;
    }
    return nullptr;
}

static void* GetWallBreakerInstance() {
    void* inst = g_pWallBreakerInstance.load();
    if (inst) return inst;

    void* klassWallBreaker = FindClassAcrossImages("HeoUi.Launch", "WallBreaker");
    if (klassWallBreaker) {
        if (g_pfn_runtime_class_init) g_pfn_runtime_class_init(klassWallBreaker);
        if (g_pfn_class_get_field_from_name && g_pfn_field_static_get_value) {
            void* fieldSingleton = g_pfn_class_get_field_from_name(klassWallBreaker, "s_Singleton");
            if (fieldSingleton) {
                void* singletonInst = nullptr;
                g_pfn_field_static_get_value(fieldSingleton, &singletonInst);
                if (singletonInst) {
                    g_pWallBreakerInstance.store(singletonInst);
                    ReFixLog("[IL2CPP] Discovered WallBreaker via s_Singleton: %p", singletonInst);
                    return singletonInst;
                }
            }
        }
    }

    inst = FindLiveObjectInScene("HeoUi.Launch", "WallBreaker");
    if (inst) {
        g_pWallBreakerInstance.store(inst);
        ReFixLog("[IL2CPP] Discovered WallBreaker via FindLiveObjectInScene: %p", inst);
        return inst;
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

static void* ResolveConcreteMethod(void* obj, void* intfMethod, void* fallbackKlass, const char* methodName) {
    if (obj && intfMethod && g_pfn_object_get_virtual_method) {
        void* virt = g_pfn_object_get_virtual_method(obj, intfMethod);
        if (virt) return virt;
    }

    void* objKlass = (obj && g_pfn_object_get_class) ? g_pfn_object_get_class(obj) : nullptr;
    void* targetKlasses[2] = { objKlass, fallbackKlass };

    for (int k = 0; k < 2; k++) {
        void* klass = targetKlasses[k];
        if (!klass) continue;

        if (g_pfn_class_get_methods && g_pfn_method_get_name) {
            void* iter = nullptr;
            void* m = nullptr;
            while ((m = g_pfn_class_get_methods(klass, &iter)) != nullptr) {
                const char* name = g_pfn_method_get_name(m);
                if (name && (strcmp(name, methodName) == 0 || strstr(name, methodName) != nullptr)) {
                    return m;
                }
            }
        }
    }

    return intfMethod;
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
    if (g_pfn_runtime_class_init) g_pfn_runtime_class_init(klassLoader);

    // 1. set_canEnterNemoLand (param_count = 1, instance)
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

    // 2. set_platformInitSuccess (param_count = 1, instance)
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

    // 3. set_isAwakeComplete (param_count = 1, instance)
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

    // 4. set_existInterruption(false) -> Clears modal interruption state on loader
    void* methodSetInterr = g_pfn_class_get_method_from_name(klassLoader, "set_existInterruption", 1);
    if (methodSetInterr) {
        bool val = false;
        void* args[1] = { &val };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(methodSetInterr, instance, args, &exc);
            if (!exc) {
                ReFixLog("[IL2CPP] MenuSceneLoader.set_existInterruption(false) executed on instance %p", instance);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in MenuSceneLoader.set_existInterruption");
        }
    }

    return true;
}

static bool IsMethodCallable(void* method) {
    if (!method) return false;
    void* pCode = *(void**)method;
    return pCode != nullptr;
}

static void DispatchUiPhaseToObject(void* obj, int phase) {
    if (!obj) return;
    void* klassObj = g_pfn_object_get_class ? g_pfn_object_get_class(obj) : nullptr;
    if (!klassObj) return;

    void* targetMethod = nullptr;
    if (g_pfn_class_get_methods && g_pfn_method_get_name) {
        void* iter = nullptr;
        void* m = nullptr;
        while ((m = g_pfn_class_get_methods(klassObj, &iter)) != nullptr) {
            const char* name = g_pfn_method_get_name(m);
            if (name && strstr(name, "OnUiPhaseChanged") != nullptr) {
                targetMethod = m;
                break;
            }
        }
    }
    if (!targetMethod && g_pfn_class_get_method_from_name) {
        targetMethod = g_pfn_class_get_method_from_name(klassObj, "OnUiPhaseChanged", 1);
        if (!targetMethod) targetMethod = g_pfn_class_get_method_from_name(klassObj, "Recreate.LaunchUiPhase.IUiPhaseCallback.OnUiPhaseChanged", 1);
    }

    if (IsMethodCallable(targetMethod)) {
        void* args[1] = { &phase };
        void* exc = nullptr;
        __try {
            g_pfn_runtime_invoke(targetMethod, obj, args, &exc);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

static void DispatchUiPhaseToAllCallbacks(int phase) {
    __try {
        // Dispatch directly to known active launch instances with safety checks
        void* wb = GetWallBreakerInstance();
        if (wb) DispatchUiPhaseToObject(wb, phase);

        void* ss = g_pSelectServerPaneUiInstance.load();
        if (ss) DispatchUiPhaseToObject(ss, phase);

        void* dep = FindLiveObjectInScene("HeoUi.Launch.Departure", "DeparturePaneUi");
        if (dep) DispatchUiPhaseToObject(dep, phase);

        void* launch = FindLiveObjectInScene("HeoUi.Launch", "LaunchUi");
        if (launch) DispatchUiPhaseToObject(launch, phase);

        void* prog = FindLiveObjectInScene("HeoUi.Launch.TextProgress", "TextProgressPaneUi");
        if (prog) DispatchUiPhaseToObject(prog, phase);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ReFixLog("[IL2CPP] Exception absorbed in DispatchUiPhaseToAllCallbacks(%d)", phase);
    }
}

static void TransitionWallBreaker(void* wallBreakerInst) {
    if (!wallBreakerInst) return;
    ReFixLog("[IL2CPP] TransitionWallBreaker executing on %p", wallBreakerInst);

    void* klassWallBreaker = FindClassAcrossImages("HeoUi.Launch", "WallBreaker");
    if (!klassWallBreaker && g_pfn_object_get_class) klassWallBreaker = g_pfn_object_get_class(wallBreakerInst);

    // 1. Dispatch Departure (10 / 5) and EnterNemoLand (12 / 6)
    DispatchUiPhaseToObject(wallBreakerInst, 10);
    DispatchUiPhaseToObject(wallBreakerInst, 5);
    DispatchUiPhaseToObject(wallBreakerInst, 12);
    DispatchUiPhaseToObject(wallBreakerInst, 6);

    // 2. Invoke PlayEnterAnimationAsync (param_count = 0)
    if (klassWallBreaker) {
        void* methodEnter = g_pfn_class_get_method_from_name(klassWallBreaker, "PlayEnterAnimationAsync", 0);
        if (IsMethodCallable(methodEnter)) {
            void* exc = nullptr;
            __try {
                g_pfn_runtime_invoke(methodEnter, wallBreakerInst, nullptr, &exc);
                ReFixLog("[IL2CPP] WallBreaker.PlayEnterAnimationAsync() invoked (exc=%p)", exc);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // 3. Deactivate uiBackgroundGo (white background quad)
        if (g_pfn_class_get_field_from_name && g_pfn_field_get_value) {
            void* fBg = g_pfn_class_get_field_from_name(klassWallBreaker, "uiBackgroundGo");
            if (fBg) {
                void* bgGo = nullptr;
                g_pfn_field_get_value(wallBreakerInst, fBg, &bgGo);
                if (bgGo) {
                    SetObjectActive(bgGo, false);
                    ReFixLog("[IL2CPP] WallBreaker.uiBackgroundGo set inactive");
                }
            }

            // 4. Deactivate groundGo
            void* fGround = g_pfn_class_get_field_from_name(klassWallBreaker, "groundGo");
            if (fGround) {
                void* groundGo = nullptr;
                g_pfn_field_get_value(wallBreakerInst, fGround, &groundGo);
                if (groundGo) {
                    SetObjectActive(groundGo, false);
                    ReFixLog("[IL2CPP] WallBreaker.groundGo set inactive");
                }
            }

            // 5. Deactivate castMemberStayColliderGo
            void* fCol = g_pfn_class_get_field_from_name(klassWallBreaker, "castMemberStayColliderGo");
            if (fCol) {
                void* colGo = nullptr;
                g_pfn_field_get_value(wallBreakerInst, fCol, &colGo);
                if (colGo) {
                    SetObjectActive(colGo, false);
                    ReFixLog("[IL2CPP] WallBreaker.castMemberStayColliderGo set inactive");
                }
            }

            // 6. Deactivate launch camera (cam)
            void* fCam = g_pfn_class_get_field_from_name(klassWallBreaker, "cam");
            if (fCam) {
                void* camInst = nullptr;
                g_pfn_field_get_value(wallBreakerInst, fCam, &camInst);
                if (camInst) {
                    SetObjectActive(camInst, false);
                    ReFixLog("[IL2CPP] WallBreaker.cam set inactive");
                }
            }
        }
    }

    // 7. Deactivate the WallBreaker GameObject itself so the 3D NemoLand menu is unobstructed
    SetObjectActive(wallBreakerInst, false);
    ReFixLog("[IL2CPP] WallBreaker GameObject set inactive");
}

static void CompleteLoginFlowTree(void* loaderInst) {
    if (!loaderInst) return;
    void* klassLoader = FindClassAcrossImages("", "MenuSceneLoader");
    if (!klassLoader) klassLoader = FindClassAcrossImages("Recreate", "MenuSceneLoader");
    if (!klassLoader) return;

    if (g_pfn_class_get_field_from_name && g_pfn_field_get_value) {
        void* fTree = g_pfn_class_get_field_from_name(klassLoader, "m_LoginFlowTree");
        if (fTree) {
            void* treeInst = nullptr;
            g_pfn_field_get_value(loaderInst, fTree, &treeInst);
            if (treeInst) {
                ReFixLog("[IL2CPP] Found LoginFlowTree instance %p on MenuSceneLoader", treeInst);
                void* klassTree = FindClassAcrossImages("Recreate.LoginFlow", "LoginFlowTree");
                if (klassTree) {
                    void* methodOnTreeComp = g_pfn_class_get_method_from_name(klassTree, "OnTreeCompleted", 1);
                    if (IsMethodCallable(methodOnTreeComp)) {
                        int state = 5; // TaskStatus.RanToCompletion
                        void* args[1] = { &state };
                        void* exc = nullptr;
                        __try {
                            g_pfn_runtime_invoke(methodOnTreeComp, treeInst, args, &exc);
                            ReFixLog("[IL2CPP] LoginFlowTree.OnTreeCompleted(5) invoked (exc=%p)", exc);
                        } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            }
        }
    }
}

static bool TransitionToMainMenuInternal() {
    EnsureThreadAttached();

    bool anySuccess = false;

    void* loaderInst = g_pMenuSceneLoaderInstance.load();
    if (!loaderInst) {
        loaderInst = GetMenuSceneLoaderInstance();
    }

    // 1. Setup MenuSceneLoader flags & complete login flow tree
    if (loaderInst) {
        ReFixLog("[IL2CPP] Transition: Configuring MenuSceneLoader flags on %p", loaderInst);
        SetCanEnterNemoLandOnInstance(loaderInst, true);
        CompleteLoginFlowTree(loaderInst);
    }

    // 2. Dispatch UiPhase Departure (10 / 5) and EnterNemoLand (12 / 6) to all registered callbacks
    ReFixLog("[IL2CPP] Transition: Dispatching UiPhase Departure (10/5) and EnterNemoLand (12/6)...");
    DispatchUiPhaseToAllCallbacks(10);
    DispatchUiPhaseToAllCallbacks(5);
    DispatchUiPhaseToAllCallbacks(12);
    DispatchUiPhaseToAllCallbacks(6);

    // 3. Handle WallBreaker (PlayEnterAnimationAsync, disable camera, disable white background)
    void* wbInst = GetWallBreakerInstance();
    if (wbInst) {
        TransitionWallBreaker(wbInst);
        anySuccess = true;
    }

    // 4. Cleanly deactivate all launch UI canvas overlays so 3D scene is unobstructed
    __try {
        void* depView = FindLiveObjectInScene("HeoUi.Launch.Departure.View", "DeparturePaneUiView");
        if (depView) {
            SetObjectActive(depView, false);
            ReFixLog("[IL2CPP] DeparturePaneUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* splashView = FindLiveObjectInScene("HeoUi.Launch.Splash.View", "SplashUiView");
        if (splashView) {
            SetObjectActive(splashView, false);
            ReFixLog("[IL2CPP] SplashUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* srvView = FindLiveObjectInScene("HeoUi.Launch.SelectServer.View", "SelectServerPaneUiView");
        if (srvView) {
            SetObjectActive(srvView, false);
            ReFixLog("[IL2CPP] SelectServerPaneUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* progView = FindLiveObjectInScene("HeoUi.Launch.TextProgress.View", "TextProgressPaneUiView");
        if (progView) {
            SetObjectActive(progView, false);
            ReFixLog("[IL2CPP] TextProgressPaneUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* launchView = FindLiveObjectInScene("HeoUi.Launch.View", "LaunchUiView");
        if (launchView) {
            SetObjectActive(launchView, false);
            ReFixLog("[IL2CPP] LaunchUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // 5. Connect to NemoLandNavigatorBridge and trigger main menu hall zone navigation
    ReFixLog("[IL2CPP] Transition: Resolving NemoLandNavigatorBridge.get_bridge()...");
    void* klassNav = FindClassAcrossImages("Recreate", "NemoLandNavigatorBridge");
    if (klassNav) {
        if (g_pfn_runtime_class_init) g_pfn_runtime_class_init(klassNav);

        void* methodGetBridge = g_pfn_class_get_method_from_name(klassNav, "get_bridge", 0);
        if (IsMethodCallable(methodGetBridge)) {
            void* excBridge = nullptr;
            void* bridgeInst = nullptr;
            __try {
                bridgeInst = g_pfn_runtime_invoke(methodGetBridge, nullptr, nullptr, &excBridge);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ReFixLog("[IL2CPP] Exception calling NemoLandNavigatorBridge.get_bridge");
            }

            if (!excBridge && bridgeInst) {
                ReFixLog("[IL2CPP] NemoLandNavigatorBridge.get_bridge() returned instance %p", bridgeInst);

                void* klassConcrete = g_pfn_object_get_class ? g_pfn_object_get_class(bridgeInst) : klassNav;
                void* callLoadHall = g_pfn_class_get_method_from_name(klassConcrete, "LoadHallScene", 0);
                if (!callLoadHall) callLoadHall = g_pfn_class_get_method_from_name(klassNav, "LoadHallScene", 0);

                if (IsMethodCallable(callLoadHall)) {
                    void* excL = nullptr;
                    __try {
                        g_pfn_runtime_invoke(callLoadHall, bridgeInst, nullptr, &excL);
                        if (!excL) {
                            ReFixLog("[IL2CPP] NemoLandNavigatorBridge.LoadHallScene() executed successfully");
                            anySuccess = true;
                        } else {
                            ReFixLog("[IL2CPP] NemoLandNavigatorBridge.LoadHallScene() returned exception %p", excL);
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        ReFixLog("[IL2CPP] Exception invoking NemoLandNavigatorBridge.LoadHallScene()");
                    }
                }

                void* callGoRoot = g_pfn_class_get_method_from_name(klassConcrete, "GoToRootZone", 0);
                if (!callGoRoot) callGoRoot = g_pfn_class_get_method_from_name(klassNav, "GoToRootZone", 0);

                if (IsMethodCallable(callGoRoot)) {
                    void* excR = nullptr;
                    __try {
                        g_pfn_runtime_invoke(callGoRoot, bridgeInst, nullptr, &excR);
                        if (!excR) {
                            ReFixLog("[IL2CPP] NemoLandNavigatorBridge.GoToRootZone() executed successfully");
                            anySuccess = true;
                        } else {
                            ReFixLog("[IL2CPP] NemoLandNavigatorBridge.GoToRootZone() returned exception %p", excR);
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        ReFixLog("[IL2CPP] Exception invoking NemoLandNavigatorBridge.GoToRootZone()");
                    }
                }
            }
        }
    }

    // 6. Switch music to Hall via SoundManager
    ReFixLog("[IL2CPP] Transition: Switching SoundManager to Hall...");
    void* soundInst = g_pSoundManagerInstance.load();
    void* klassSound = FindClassAcrossImages("", "SoundManager");
    if (!klassSound) klassSound = FindClassAcrossImages("Recreate", "SoundManager");
    if (klassSound) {
        void* methodPlayHall = g_pfn_class_get_method_from_name(klassSound, "PlayHallOrRoomMusic", 0);
        if (IsMethodCallable(methodPlayHall)) {
            void* exc = nullptr;
            __try {
                g_pfn_runtime_invoke(methodPlayHall, nullptr, nullptr, &exc);
                ReFixLog("[IL2CPP] SoundManager.PlayHallOrRoomMusic() executed");
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        if (soundInst) {
            void* methodEnterHall = g_pfn_class_get_method_from_name(klassSound, "EnterHall", 0);
            if (IsMethodCallable(methodEnterHall)) {
                void* exc = nullptr;
                __try {
                    g_pfn_runtime_invoke(methodEnterHall, soundInst, nullptr, &exc);
                    ReFixLog("[IL2CPP] SoundManager.EnterHall() executed on instance %p", soundInst);
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }

    // 7. Cleanly dismiss modal progress overlay and launch UI canvas so the 3D NemoLand Hall is rendered
    __try {
        void* progView = FindLiveObjectInScene("HeoUi.Launch.TextProgress.View", "TextProgressPaneUiView");
        if (progView) {
            SetObjectActive(progView, false);
            ReFixLog("[IL2CPP] TextProgressPaneUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        void* launchView = FindLiveObjectInScene("HeoUi.Launch.View", "LaunchUiView");
        if (launchView) {
            SetObjectActive(launchView, false);
            ReFixLog("[IL2CPP] LaunchUiView set inactive");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // 8. Final UiPhase Destroy (14 / 7) dispatch
    DispatchUiPhaseToAllCallbacks(14);
    DispatchUiPhaseToAllCallbacks(7);

    ReFixLog("[IL2CPP] TransitionToMainMenuInternal completed (anySuccess=%s)", anySuccess ? "true" : "false");
    return anySuccess;
}

// Master connection function: called on server confirm/apply or depart
static void ConnectToP2PAndEnterMainMenu(void* selectServerSelf) {
    if (s_hasTransitioned.exchange(true)) return; // Prevent double transition

    ReFixLog("[IL2CPP] ===================================================================");
    ReFixLog("[IL2CPP] Master Connection Triggered: Bypassing AWS cloud -> Steam P2P SDR");
    ReFixLog("[IL2CPP] ===================================================================");

    // 1. Enable local P2P mode & fake data
    ActivateP2PModeInternal();

    // 2. Close SelectServerPaneUi cleanly
    if (selectServerSelf) {
        void* klassSelectServer = FindClassAcrossImages("HeoUi.Launch.SelectServer", "SelectServerPaneUi");
        if (klassSelectServer) {
            void* methodHide = g_pfn_class_get_method_from_name(klassSelectServer, "Hide", 0);
            if (methodHide) {
                void* excH = nullptr;
                __try {
                    g_pfn_runtime_invoke(methodHide, selectServerSelf, nullptr, &excH);
                    ReFixLog("[IL2CPP] SelectServerPaneUi.Hide() executed");
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }

            if (g_pfn_class_get_field_from_name && g_pfn_field_get_value) {
                void* fieldView = g_pfn_class_get_field_from_name(klassSelectServer, "m_View");
                if (fieldView) {
                    void* viewInst = nullptr;
                    g_pfn_field_get_value(selectServerSelf, fieldView, &viewInst);
                    if (viewInst) {
                        SetObjectActive(viewInst, false);
                        ReFixLog("[IL2CPP] SelectServerPaneUi.m_View set inactive");
                    }
                }
            }
        }
    }

    // 3. Transition to main menu
    TransitionToMainMenuInternal();
}

// Hooked SoundManager.FixedUpdate - executes queued transition on Unity main thread
static void Hooked_SoundManager_FixedUpdate(void* self, void* method) {
    if (self) g_pSoundManagerInstance.store(self);

    if (g_orig_SoundManager_FixedUpdate) {
        __try {
            g_orig_SoundManager_FixedUpdate(self, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (s_pendingMainThreadTransition.exchange(false)) {
        ReFixLog("[IL2CPP] Executing queued P2P transition on Unity main thread (via SoundManager.FixedUpdate)...");
        ConnectToP2PAndEnterMainMenu(g_pSelectServerPaneUiInstance.load());
    }
}

// Hooked WallBreaker.Awake - captures active WallBreaker instance
static void Hooked_WallBreaker_Awake(void* self, void* method) {
    if (self) {
        g_pWallBreakerInstance.store(self);
        ReFixLog("[IL2CPP] >>> WallBreaker.Awake: Captured instance %p <<<", self);
    }
    if (g_orig_WallBreaker_Awake) {
        __try {
            g_orig_WallBreaker_Awake(self, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Exception absorbed in WallBreaker.Awake");
        }
    }
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

// Hooked MenuSceneLoader.RequestInterruption - captures instance and forwards to game UI
static void Hooked_MenuSceneLoader_RequestInterruption(void* self, void* interruption, void* arg2, void* method) {
    if (self) g_pMenuSceneLoaderInstance.store(self);
    s_interruptionSeen.store(true);
    s_interruptionTime.store(GetTickCount());
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
    if (self) g_pSelectServerPaneUiInstance.store(self);
    ReFixLog("[IL2CPP] >>> SelectServerPaneUi.OnApplyButtonClick (Confirm) clicked! <<<");
    ConnectToP2PAndEnterMainMenu(self);
}

// Hooked SelectServerPaneUi.OnCellViewSelect
// When user clicks/selects ANY server row in the list, update visual selection (selection highlight / radio button)
static void Hooked_SelectServer_OnCellViewSelect(void* self, void* cellView, int dataIndex, void* method) {
    if (self) g_pSelectServerPaneUiInstance.store(self);
    ReFixLog("[IL2CPP] >>> SelectServerPaneUi.OnCellViewSelect: row %d selected <<<", dataIndex);

    // Call original first to let the UI update its selection highlight
    if (g_orig_SelectServer_OnCellViewSelect) {
        __try {
            g_orig_SelectServer_OnCellViewSelect(self, cellView, dataIndex, method);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ReFixLog("[IL2CPP] Handled exception in SelectServerPaneUi.OnCellViewSelect");
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
    g_pfn_class_get_field_from_name = (fn_il2cpp_class_get_field_from_name)GetProcAddress(hGA, "il2cpp_class_get_field_from_name");
    g_pfn_field_static_get_value = (fn_il2cpp_field_static_get_value)GetProcAddress(hGA, "il2cpp_field_static_get_value");
    g_pfn_field_static_set_value = (fn_il2cpp_field_static_set_value)GetProcAddress(hGA, "il2cpp_field_static_set_value");
    g_pfn_field_get_value = (fn_il2cpp_field_get_value)GetProcAddress(hGA, "il2cpp_field_get_value");
    g_pfn_field_set_value = (fn_il2cpp_field_set_value)GetProcAddress(hGA, "il2cpp_field_set_value");
    g_pfn_field_get_offset = (fn_il2cpp_field_get_offset)GetProcAddress(hGA, "il2cpp_field_get_offset");
    g_pfn_object_get_class = (fn_il2cpp_object_get_class)GetProcAddress(hGA, "il2cpp_object_get_class");
    g_pfn_runtime_class_init = (fn_il2cpp_runtime_class_init)GetProcAddress(hGA, "il2cpp_runtime_class_init");
    g_pfn_class_is_assignable_from = (fn_il2cpp_class_is_assignable_from)GetProcAddress(hGA, "il2cpp_class_is_assignable_from");
    g_pfn_format_exception = (fn_il2cpp_format_exception)GetProcAddress(hGA, "il2cpp_format_exception");
    g_pfn_string_new = (fn_il2cpp_string_new)GetProcAddress(hGA, "il2cpp_string_new");
    g_pfn_object_get_virtual_method = (fn_il2cpp_object_get_virtual_method)GetProcAddress(hGA, "il2cpp_object_get_virtual_method");
    g_pfn_class_get_methods = (fn_il2cpp_class_get_methods)GetProcAddress(hGA, "il2cpp_class_get_methods");
    g_pfn_method_get_name = (fn_il2cpp_method_get_name)GetProcAddress(hGA, "il2cpp_method_get_name");

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
            void* pCode = ResolveRealCodePointer(*(void**)methodAwake);
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_Awake, (void**)&g_orig_MenuSceneLoader_Awake) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.Awake at %p", pCode);
            }
        }

        // RequestInterruption (try 2 args, fallback to 1)
        void* methodReqInterr = g_pfn_class_get_method_from_name(klassLoader, "RequestInterruption", 2);
        if (!methodReqInterr) methodReqInterr = g_pfn_class_get_method_from_name(klassLoader, "RequestInterruption", 1);
        if (methodReqInterr) {
            void* pCode = ResolveRealCodePointer(*(void**)methodReqInterr);
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_MenuSceneLoader_RequestInterruption, (void**)&g_orig_MenuSceneLoader_RequestInterruption) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked MenuSceneLoader.RequestInterruption at %p", pCode);
            }
        }
    }

    // 4. Hook WallBreaker.Awake (param_count = 0)
    void* klassWallBreaker = FindClassAcrossImages("HeoUi.Launch", "WallBreaker");
    if (klassWallBreaker) {
        void* methodWbAwake = g_pfn_class_get_method_from_name(klassWallBreaker, "Awake", 0);
        if (methodWbAwake) {
            void* pCode = ResolveRealCodePointer(*(void**)methodWbAwake);
            if (pCode && MH_CreateHook(pCode, (void*)Hooked_WallBreaker_Awake, (void**)&g_orig_WallBreaker_Awake) == MH_OK) {
                MH_EnableHook(pCode);
                ReFixLog("[IL2CPP] Hooked WallBreaker.Awake at %p", pCode);
            }
        }
    }

    // 5. Hook SelectServerPaneUi.OnApplyButtonClick (param_count = 0)
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

    // 6. Hook SoundManager.FixedUpdate (param_count = 0) to drive main-thread updates
    void* klassSound = FindClassAcrossImages("", "SoundManager");
    if (!klassSound) klassSound = FindClassAcrossImages("Recreate", "SoundManager");
    if (klassSound) {
        void* methodFixed = g_pfn_class_get_method_from_name(klassSound, "FixedUpdate", 0);
        if (methodFixed) {
            void* pCode = ResolveRealCodePointer(*(void**)methodFixed);
            if (pCode) {
                if (MH_CreateHook(pCode, (void*)Hooked_SoundManager_FixedUpdate, (void**)&g_orig_SoundManager_FixedUpdate) == MH_OK) {
                    MH_EnableHook(pCode);
                    ReFixLog("[IL2CPP] Hooked SoundManager.FixedUpdate at %p", pCode);
                }
            }
        }
    }

    g_initialized.store(true);
    ReFixLog("[IL2CPP] Initialization and hook installation completed successfully.");

    // Auto-transition watchdog: if interruption modal dialog is shown and user doesn't click for 25 seconds,
    // transition automatically to ensure automated tests / idle users advance smoothly.
    while (g_watcherRunning.load()) {
        Sleep(250);
        if (s_hasTransitioned.load()) break;

        if (s_interruptionSeen.load()) {
            DWORD elapsed = GetTickCount() - s_interruptionTime.load();
            if (elapsed > 25000 && !s_hasTransitioned.load()) {
                ReFixLog("[IL2CPP] Auto-transition watchdog triggered (elapsed=%lu ms) -> queueing main-thread NemoLand entrance", elapsed);
                s_pendingMainThreadTransition.store(true);

                // Wait up to 2 seconds for FixedUpdate to execute the transition on the main thread
                for (int w = 0; w < 8 && !s_hasTransitioned.load(); w++) {
                    Sleep(250);
                }

                // If still not executed (e.g. SoundManager not running FixedUpdate yet), fallback to direct call
                if (!s_hasTransitioned.load()) {
                    ReFixLog("[IL2CPP] Watchdog fallback: executing direct transition from watcher thread...");
                    ConnectToP2PAndEnterMainMenu(g_pSelectServerPaneUiInstance.load());
                }
                break;
            }
        }
    }

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

    // Process-wide single instance guard: ensure only ONE steam_api64.dll instance manages IL2CPP hooks
    char mutexName[64];
    snprintf(mutexName, sizeof(mutexName), "Local\\ReFix_IL2CPP_Master_%lu", GetCurrentProcessId());
    s_hIL2CPPMasterMutex = CreateMutexA(NULL, TRUE, mutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ReFixLog("[IL2CPP] Secondary DLL instance in PID %lu detected; skipping duplicate IL2CPP initialization.", GetCurrentProcessId());
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
    if (s_hIL2CPPMasterMutex) {
        CloseHandle(s_hIL2CPPMasterMutex);
        s_hIL2CPPMasterMutex = nullptr;
    }
}

void Update() {
    if (s_pendingMainThreadTransition.exchange(false)) {
        ReFixLog("[IL2CPP] Executing queued P2P transition on Unity main thread (via Update)...");
        ConnectToP2PAndEnterMainMenu(g_pSelectServerPaneUiInstance.load());
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
