#include "refix_mono.h"
#include "overlay/refix_overlay.h"
#include "unreal_steam_emu.h"
#include "minhook/MinHook.h"
#include <windows.h>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

extern void ReFixLog(const char* fmt, ...);

namespace {

// Mono API function pointer types
typedef void* (*fn_mono_domain_get)();
typedef void* (*fn_mono_thread_attach)(void* domain);
typedef void (*fn_mono_assembly_foreach)(void (*func)(void* assembly, void* user_data), void* user_data);
typedef void* (*fn_mono_assembly_get_image)(void* assembly);
typedef const char* (*fn_mono_image_get_name)(void* image);
typedef void* (*fn_mono_class_from_name)(void* image, const char* name_space, const char* name);
typedef void* (*fn_mono_class_get_method_from_name)(void* klass, const char* name, int param_count);
typedef void* (*fn_mono_compile_method)(void* method);
typedef void* (*fn_mono_runtime_invoke)(void* method, void* obj, void** params, void** exc);
typedef void* (*fn_mono_class_get_field_from_name)(void* klass, const char* name);
typedef void (*fn_mono_field_get_value)(void* obj, void* field, void* value);
typedef void (*fn_mono_field_set_value)(void* obj, void* field, void* value);
typedef void (*fn_mono_field_static_set_value)(void* vtable, void* field, void* value);
typedef void* (*fn_mono_class_vtable)(void* domain, void* klass);
typedef void* (*fn_mono_object_new)(void* domain, void* klass);
typedef void (*fn_mono_runtime_object_init)(void* obj);
typedef void* (*fn_mono_string_new)(void* domain, const char* text);
typedef char* (*fn_mono_string_to_utf8)(void* string_obj);
typedef void (*fn_mono_free)(void* ptr);
typedef void* (*fn_mono_object_get_class)(void* obj);
typedef void* (*fn_mono_field_get_type)(void* field);
typedef void* (*fn_mono_class_from_mono_type)(void* type);
typedef void* (*fn_mono_array_new)(void* domain, void* eclass, uintptr_t n);

static fn_mono_domain_get g_pfn_domain_get = nullptr;
static fn_mono_thread_attach g_pfn_thread_attach = nullptr;
static fn_mono_assembly_foreach g_pfn_assembly_foreach = nullptr;
static fn_mono_assembly_get_image g_pfn_assembly_get_image = nullptr;
static fn_mono_image_get_name g_pfn_image_get_name = nullptr;
static fn_mono_class_from_name g_pfn_class_from_name = nullptr;
static fn_mono_class_get_method_from_name g_pfn_class_get_method_from_name = nullptr;
static fn_mono_compile_method g_pfn_compile_method = nullptr;
static fn_mono_runtime_invoke g_pfn_runtime_invoke = nullptr;
static fn_mono_class_get_field_from_name g_pfn_class_get_field_from_name = nullptr;
static fn_mono_field_get_value g_pfn_field_get_value = nullptr;
static fn_mono_field_set_value g_pfn_field_set_value = nullptr;
static fn_mono_field_static_set_value g_pfn_field_static_set_value = nullptr;
static fn_mono_class_vtable g_pfn_class_vtable = nullptr;
static fn_mono_object_new g_pfn_object_new = nullptr;
static fn_mono_runtime_object_init g_pfn_runtime_object_init = nullptr;
static fn_mono_string_new g_pfn_string_new = nullptr;
static fn_mono_string_to_utf8 g_pfn_string_to_utf8 = nullptr;
static fn_mono_free g_pfn_free = nullptr;
static fn_mono_object_get_class g_pfn_object_get_class = nullptr;
static fn_mono_field_get_type g_pfn_field_get_type = nullptr;
static fn_mono_class_from_mono_type g_pfn_class_from_mono_type = nullptr;
static fn_mono_array_new g_pfn_array_new = nullptr;

static std::atomic<bool> g_initialized{false};
static void* g_assemblyCSharpImage = nullptr;
static void* g_blayFapClientClass = nullptr;
static void* g_fBaseUrl = nullptr;
static void* g_startReqClass = nullptr;
static void* g_startRespClass = nullptr;
static void* g_finishReqClass = nullptr;
static void* g_finishRespClass = nullptr;
static void* g_loginRespClass = nullptr;
static void* g_titleRespClass = nullptr;
static void* g_catalogRespClass = nullptr;
static void* g_catalogItemClass = nullptr;
static void* g_inventoryRespClass = nullptr;
static void* g_getTimeRespClass = nullptr;
static void* g_getUserDataRespClass = nullptr;
static void* g_setUserDataRespClass = nullptr;
static void* g_consumeItemRespClass = nullptr;
static void* g_consumeInventoryMethod = nullptr;

static std::atomic<uint64_t> g_nextOrderId{10001};
static std::unordered_map<uint64_t, std::string> g_pendingPurchases;
static std::string g_lastRequestedItemId;
static std::mutex g_purchaseMutex;

typedef void (__cdecl *fn_LoginWithSteam_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_GetTitleData_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_GetFilteredCatalog_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_GetUserInventory_t)(void* this_ptr, void* responseCallback);
typedef void (__cdecl *fn_ValidateSteamEntitlement_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_GetTime_t)(void* this_ptr, void* responseCallback);
typedef void (__cdecl *fn_GetUserData_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_SetUserData_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_ConsumeUserItem_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_StartPurchase_t)(void* this_ptr, void* request, void* responseCallback);
typedef void (__cdecl *fn_FinishPurchase_t)(void* this_ptr, void* request, void* responseCallback);

static fn_LoginWithSteam_t g_orig_LoginWithSteam = nullptr;
static fn_GetTitleData_t g_orig_GetTitleData = nullptr;
static fn_GetFilteredCatalog_t g_orig_GetFilteredCatalog = nullptr;
static fn_GetUserInventory_t g_orig_GetUserInventory = nullptr;
static fn_ValidateSteamEntitlement_t g_orig_ValidateSteamEntitlement = nullptr;
static fn_GetTime_t g_orig_GetTime = nullptr;
static fn_GetUserData_t g_orig_GetUserData = nullptr;
static fn_SetUserData_t g_orig_SetUserData = nullptr;
static fn_ConsumeUserItem_t g_orig_ConsumeUserItem = nullptr;
static fn_StartPurchase_t g_orig_StartPurchase = nullptr;
static fn_FinishPurchase_t g_orig_FinishPurchase = nullptr;

struct CatalogDef {
    const char* id;
    unsigned int price;
    unsigned int qty;
    int currency; // 0=Diamond, 1=RealMoney
};

static const CatalogDef kCatalogDefs[] = {
    // 6 Diamond Packs (RealMoney)
    { "ca.sadpanda.crushcrush.steam.diamond_20", 299, 20, 1 },
    { "ca.sadpanda.crushcrush.steam.diamond_55", 699, 55, 1 },
    { "ca.sadpanda.crushcrush.steam.diamond_120", 1399, 120, 1 },
    { "ca.sadpanda.crushcrush.steam.diamond_260", 2799, 260, 1 },
    { "ca.sadpanda.crushcrush.steam.diamond_560", 5499, 560, 1 },
    { "ca.sadpanda.crushcrush.steam.diamond_1200", 9999, 1200, 1 },

    // 6 PES Currency Packs
    { "ca.sadpanda.crushcrush.steam.pescurrency_1", 100, 5, 1 },
    { "ca.sadpanda.crushcrush.steam.pescurrency_2", 250, 15, 1 },
    { "ca.sadpanda.crushcrush.steam.pescurrency_3", 500, 35, 1 },
    { "ca.sadpanda.crushcrush.steam.pescurrency_4", 1000, 75, 1 },
    { "ca.sadpanda.crushcrush.steam.pescurrency_5", 2000, 160, 1 },
    { "ca.sadpanda.crushcrush.steam.pescurrency_6", 4500, 400, 1 },

    // In-game bundles & DLC items
    { "ca.sadpanda.crushcrush.steam.cstarterpack", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.starterpack", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.peanutdlc", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.lotus", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.specialticketbundle", 999, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.lumi", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.kaiju", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.catara", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.daria", 499, 1, 1 },
    { "ca.sadpanda.crushcrush.steam.jellequillzone", 200, 1, 0 },
    { "ca.sadpanda.crushcrush.steam.bonchovyspectrum", 200, 1, 0 }
};

static void FindAssemblyCallback(void* assembly, void* userData) {
    if (!assembly || !g_pfn_assembly_get_image || !g_pfn_image_get_name) return;
    void* img = g_pfn_assembly_get_image(assembly);
    if (img) {
        const char* name = g_pfn_image_get_name(img);
        if (name && strstr(name, "Assembly-CSharp")) {
            *(void**)userData = img;
        }
    }
}

static void EnsureLocalBaseUrl(void* this_ptr, void* domain) {
    if (this_ptr && g_fBaseUrl && domain && g_pfn_string_new && g_pfn_field_set_value) {
        void* localUrlStr = g_pfn_string_new(domain, "http://127.0.0.1:47585/");
        g_pfn_field_set_value(this_ptr, g_fBaseUrl, localUrlStr);
    }
}

static void InvokeCallback(void* callback, void* argObj) {
    if (!callback || !argObj || !g_pfn_runtime_invoke) return;
    void* cbClass = g_pfn_object_get_class ? g_pfn_object_get_class(callback) : nullptr;
    if (cbClass && g_pfn_class_get_method_from_name) {
        void* invokeMethod = g_pfn_class_get_method_from_name(cbClass, "Invoke", 1);
        if (invokeMethod) {
            void* args[1] = { argObj };
            void* exc = nullptr;
            g_pfn_runtime_invoke(invokeMethod, callback, args, &exc);
        }
    }
}

static void __cdecl Hook_LoginWithSteam(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] LoginWithSteam intercepted! this=%p, req=%p, cb=%p", this_ptr, request, responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    const char* tokenJwt = "refix.eyJJRCI6IjEwMDAxIiwiZXhwIjoyMDAwMDAwMDAwLCJmbGFncyI6MH0.refix";

