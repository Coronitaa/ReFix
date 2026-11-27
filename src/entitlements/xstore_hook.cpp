#include "xstore_hook.h"
#include "entitlements_store.h"
#include "../minhook/MinHook.h"
#include "../overlay/refix_overlay.h"

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

extern void ReFixLog(const char* fmt, ...);

namespace refix::entitlements {
namespace {

struct XStorePriceLayout {
    float basePrice;
    float price;
    float recurrencePrice;
    const char* currencyCode;
    char formattedBasePrice[16];
    char formattedPrice[16];
    char formattedRecurrencePrice[16];
    bool isOnSale;
    int64_t saleEndDate;
};

#ifdef _WIN64
static_assert(sizeof(XStorePriceLayout) == 0x58, "XStorePriceLayout must match GDK XStorePrice size");
#endif

struct XStoreProductLayout {
    const char* storeId;
    const char* title;
    const char* description;
    const char* language;
    const char* inAppOfferToken;
    char* linkUri;
    uint32_t productKind;
    XStorePriceLayout price;
    bool hasDigitalDownload;
    bool isInUserCollection;
    uint32_t keywordsCount;
    const char** keywords;
    uint32_t skusCount;
    void* skus;
    uint32_t imagesCount;
    void* images;
    uint32_t videosCount;
    void* videos;
};
#ifdef _WIN64
static_assert(sizeof(XStoreProductLayout) == 0xD0, "XStoreProductLayout must match GDK XStoreProduct size");
#endif

struct XStoreGameLicenseLayout {
    char skuStoreId[18];
    bool isActive;
    bool isTrialOwnedByThisUser;
    bool isDiscLicense;
    bool isTrial;
    uint32_t trialTimeRemainingInSeconds;
    char trialUniqueId[64];
    int64_t expirationDate;
};

struct XStoreAddonLicenseLayout {
    char skuStoreId[18];
    char inAppOfferToken[64];
    bool isActive;
    int64_t expirationDate;
};

struct XStoreCanAcquireLicenseResultLayout {
    char licensableSku[18];
    uint32_t status; // 1 = Licensable
};

struct XAsyncBlockLayout {
    void* queue;
    void* context;
    void (WINAPI* callback)(XAsyncBlockLayout* async);
    unsigned char internal[sizeof(void*) * 4];
};

using Fn_XTaskQueueSubmitCallback = HRESULT(WINAPI*)(void* queue, uint32_t port, void* callbackContext, void(WINAPI* callback)(void*, bool));
using Fn_XStoreIsLicenseValid = bool(WINAPI*)(void* licenseHandle);
using Fn_XStoreProductQueryCallback = bool(WINAPI*)(const XStoreProductLayout* product, void* context);
using Fn_XStoreEnumerateProductsQuery = HRESULT(WINAPI*)(void* queryHandle, void* context, Fn_XStoreProductQueryCallback callback);
using Fn_XStoreQueryGameLicenseResult = HRESULT(WINAPI*)(void* async, XStoreGameLicenseLayout* license);
using Fn_XStoreQueryAddOnLicensesResult = HRESULT(WINAPI*)(void* async, uint32_t count, XStoreAddonLicenseLayout* addOnLicenses);
using Fn_XStoreCanAcquireLicenseResult = HRESULT(WINAPI*)(void* async, XStoreCanAcquireLicenseResultLayout* result);
using Fn_XStoreCreateContext = HRESULT(WINAPI*)(void* user, void** storeContextHandle);
using Fn_XStoreQueryProductsResult = HRESULT(WINAPI*)(void* async, void** productQueryHandle);
using Fn_XStoreQueryAddOnLicensesResultCount = HRESULT(WINAPI*)(void* async, uint32_t* count);
using Fn_XStoreAcquireLicenseResult = HRESULT(WINAPI*)(void* async, void** storeLicenseHandle);
using Fn_XStoreShowPurchaseUIAsync = HRESULT(WINAPI*)(void* storeContext, const char* storeId, const char* name, const char* extendedJson, XAsyncBlockLayout* async);
using Fn_XStoreShowPurchaseUIResult = HRESULT(WINAPI*)(XAsyncBlockLayout* async);

Fn_XTaskQueueSubmitCallback g_submitTaskCallback = nullptr;
Fn_XStoreCreateContext g_origCreateContext = nullptr;
Fn_XStoreIsLicenseValid g_origIsLicenseValid = nullptr;
Fn_XStoreEnumerateProductsQuery g_origEnumProducts = nullptr;
Fn_XStoreQueryProductsResult g_origQueryAssociatedResult = nullptr;
Fn_XStoreQueryProductsResult g_origQueryEntitledResult = nullptr;
Fn_XStoreQueryProductsResult g_origQueryProductsResult = nullptr;
Fn_XStoreQueryGameLicenseResult g_origQueryGameLicenseResult = nullptr;
Fn_XStoreQueryAddOnLicensesResultCount g_origQueryAddOnCount = nullptr;
Fn_XStoreQueryAddOnLicensesResult g_origQueryAddOnResult = nullptr;
Fn_XStoreAcquireLicenseResult g_origAcquireDurablesResult = nullptr;
Fn_XStoreAcquireLicenseResult g_origAcquirePackageResult = nullptr;
Fn_XStoreCanAcquireLicenseResult g_origCanAcquireStoreIdResult = nullptr;
Fn_XStoreCanAcquireLicenseResult g_origCanAcquirePackageResult = nullptr;
Fn_XStoreShowPurchaseUIAsync g_origShowPurchaseUIAsync = nullptr;
Fn_XStoreShowPurchaseUIResult g_origShowPurchaseUIResult = nullptr;

std::atomic<bool> g_hooksInstalled{ false };
std::mutex g_installMu;
std::mutex g_purchaseMu;
std::unordered_map<XAsyncBlockLayout*, HRESULT> g_purchaseResults;
void* g_dllNotifyCookie = nullptr;
HANDLE g_dllLoadEvent = nullptr;

void WINAPI CompletePurchaseOnQueue(void* ctx, bool) {
    auto* async = static_cast<XAsyncBlockLayout*>(ctx);
    if (async && async->callback) {
        async->callback(async);
    }
}

bool WINAPI Hook_XStoreIsLicenseValid(void* licenseHandle) {
    if (g_origIsLicenseValid) {
        (void)g_origIsLicenseValid(licenseHandle);
    }
    return true;
}

struct EnumWrapCtx {
    void* userCtx;
    Fn_XStoreProductQueryCallback userCb;
};

bool WINAPI WrappedProductCallback(const XStoreProductLayout* product, void* rawCtx) {
    auto* ctx = static_cast<EnumWrapCtx*>(rawCtx);
    if (!ctx || !ctx->userCb) return false;
    if (!product) return ctx->userCb(nullptr, ctx->userCtx);
    XStoreProductLayout copy = *product;
    if (!copy.isInUserCollection) {
        ReFixLog("[XStoreHook] Unlocking product '%s' (%s)", copy.title ? copy.title : "", copy.storeId ? copy.storeId : "");
        copy.isInUserCollection = true;
    }
    return ctx->userCb(&copy, ctx->userCtx);
}

HRESULT WINAPI Hook_XStoreEnumerateProductsQuery(void* queryHandle, void* context, Fn_XStoreProductQueryCallback callback) {
    if (!g_origEnumProducts) return E_FAIL;
    if (!callback) return g_origEnumProducts(queryHandle, context, callback);
    EnumWrapCtx wrap{ context, callback };
    return g_origEnumProducts(queryHandle, &wrap, WrappedProductCallback);
}

HRESULT WINAPI Hook_XStoreQueryGameLicenseResult(void* async, XStoreGameLicenseLayout* license) {
    HRESULT hr = g_origQueryGameLicenseResult ? g_origQueryGameLicenseResult(async, license) : E_FAIL;
    if (SUCCEEDED(hr) && license) {
        license->isActive = true;
        license->isTrial = false;
    }
    return hr;
}

HRESULT WINAPI Hook_XStoreCreateContext(void* user, void** storeContextHandle) {
    HRESULT hr = g_origCreateContext ? g_origCreateContext(user, storeContextHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreCreateContext(user=%p) -> hr=0x%08X", user, (unsigned)hr);
    return hr;
}

HRESULT WINAPI Hook_XStoreQueryAssociatedProductsResult(void* async, void** productQueryHandle) {
    HRESULT hr = g_origQueryAssociatedResult ? g_origQueryAssociatedResult(async, productQueryHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreQueryAssociatedProductsResult -> hr=0x%08X handle=%p",
             (unsigned)hr, productQueryHandle ? *productQueryHandle : nullptr);
    return hr;
}

HRESULT WINAPI Hook_XStoreQueryEntitledProductsResult(void* async, void** productQueryHandle) {
    HRESULT hr = g_origQueryEntitledResult ? g_origQueryEntitledResult(async, productQueryHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreQueryEntitledProductsResult -> hr=0x%08X handle=%p",
             (unsigned)hr, productQueryHandle ? *productQueryHandle : nullptr);
    return hr;
}

HRESULT WINAPI Hook_XStoreQueryProductsResult(void* async, void** productQueryHandle) {
    HRESULT hr = g_origQueryProductsResult ? g_origQueryProductsResult(async, productQueryHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreQueryProductsResult -> hr=0x%08X handle=%p",
             (unsigned)hr, productQueryHandle ? *productQueryHandle : nullptr);
    return hr;
}

HRESULT WINAPI Hook_XStoreQueryAddOnLicensesResultCount(void* async, uint32_t* count) {
    HRESULT hr = g_origQueryAddOnCount ? g_origQueryAddOnCount(async, count) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreQueryAddOnLicensesResultCount -> hr=0x%08X count=%u",
             (unsigned)hr, count ? *count : 0u);
    return hr;
}

HRESULT WINAPI Hook_XStoreQueryAddOnLicensesResult(void* async, uint32_t count, XStoreAddonLicenseLayout* addOnLicenses) {
    HRESULT hr = g_origQueryAddOnResult ? g_origQueryAddOnResult(async, count, addOnLicenses) : E_FAIL;
    if (SUCCEEDED(hr) && addOnLicenses) {
        for (uint32_t i = 0; i < count; ++i) {
            addOnLicenses[i].isActive = true;
            ReFixLog("[XStoreHook] Activated addon license sku='%s'", addOnLicenses[i].skuStoreId);
        }
    }
    return hr;
}

HRESULT WINAPI Hook_XStoreAcquireDurablesResult(void* async, void** storeLicenseHandle) {
    HRESULT hr = g_origAcquireDurablesResult ? g_origAcquireDurablesResult(async, storeLicenseHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreAcquireLicenseForDurablesResult -> hr=0x%08X", (unsigned)hr);
    return hr;
}

HRESULT WINAPI Hook_XStoreAcquirePackageResult(void* async, void** storeLicenseHandle) {
    HRESULT hr = g_origAcquirePackageResult ? g_origAcquirePackageResult(async, storeLicenseHandle) : E_FAIL;
    ReFixLog("[XStoreHook] XStoreAcquireLicenseForPackageResult -> hr=0x%08X", (unsigned)hr);
    return hr;
}

HRESULT WINAPI Hook_XStoreCanAcquireStoreIdResult(void* async, XStoreCanAcquireLicenseResultLayout* result) {
    HRESULT hr = g_origCanAcquireStoreIdResult ? g_origCanAcquireStoreIdResult(async, result) : E_FAIL;
    if (SUCCEEDED(hr) && result) result->status = 1;
    return hr;
}

HRESULT WINAPI Hook_XStoreCanAcquirePackageResult(void* async, XStoreCanAcquireLicenseResultLayout* result) {
    HRESULT hr = g_origCanAcquirePackageResult ? g_origCanAcquirePackageResult(async, result) : E_FAIL;
    if (SUCCEEDED(hr) && result) result->status = 1;
    return hr;
}

HRESULT WINAPI Hook_XStoreShowPurchaseUIAsync(void* storeContext, const char* storeId, const char* name, const char* extendedJson, XAsyncBlockLayout* async) {
    if (!async) {
        return g_origShowPurchaseUIAsync ? g_origShowPurchaseUIAsync(storeContext, storeId, name, extendedJson, async) : E_INVALIDARG;
    }

    std::string idStr = storeId ? storeId : "";
    std::string nameStr = (name && name[0]) ? name : (idStr.empty() ? "Microsoft Store Item" : idStr);
    std::string descStr = idStr.empty() ? "Microsoft Store Add-on" : ("StoreId: " + idStr);

    {
        std::lock_guard<std::mutex> lock(g_purchaseMu);
        g_purchaseResults[async] = E_PENDING;
    }

    ReFixLog("[XStoreHook] Intercepted XStoreShowPurchaseUIAsync: storeId='%s' name='%s'", idStr.c_str(), nameStr.c_str());

    auto completeAsync = [async](bool approved) {
        HRESULT res = approved ? S_OK : HRESULT_FROM_WIN32(ERROR_CANCELLED);
        {
            std::lock_guard<std::mutex> lock(g_purchaseMu);
            g_purchaseResults[async] = res;
        }
        if (async->callback) {
            if (async->queue && g_submitTaskCallback) {
                // Port 1 = XTaskQueuePort::Completion
                g_submitTaskCallback(async->queue, 1, async, CompletePurchaseOnQueue);
            } else {
                async->callback(async);
            }
        }
    };

    if (!ReFixOverlay::IsEnabled()) {
        completeAsync(true);
        return S_OK;
    }

    ReFixOverlay::PromptPurchase(0, 0, nameStr, descStr, false, completeAsync);
    return S_OK;
}

HRESULT WINAPI Hook_XStoreShowPurchaseUIResult(XAsyncBlockLayout* async) {
    if (async) {
        std::lock_guard<std::mutex> lock(g_purchaseMu);
        auto it = g_purchaseResults.find(async);
        if (it != g_purchaseResults.end()) {
            HRESULT hr = it->second;
            if (hr != E_PENDING) {
                g_purchaseResults.erase(it);
            }
            return hr;
        }
    }
    return g_origShowPurchaseUIResult ? g_origShowPurchaseUIResult(async) : E_FAIL;
}

template <typename Fn>
bool TryHookExport(HMODULE mod, const char* name, void* hook, Fn* origOut) {
    void* addr = (void*)GetProcAddress(mod, name);
    if (!addr) return false;
    if (MH_CreateHook(addr, hook, (void**)origOut) == MH_OK) {
        MH_EnableHook(addr);
        return true;
    }
    return false;
}

bool TryInstallForLoadedModules() {
    if (g_hooksInstalled.load(std::memory_order_acquire)) return true;
    std::lock_guard<std::mutex> lock(g_installMu);
    if (g_hooksInstalled.load(std::memory_order_acquire)) return true;

    // XGamingRuntimeThunks.dll / XGamingRuntime.dll expose the flat XStore* C exports;
    // System32 xgameruntime.dll only exposes COM internals and has no XStore* exports.
    const char* candidates[] = { "XGamingRuntimeThunks.dll", "XGamingRuntime.dll" };
    HMODULE hMod = nullptr;
    for (const char* dllName : candidates) {
        HMODULE m = GetModuleHandleA(dllName);
        if (m && (GetProcAddress(m, "XStoreEnumerateProductsQuery") || GetProcAddress(m, "XStoreIsLicenseValid"))) {
            hMod = m;
            break;
        }
    }
    if (!hMod) return false;

    MH_Initialize();
    g_submitTaskCallback = (Fn_XTaskQueueSubmitCallback)GetProcAddress(hMod, "XTaskQueueSubmitCallback");

    int count = 0;
    if (TryHookExport(hMod, "XStoreCreateContext", (void*)Hook_XStoreCreateContext, &g_origCreateContext)) ++count;
    if (TryHookExport(hMod, "XStoreIsLicenseValid", (void*)Hook_XStoreIsLicenseValid, &g_origIsLicenseValid)) ++count;
    if (TryHookExport(hMod, "XStoreEnumerateProductsQuery", (void*)Hook_XStoreEnumerateProductsQuery, &g_origEnumProducts)) ++count;
    if (TryHookExport(hMod, "XStoreQueryAssociatedProductsResult", (void*)Hook_XStoreQueryAssociatedProductsResult, &g_origQueryAssociatedResult)) ++count;
    if (TryHookExport(hMod, "XStoreQueryEntitledProductsResult", (void*)Hook_XStoreQueryEntitledProductsResult, &g_origQueryEntitledResult)) ++count;
    if (TryHookExport(hMod, "XStoreQueryProductsResult", (void*)Hook_XStoreQueryProductsResult, &g_origQueryProductsResult)) ++count;
    if (TryHookExport(hMod, "XStoreQueryGameLicenseResult", (void*)Hook_XStoreQueryGameLicenseResult, &g_origQueryGameLicenseResult)) ++count;
    if (TryHookExport(hMod, "XStoreQueryAddOnLicensesResultCount", (void*)Hook_XStoreQueryAddOnLicensesResultCount, &g_origQueryAddOnCount)) ++count;
    if (TryHookExport(hMod, "XStoreQueryAddOnLicensesResult", (void*)Hook_XStoreQueryAddOnLicensesResult, &g_origQueryAddOnResult)) ++count;
    if (TryHookExport(hMod, "XStoreAcquireLicenseForDurablesResult", (void*)Hook_XStoreAcquireDurablesResult, &g_origAcquireDurablesResult)) ++count;
    if (TryHookExport(hMod, "XStoreAcquireLicenseForPackageResult", (void*)Hook_XStoreAcquirePackageResult, &g_origAcquirePackageResult)) ++count;
    if (TryHookExport(hMod, "XStoreCanAcquireLicenseForStoreIdResult", (void*)Hook_XStoreCanAcquireStoreIdResult, &g_origCanAcquireStoreIdResult)) ++count;
    if (TryHookExport(hMod, "XStoreCanAcquireLicenseForPackageResult", (void*)Hook_XStoreCanAcquirePackageResult, &g_origCanAcquirePackageResult)) ++count;
    if (TryHookExport(hMod, "XStoreShowPurchaseUIAsync", (void*)Hook_XStoreShowPurchaseUIAsync, &g_origShowPurchaseUIAsync)) ++count;
    if (TryHookExport(hMod, "XStoreShowPurchaseUIResult", (void*)Hook_XStoreShowPurchaseUIResult, &g_origShowPurchaseUIResult)) ++count;

    if (count > 0) {
        g_hooksInstalled.store(true, std::memory_order_release);
        ReFixLog("[XStoreHook] Installed %d Microsoft Store DLC/purchase hook(s)", count);
        return true;
    }
    return false;
}

typedef VOID(CALLBACK* Fn_LdrDllNotification)(ULONG reason, const void* data, PVOID context);
using Fn_LdrRegisterDllNotification = LONG(NTAPI*)(ULONG flags, Fn_LdrDllNotification cb, PVOID ctx, PVOID* cookie);

VOID CALLBACK OnDllLoadedNotify(ULONG reason, const void*, PVOID) {
    // Wake the installer thread off the loader lock when a new DLL maps in.
    if (reason == 1 && !g_hooksInstalled.load(std::memory_order_acquire) && g_dllLoadEvent) {
        SetEvent(g_dllLoadEvent);
    }
}

} // namespace

void InstallXStoreHooks(bool enabled) {
    if (!enabled) return;
    if (TryInstallForLoadedModules()) return;

    if (!g_dllLoadEvent) {
        g_dllLoadEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    }
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll && !g_dllNotifyCookie) {
        auto regFn = reinterpret_cast<Fn_LdrRegisterDllNotification>(GetProcAddress(hNtdll, "LdrRegisterDllNotification"));
        if (regFn) {
            regFn(0, OnDllLoadedNotify, nullptr, &g_dllNotifyCookie);
        }
    }

    std::thread([]() {
        for (int i = 0; i < 120; ++i) {
            if (TryInstallForLoadedModules()) break;
            if (g_dllLoadEvent) {
                WaitForSingleObject(g_dllLoadEvent, 150);
            } else {
                Sleep(150);
            }
        }
    }).detach();
}

} // namespace refix::entitlements