    if (g_blayFapClientClass && g_pfn_class_vtable && g_pfn_field_static_set_value) {
        void* vtable = g_pfn_class_vtable(domain, g_blayFapClientClass);
        if (vtable) {
            void* fLoggedIn = g_pfn_class_get_field_from_name(g_blayFapClientClass, "LoggedIn");
            if (fLoggedIn) {
                uint8_t val = 1;
                g_pfn_field_static_set_value(vtable, fLoggedIn, &val);
            }
            void* fBlayFapId = g_pfn_class_get_field_from_name(g_blayFapClientClass, "BlayFapId");
            if (fBlayFapId) {
                uint64_t val = 10001;
                g_pfn_field_static_set_value(vtable, fBlayFapId, &val);
            }
            void* fAuthToken = g_pfn_class_get_field_from_name(g_blayFapClientClass, "s_authToken");
            if (fAuthToken && g_pfn_string_new) {
                void* strObj = g_pfn_string_new(domain, tokenJwt);
                g_pfn_field_static_set_value(vtable, fAuthToken, strObj);
            }
        }
    }

    if (responseCallback && g_loginRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_loginRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);

            uint64_t blayFapId = 10001;
            void* fId = g_pfn_class_get_field_from_name(g_loginRespClass, "BlayFapId");
            if (fId && g_pfn_field_set_value) g_pfn_field_set_value(respObj, fId, &blayFapId);

            void* fToken = g_pfn_class_get_field_from_name(g_loginRespClass, "AuthToken");
            if (fToken && g_pfn_string_new && g_pfn_field_set_value) {
                void* tokenStr = g_pfn_string_new(domain, tokenJwt);
                g_pfn_field_set_value(respObj, fToken, tokenStr);
            }

            void* fCat = g_pfn_class_get_field_from_name(g_loginRespClass, "CatalogId");
            if (fCat && g_pfn_string_new && g_pfn_field_set_value) {
                void* catStr = g_pfn_string_new(domain, "steam");
                g_pfn_field_set_value(respObj, fCat, catStr);
            }

            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] LoginWithSteam response delivered successfully (BlayFapId=10001)");
        }
    }
}

static void __cdecl Hook_GetTitleData(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] GetTitleData intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_titleRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_titleRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);

            void* fData = g_pfn_class_get_field_from_name(g_titleRespClass, "Data");
            if (fData && g_pfn_field_get_type && g_pfn_class_from_mono_type) {
                void* dictType = g_pfn_field_get_type(fData);
                if (dictType) {
                    void* dictKlass = g_pfn_class_from_mono_type(dictType);
                    if (dictKlass) {
                        void* dictObj = g_pfn_object_new(domain, dictKlass);
                        if (dictObj) {
                            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(dictObj);
                            void* mAdd = g_pfn_class_get_method_from_name(dictKlass, "Add", 2);
                            if (mAdd && g_pfn_runtime_invoke && g_pfn_string_new) {
                                auto addEntry = [&](const char* k, const char* v) {
                                    void* kStr = g_pfn_string_new(domain, k);
                                    void* vStr = g_pfn_string_new(domain, v);
                                    void* args[2] = { kStr, vStr };
                                    void* exc = nullptr;
                                    g_pfn_runtime_invoke(mAdd, dictObj, args, &exc);
                                };
                                addEntry("steamPromotion", "");
                                addEntry("steamTracking", "");
                                addEntry("steamTicketBoothPromotion", "");
                                addEntry("steamCatalog", "steam");
                            }
                            g_pfn_field_set_value(respObj, fData, dictObj);
                        }
                    }
                }
            }

            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] GetTitleData response delivered successfully");
        }
    }
}

static void __cdecl Hook_GetFilteredCatalog(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] GetFilteredCatalog intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_catalogRespClass && g_catalogItemClass && domain && g_pfn_object_new && g_pfn_array_new) {
        size_t count = sizeof(kCatalogDefs) / sizeof(kCatalogDefs[0]);
        void* arr = g_pfn_array_new(domain, g_catalogItemClass, count);
        if (arr) {
            void** items = (void**)((char*)arr + sizeof(void*) * 4);
            void* fId = g_pfn_class_get_field_from_name(g_catalogItemClass, "ItemID");
            void* fPrice = g_pfn_class_get_field_from_name(g_catalogItemClass, "Price");
            void* fQty = g_pfn_class_get_field_from_name(g_catalogItemClass, "Quantity");
            void* fCur = g_pfn_class_get_field_from_name(g_catalogItemClass, "CurrencyType");

            for (size_t i = 0; i < count; i++) {
                void* itemObj = g_pfn_object_new(domain, g_catalogItemClass);
                if (itemObj) {
                    if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(itemObj);

                    void* idStr = g_pfn_string_new(domain, kCatalogDefs[i].id);
                    unsigned int price = kCatalogDefs[i].price;
                    unsigned int qty = kCatalogDefs[i].qty;
                    int cur = kCatalogDefs[i].currency;

                    if (fId) g_pfn_field_set_value(itemObj, fId, idStr);
                    if (fPrice) g_pfn_field_set_value(itemObj, fPrice, &price);
                    if (fQty) g_pfn_field_set_value(itemObj, fQty, &qty);
                    if (fCur) g_pfn_field_set_value(itemObj, fCur, &cur);

                    items[i] = itemObj;
                }
            }

            void* respObj = g_pfn_object_new(domain, g_catalogRespClass);
            if (respObj) {
                if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
                void* fCat = g_pfn_class_get_field_from_name(g_catalogRespClass, "Catalog");
                if (fCat) g_pfn_field_set_value(respObj, fCat, arr);

                InvokeCallback(responseCallback, respObj);
                ReFixLog("[ReFixMono] GetFilteredCatalog delivered %d items successfully", (int)count);
            }
        }
    }
}

static void __cdecl Hook_GetUserInventory(void* this_ptr, void* responseCallback) {
    ReFixLog("[ReFixMono] GetUserInventory intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_inventoryRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_inventoryRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);

            void* fItems = g_pfn_class_get_field_from_name(g_inventoryRespClass, "Items");
            if (fItems && g_pfn_field_get_type && g_pfn_class_from_mono_type) {
                void* listType = g_pfn_field_get_type(fItems);
                if (listType) {
                    void* listKlass = g_pfn_class_from_mono_type(listType);
                    if (listKlass) {
                        void* listObj = g_pfn_object_new(domain, listKlass);
                        if (listObj) {
                            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(listObj);
                            g_pfn_field_set_value(respObj, fItems, listObj);
                        }
                    }
                }
            }

            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] GetUserInventory delivered successfully");
        }
    }
}

static void __cdecl Hook_ValidateSteamEntitlement(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] ValidateSteamEntitlement intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_finishRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_finishRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] ValidateSteamEntitlement response delivered");
        }
    }
}

static void __cdecl Hook_GetTime(void* this_ptr, void* responseCallback) {
    ReFixLog("[ReFixMono] GetTime intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_getTimeRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_getTimeRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] GetTime delivered successfully");
        }
    }
}

static void __cdecl Hook_GetUserData(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] GetUserData intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_getUserDataRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_getUserDataRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] GetUserData delivered successfully");
        }
    }
}

static void __cdecl Hook_SetUserData(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] SetUserData intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_setUserDataRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_setUserDataRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] SetUserData delivered successfully");
        }
    }
}

static void __cdecl Hook_ConsumeUserItem(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] ConsumeUserItem intercepted! cb=%p", responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    if (responseCallback && g_consumeItemRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_consumeItemRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] ConsumeUserItem delivered successfully");
        }
    }
}

static void __cdecl Hook_StartPurchase(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] StartPurchase intercepted! this=%p, req=%p, cb=%p", this_ptr, request, responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    std::string itemId = "ca.sadpanda.crushcrush.steam.diamond_20";
    std::string desc = "";

    if (request && g_startReqClass && g_pfn_class_get_field_from_name && g_pfn_field_get_value) {
        void* itemIdField = g_pfn_class_get_field_from_name(g_startReqClass, "ItemID");
        if (itemIdField) {
            void* strObj = nullptr;
            g_pfn_field_get_value(request, itemIdField, &strObj);
            if (strObj && g_pfn_string_to_utf8) {
                char* s = g_pfn_string_to_utf8(strObj);
                if (s) { itemId = s; if (g_pfn_free) g_pfn_free(s); }
            }
        }

        void* descField = g_pfn_class_get_field_from_name(g_startReqClass, "Description");
        if (descField) {
            void* strObj = nullptr;
            g_pfn_field_get_value(request, descField, &strObj);
            if (strObj && g_pfn_string_to_utf8) {
                char* s = g_pfn_string_to_utf8(strObj);
                if (s) { desc = s; if (g_pfn_free) g_pfn_free(s); }
            }
        }
    }

    uint64_t orderId = g_nextOrderId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_purchaseMutex);
        g_pendingPurchases[orderId] = itemId;
        g_lastRequestedItemId = itemId;
    }
    ReFixLog("[ReFixMono] Purchase started: OrderID=%llu, ItemID='%s', Desc='%s'", (unsigned long long)orderId, itemId.c_str(), desc.c_str());

    std::string orderStr = std::to_string(orderId);
    if (responseCallback && g_startRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_startRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            void* orderIdField = g_pfn_class_get_field_from_name(g_startRespClass, "OrderID");
            if (orderIdField && g_pfn_string_new && g_pfn_field_set_value) {
                void* orderStrObj = g_pfn_string_new(domain, orderStr.c_str());
                g_pfn_field_set_value(respObj, orderIdField, orderStrObj);
            }
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] StartPurchase responseCallback delivered (OrderID=%s)", orderStr.c_str());
        }
    }

    // Determine clean localized display name for purchase modal
    std::string displayName = desc;
    if (displayName.empty() || displayName == itemId) {
        size_t lastDot = itemId.rfind('.');
        std::string suffix = (lastDot != std::string::npos) ? itemId.substr(lastDot + 1) : itemId;
        if (suffix.rfind("diamond_", 0) == 0) {
            displayName = "Diamonds (" + suffix.substr(8) + ")";
        } else {
            displayName = suffix;
        }
    }

    // Open ReFix overlay purchase prompt modal
    ReFixOverlay::PromptPurchase(459820, orderId, displayName, desc, true);
}

static void __cdecl Hook_FinishPurchase(void* this_ptr, void* request, void* responseCallback) {
    ReFixLog("[ReFixMono] FinishPurchase intercepted! this=%p, req=%p, cb=%p", this_ptr, request, responseCallback);

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    EnsureLocalBaseUrl(this_ptr, domain);

    std::string itemId;
    uint64_t orderId = 0;

    if (request && g_finishReqClass && g_pfn_class_get_field_from_name && g_pfn_field_get_value) {
        void* orderIdField = g_pfn_class_get_field_from_name(g_finishReqClass, "OrderID");
        if (orderIdField) {
            void* strObj = nullptr;
            g_pfn_field_get_value(request, orderIdField, &strObj);
            if (strObj && g_pfn_string_to_utf8) {
                char* s = g_pfn_string_to_utf8(strObj);
                if (s) {
                    try { orderId = std::stoull(s); } catch (...) {}
                    if (g_pfn_free) g_pfn_free(s);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_purchaseMutex);
        if (orderId != 0 && g_pendingPurchases.find(orderId) != g_pendingPurchases.end()) {
            itemId = g_pendingPurchases[orderId];
            g_pendingPurchases.erase(orderId);
        } else {
            itemId = g_lastRequestedItemId;
        }
    }

    if (responseCallback && g_finishRespClass && domain && g_pfn_object_new) {
        void* respObj = g_pfn_object_new(domain, g_finishRespClass);
        if (respObj) {
            if (g_pfn_runtime_object_init) g_pfn_runtime_object_init(respObj);
            void* itemIdField = g_pfn_class_get_field_from_name(g_finishRespClass, "ItemID");
            if (itemIdField && g_pfn_string_new && g_pfn_field_set_value) {
                void* itemStr = g_pfn_string_new(domain, itemId.c_str());
                g_pfn_field_set_value(respObj, itemIdField, itemStr);
            }
            InvokeCallback(responseCallback, respObj);
            ReFixLog("[ReFixMono] FinishPurchase responseCallback delivered (ItemID='%s')", itemId.c_str());
        }
    }
}

} // namespace

void ReFixMono_Init() {
    if (g_initialized.exchange(true)) return;

    HMODULE hMono = GetModuleHandleA("mono-2.0-bdwgc.dll");
    if (!hMono) hMono = GetModuleHandleA("mono.dll");
    if (!hMono) return;

    ReFixLog("[ReFixMono] Found Mono runtime module: %p", hMono);

    g_pfn_domain_get = (fn_mono_domain_get)GetProcAddress(hMono, "mono_domain_get");
    g_pfn_thread_attach = (fn_mono_thread_attach)GetProcAddress(hMono, "mono_thread_attach");
    g_pfn_assembly_foreach = (fn_mono_assembly_foreach)GetProcAddress(hMono, "mono_assembly_foreach");
    g_pfn_assembly_get_image = (fn_mono_assembly_get_image)GetProcAddress(hMono, "mono_assembly_get_image");
    g_pfn_image_get_name = (fn_mono_image_get_name)GetProcAddress(hMono, "mono_image_get_name");
    g_pfn_class_from_name = (fn_mono_class_from_name)GetProcAddress(hMono, "mono_class_from_name");
    g_pfn_class_get_method_from_name = (fn_mono_class_get_method_from_name)GetProcAddress(hMono, "mono_class_get_method_from_name");
    g_pfn_compile_method = (fn_mono_compile_method)GetProcAddress(hMono, "mono_compile_method");
    g_pfn_runtime_invoke = (fn_mono_runtime_invoke)GetProcAddress(hMono, "mono_runtime_invoke");
    g_pfn_class_get_field_from_name = (fn_mono_class_get_field_from_name)GetProcAddress(hMono, "mono_class_get_field_from_name");
    g_pfn_field_get_value = (fn_mono_field_get_value)GetProcAddress(hMono, "mono_field_get_value");
    g_pfn_field_set_value = (fn_mono_field_set_value)GetProcAddress(hMono, "mono_field_set_value");
    g_pfn_field_static_set_value = (fn_mono_field_static_set_value)GetProcAddress(hMono, "mono_field_static_set_value");
    g_pfn_class_vtable = (fn_mono_class_vtable)GetProcAddress(hMono, "mono_class_vtable");
    g_pfn_object_new = (fn_mono_object_new)GetProcAddress(hMono, "mono_object_new");
    g_pfn_runtime_object_init = (fn_mono_runtime_object_init)GetProcAddress(hMono, "mono_runtime_object_init");
    g_pfn_string_new = (fn_mono_string_new)GetProcAddress(hMono, "mono_string_new");
    g_pfn_string_to_utf8 = (fn_mono_string_to_utf8)GetProcAddress(hMono, "mono_string_to_utf8");
    g_pfn_free = (fn_mono_free)GetProcAddress(hMono, "mono_free");
    g_pfn_object_get_class = (fn_mono_object_get_class)GetProcAddress(hMono, "mono_object_get_class");
    g_pfn_field_get_type = (fn_mono_field_get_type)GetProcAddress(hMono, "mono_field_get_type");
    g_pfn_class_from_mono_type = (fn_mono_class_from_mono_type)GetProcAddress(hMono, "mono_class_from_mono_type");
    g_pfn_array_new = (fn_mono_array_new)GetProcAddress(hMono, "mono_array_new");

    if (!g_pfn_domain_get || !g_pfn_assembly_foreach || !g_pfn_class_from_name || !g_pfn_compile_method) {
        ReFixLog("[ReFixMono] Failed to resolve essential Mono functions");
        return;
    }

    void* domain = g_pfn_domain_get();
    if (!domain) {
        ReFixLog("[ReFixMono] No active Mono domain yet");
        return;
    }
    if (g_pfn_thread_attach) g_pfn_thread_attach(domain);

    void* img = nullptr;
    g_pfn_assembly_foreach(FindAssemblyCallback, &img);
    if (!img) {
        ReFixLog("[ReFixMono] Assembly-CSharp not found in loaded assemblies");
        return;
    }
    g_assemblyCSharpImage = img;
    ReFixLog("[ReFixMono] Located Assembly-CSharp image: %p", img);

    // Resolve response and model classes
    g_blayFapClientClass = g_pfn_class_from_name(img, "BlayFap", "BlayFapClient");
    if (g_blayFapClientClass && g_pfn_class_get_field_from_name) {
        g_fBaseUrl = g_pfn_class_get_field_from_name(g_blayFapClientClass, "baseUrl");
    }

    g_startReqClass = g_pfn_class_from_name(img, "BlayFapShared", "StartPurchaseRequest");
    g_startRespClass = g_pfn_class_from_name(img, "BlayFapShared", "StartPurchaseResponse");
    g_finishReqClass = g_pfn_class_from_name(img, "BlayFapShared", "FinishPurchaseRequest");
    g_finishRespClass = g_pfn_class_from_name(img, "BlayFapShared", "FinishPurchaseResponse");
    g_loginRespClass = g_pfn_class_from_name(img, "BlayFapShared", "LoginResponse");
    g_titleRespClass = g_pfn_class_from_name(img, "BlayFapShared", "GetTitleDataResponse");
    g_catalogRespClass = g_pfn_class_from_name(img, "BlayFapShared", "GetCatalogResponse");
    g_catalogItemClass = g_pfn_class_from_name(img, "BlayFapShared", "CatalogItem");
    g_inventoryRespClass = g_pfn_class_from_name(img, "BlayFapShared", "GetUserInventoryResponse");
    g_getTimeRespClass = g_pfn_class_from_name(img, "BlayFapShared", "GetTimeResponse");
    g_getUserDataRespClass = g_pfn_class_from_name(img, "BlayFapShared", "GetUserDataResponse");
    g_setUserDataRespClass = g_pfn_class_from_name(img, "BlayFapShared", "SetUserDataResponse");
    g_consumeItemRespClass = g_pfn_class_from_name(img, "BlayFapShared", "ConsumeUserItemResponse");

    MH_Initialize();

    if (g_blayFapClientClass) {
        // 1. Hook LoginWithSteam
        void* loginMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "LoginWithSteam", 2);
        if (loginMethod) {
            void* pNative = g_pfn_compile_method(loginMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_LoginWithSteam, (void**)&g_orig_LoginWithSteam) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.LoginWithSteam at %p", pNative);
            }
        }

        // 2. Hook GetTitleData
        void* titleMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "GetTitleData", 2);
        if (titleMethod) {
            void* pNative = g_pfn_compile_method(titleMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_GetTitleData, (void**)&g_orig_GetTitleData) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.GetTitleData at %p", pNative);
            }
        }

        // 3. Hook GetFilteredCatalog
        void* catalogMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "GetFilteredCatalog", 2);
        if (catalogMethod) {
            void* pNative = g_pfn_compile_method(catalogMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_GetFilteredCatalog, (void**)&g_orig_GetFilteredCatalog) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.GetFilteredCatalog at %p", pNative);
            }
        }

        // 4. Hook GetUserInventory
        void* invMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "GetUserInventory", 1);
        if (invMethod) {
            void* pNative = g_pfn_compile_method(invMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_GetUserInventory, (void**)&g_orig_GetUserInventory) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.GetUserInventory at %p", pNative);
            }
        }

        // 5. Hook ValidateSteamEntitlement
        void* valMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "ValidateSteamEntitlement", 2);
        if (valMethod) {
            void* pNative = g_pfn_compile_method(valMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_ValidateSteamEntitlement, (void**)&g_orig_ValidateSteamEntitlement) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.ValidateSteamEntitlement at %p", pNative);
            }
        }

        // 6. Hook GetTime
        void* getTimeMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "GetTime", 1);
        if (getTimeMethod) {
            void* pNative = g_pfn_compile_method(getTimeMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_GetTime, (void**)&g_orig_GetTime) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.GetTime at %p", pNative);
            }
        }

        // 7. Hook GetUserData
        void* getUserDataMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "GetUserData", 2);
        if (getUserDataMethod) {
            void* pNative = g_pfn_compile_method(getUserDataMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_GetUserData, (void**)&g_orig_GetUserData) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.GetUserData at %p", pNative);
            }
        }

        // 8. Hook SetUserData
        void* setUserDataMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "SetUserData", 2);
        if (setUserDataMethod) {
            void* pNative = g_pfn_compile_method(setUserDataMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_SetUserData, (void**)&g_orig_SetUserData) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.SetUserData at %p", pNative);
            }
        }

        // 9. Hook ConsumeUserItem
        void* consumeItemMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "ConsumeUserItem", 2);
        if (consumeItemMethod) {
            void* pNative = g_pfn_compile_method(consumeItemMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_ConsumeUserItem, (void**)&g_orig_ConsumeUserItem) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.ConsumeUserItem at %p", pNative);
            }
        }

        // 10. Hook StartPurchase
        void* startMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "StartPurchase", 2);
        if (startMethod) {
            void* pNative = g_pfn_compile_method(startMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_StartPurchase, (void**)&g_orig_StartPurchase) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.StartPurchase at %p", pNative);
            }
        }

        // 11. Hook FinishPurchase
        void* finishMethod = g_pfn_class_get_method_from_name(g_blayFapClientClass, "FinishPurchase", 2);
        if (finishMethod) {
            void* pNative = g_pfn_compile_method(finishMethod);
            if (pNative && MH_CreateHook(pNative, (void*)&Hook_FinishPurchase, (void**)&g_orig_FinishPurchase) == MH_OK) {
                MH_EnableHook(pNative);
                ReFixLog("[ReFixMono] Hooked BlayFapClient.FinishPurchase at %p", pNative);
            }
        }
    }

    // Resolve BlayFapIntegration class for DLC unlocking
    void* integrationKlass = g_pfn_class_from_name(img, "", "BlayFapIntegration");
    if (integrationKlass && g_pfn_class_get_method_from_name) {
        g_consumeInventoryMethod = g_pfn_class_get_method_from_name(integrationKlass, "ConsumeCachedBlayFapInventory", 1);
        ReFixLog("[ReFixMono] Located BlayFapIntegration.ConsumeCachedBlayFapInventory: %p", g_consumeInventoryMethod);
    }
}

void ReFixMono_UnlockDLC(uint32_t dlcId) {
    if (!g_consumeInventoryMethod || !g_pfn_runtime_invoke) return;

    void* domain = g_pfn_domain_get ? g_pfn_domain_get() : nullptr;
    if (domain && g_pfn_thread_attach) g_pfn_thread_attach(domain);

    uint8_t validateEntitlements = 0;
    void* args[1] = { &validateEntitlements };
    void* exc = nullptr;
    g_pfn_runtime_invoke(g_consumeInventoryMethod, nullptr, args, &exc);
    ReFixLog("[ReFixMono] Invoked ConsumeCachedBlayFapInventory after DLC %u unlock", dlcId);
}
