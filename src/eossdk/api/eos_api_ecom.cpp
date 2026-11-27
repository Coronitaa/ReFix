// ReFix EOS v3 - EOS_Ecom_* emulation for Epic Games Store DLC/entitlement unlocking and purchase prompt.
#include "../core/refix_common.h"
#include "../sdk/eos_ecom.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

using namespace refix;

struct EOS_Ecom_TransactionHandle {
    uint32_t Magic;
    std::string TxId;
    std::vector<std::string> Entitlements;
};

namespace {

constexpr uint32_t kMagicTransaction = 0x52465854u; // 'RFXT'

std::mutex g_ecomMutex;
std::vector<std::string> g_cachedEntitlements;
std::vector<EOS_Ecom_TransactionHandle> g_transactions;
std::atomic<uint64_t> g_nextTxSeq{ 1 };

bool IsEcomUnlockEnabled() {
    std::string unlock = Config::Get().GetString("EOS", "UnlockDLCs", "");
    if (!unlock.empty()) {
        return _stricmp(unlock.c_str(), "true") == 0 || unlock == "1";
    }
    std::string dlcs = Config::Get().GetString("Steam", "DLCs", "all");
    return _stricmp(dlcs.c_str(), "none") != 0;
}

void RecordEntitlements(const EOS_Ecom_EntitlementName* names, uint32_t count) {
    if (!names || count == 0) return;
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    for (uint32_t i = 0; i < count; ++i) {
        if (!names[i] || !names[i][0]) continue;
        bool exists = false;
        for (const auto& s : g_cachedEntitlements) {
            if (s == names[i]) { exists = true; break; }
        }
        if (!exists) g_cachedEntitlements.emplace_back(names[i]);
    }
}

void RecordEntitlementNames(const std::vector<std::string>& names) {
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    for (const auto& n : names) {
        if (n.empty()) continue;
        bool exists = false;
        for (const auto& s : g_cachedEntitlements) {
            if (s == n) { exists = true; break; }
        }
        if (!exists) g_cachedEntitlements.emplace_back(n);
    }
}

char* DupStr(const std::string& s) {
    char* p = new char[s.size() + 1];
    memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

EOS_Ecom_Entitlement* AllocEntitlement(const std::string& name, int32_t index) {
    auto* e = new EOS_Ecom_Entitlement{};
    e->ApiVersion = EOS_ECOM_ENTITLEMENT_API_LATEST;
    e->EntitlementName = DupStr(name);
    e->EntitlementId = DupStr("refix_" + name);
    e->CatalogItemId = DupStr(name);
    e->ServerIndex = index;
    e->bRedeemed = EOS_FALSE;
    e->EndTimestamp = EOS_ECOM_ENTITLEMENT_ENDTIMESTAMP_UNDEFINED;
    return e;
}

using Fn_ReFix_PromptPurchase = bool(*)(uint32_t appId, const char* itemName, const char* itemDesc, void(*onComplete)(bool approved, void* ctx), void* ctx);

Fn_ReFix_PromptPurchase ResolvePromptPurchase() {
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (!hSteam) hSteam = GetModuleHandleA("steam_api.dll");
    if (!hSteam) return nullptr;
    return (Fn_ReFix_PromptPurchase)GetProcAddress(hSteam, "ReFix_PromptPurchase");
}

struct EosCheckoutCtx {
    EOS_EpicAccountId User;
    std::vector<std::string> OfferIds;
    void* ClientData;
    EOS_Ecom_OnCheckoutCallback Callback;
};

void CompleteEosCheckout(bool approved, void* rawCtx) {
    auto* ctx = static_cast<EosCheckoutCtx*>(rawCtx);
    if (!ctx) return;
    std::string txId;
    if (approved) {
        txId = "rfxtx_" + std::to_string(g_nextTxSeq.fetch_add(1, std::memory_order_relaxed));
        RecordEntitlementNames(ctx->OfferIds);
        std::lock_guard<std::mutex> lock(g_ecomMutex);
        EOS_Ecom_TransactionHandle tx;
        tx.Magic = kMagicTransaction;
        tx.TxId = txId;
        tx.Entitlements = ctx->OfferIds;
        g_transactions.push_back(std::move(tx));
    }

    auto cb = ctx->Callback;
    void* clientData = ctx->ClientData;
    EOS_EpicAccountId user = ctx->User;
    delete ctx;

    if (!cb) return;
    Dispatcher::Get().Post([cb, clientData, user, approved, txId]() {
        EOS_Ecom_CheckoutCallbackInfo info{};
        info.ResultCode = approved ? EOS_EResult::EOS_Success : EOS_EResult::EOS_Canceled;
        info.ClientData = clientData;
        info.LocalUserId = user;
        info.TransactionId = approved ? txId.c_str() : nullptr;
        cb(&info);
    });
}

} // namespace

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryOwnership(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryOwnershipOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryOwnershipCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;

    std::vector<std::string> ids;
    if (Options && Options->CatalogItemIds && Options->CatalogItemIdCount > 0) {
        for (uint32_t i = 0; i < Options->CatalogItemIdCount; ++i) {
            if (Options->CatalogItemIds[i]) ids.emplace_back(Options->CatalogItemIds[i]);
        }
    }
    const EOS_EOwnershipStatus status = IsEcomUnlockEnabled() ? EOS_EOwnershipStatus::EOS_OS_Owned
                                                              : EOS_EOwnershipStatus::EOS_OS_NotOwned;

    Dispatcher::Get().Post([CompletionDelegate, ClientData, user, ids = std::move(ids), status]() {
        std::vector<EOS_Ecom_ItemOwnership> items;
        items.reserve(ids.size());
        for (const auto& id : ids) {
            EOS_Ecom_ItemOwnership item{};
            item.ApiVersion = EOS_ECOM_ITEMOWNERSHIP_API_LATEST;
            item.Id = id.c_str();
            item.OwnershipStatus = status;
            items.push_back(item);
        }
        EOS_Ecom_QueryOwnershipCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        info.ItemOwnership = items.empty() ? nullptr : items.data();
        info.ItemOwnershipCount = (uint32_t)items.size();
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryOwnershipBySandboxIds(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryOwnershipBySandboxIdsOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryOwnershipBySandboxIdsCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user]() {
        EOS_Ecom_QueryOwnershipBySandboxIdsCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryOwnershipToken(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryOwnershipTokenOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryOwnershipTokenCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user]() {
        EOS_Ecom_QueryOwnershipTokenCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        info.OwnershipToken = "refix_ownership_token";
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryEntitlements(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryEntitlementsOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryEntitlementsCallback CompletionDelegate) {
    if (Options && IsEcomUnlockEnabled()) {
        RecordEntitlements(Options->EntitlementNames, Options->EntitlementNameCount);
    }
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user]() {
        EOS_Ecom_QueryEntitlementsCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryEntitlementToken(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryEntitlementTokenOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryEntitlementTokenCallback CompletionDelegate) {
    if (Options && IsEcomUnlockEnabled()) {
        RecordEntitlements(Options->EntitlementNames, Options->EntitlementNameCount);
    }
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user]() {
        EOS_Ecom_QueryEntitlementTokenCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        info.EntitlementToken = "refix_entitlement_token";
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_QueryOffers(
    EOS_HEcom Handle,
    const EOS_Ecom_QueryOffersOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnQueryOffersCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user]() {
        EOS_Ecom_QueryOffersCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_Ecom_Checkout(
    EOS_HEcom Handle,
    const EOS_Ecom_CheckoutOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnCheckoutCallback CompletionDelegate) {
    auto* ctx = new EosCheckoutCtx{};
    ctx->User = Options ? Options->LocalUserId : nullptr;
    ctx->ClientData = ClientData;
    ctx->Callback = CompletionDelegate;

    std::string summary;
    if (Options && Options->Entries && Options->EntryCount > 0) {
        for (uint32_t i = 0; i < Options->EntryCount; ++i) {
            if (Options->Entries[i].OfferId && Options->Entries[i].OfferId[0]) {
                std::string oid = Options->Entries[i].OfferId;
                ctx->OfferIds.push_back(oid);
                if (!summary.empty()) summary += ", ";
                summary += oid;
            }
        }
    }
    if (summary.empty()) summary = "Epic Games Store Item";

    RFLOG(Core, "[Ecom] EOS_Ecom_Checkout requested: %s", summary.c_str());

    if (auto pPrompt = ResolvePromptPurchase()) {
        std::string desc = "Epic Games Store Offer";
        if (pPrompt(0, summary.c_str(), desc.c_str(), CompleteEosCheckout, ctx)) {
            return;
        }
    }
    CompleteEosCheckout(true, ctx);
}

EOS_DECLARE_FUNC(void) EOS_Ecom_RedeemEntitlements(
    EOS_HEcom Handle,
    const EOS_Ecom_RedeemEntitlementsOptions* Options,
    void* ClientData,
    const EOS_Ecom_OnRedeemEntitlementsCallback CompletionDelegate) {
    uint32_t count = (Options && Options->EntitlementIds) ? Options->EntitlementIdCount : 0;
    if (!CompletionDelegate) return;
    EOS_EpicAccountId user = Options ? Options->LocalUserId : nullptr;
    Dispatcher::Get().Post([CompletionDelegate, ClientData, user, count]() {
        EOS_Ecom_RedeemEntitlementsCallbackInfo info{};
        info.ResultCode = EOS_EResult::EOS_Success;
        info.ClientData = ClientData;
        info.LocalUserId = user;
        info.RedeemedEntitlementIdsCount = count;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(uint32_t) EOS_Ecom_GetEntitlementsCount(
    EOS_HEcom Handle,
    const EOS_Ecom_GetEntitlementsCountOptions* Options) {
    if (!IsEcomUnlockEnabled()) return 0;
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    return (uint32_t)g_cachedEntitlements.size();
}

EOS_DECLARE_FUNC(uint32_t) EOS_Ecom_GetEntitlementsByNameCount(
    EOS_HEcom Handle,
    const EOS_Ecom_GetEntitlementsByNameCountOptions* Options) {
    if (!IsEcomUnlockEnabled() || !Options || !Options->EntitlementName || !Options->EntitlementName[0]) return 0;
    return 1;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_CopyEntitlementByIndex(
    EOS_HEcom Handle,
    const EOS_Ecom_CopyEntitlementByIndexOptions* Options,
    EOS_Ecom_Entitlement** OutEntitlement) {
    if (!Options || !OutEntitlement) return EOS_EResult::EOS_InvalidParameters;
    *OutEntitlement = nullptr;
    if (!IsEcomUnlockEnabled()) return EOS_EResult::EOS_NotFound;
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    if (Options->EntitlementIndex >= g_cachedEntitlements.size()) return EOS_EResult::EOS_NotFound;
    *OutEntitlement = AllocEntitlement(g_cachedEntitlements[Options->EntitlementIndex], (int32_t)Options->EntitlementIndex);
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_CopyEntitlementByNameAndIndex(
    EOS_HEcom Handle,
    const EOS_Ecom_CopyEntitlementByNameAndIndexOptions* Options,
    EOS_Ecom_Entitlement** OutEntitlement) {
    if (!Options || !Options->EntitlementName || !OutEntitlement) return EOS_EResult::EOS_InvalidParameters;
    *OutEntitlement = nullptr;
    if (!IsEcomUnlockEnabled() || Options->Index > 0) return EOS_EResult::EOS_NotFound;
    *OutEntitlement = AllocEntitlement(Options->EntitlementName, 0);
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_CopyEntitlementById(
    EOS_HEcom Handle,
    const EOS_Ecom_CopyEntitlementByIdOptions* Options,
    EOS_Ecom_Entitlement** OutEntitlement) {
    if (!Options || !Options->EntitlementId || !OutEntitlement) return EOS_EResult::EOS_InvalidParameters;
    *OutEntitlement = nullptr;
    if (!IsEcomUnlockEnabled()) return EOS_EResult::EOS_NotFound;
    *OutEntitlement = AllocEntitlement(Options->EntitlementId, 0);
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Ecom_Entitlement_Release(EOS_Ecom_Entitlement* Entitlement) {
    if (!Entitlement) return;
    delete[] Entitlement->EntitlementName;
    delete[] Entitlement->EntitlementId;
    delete[] Entitlement->CatalogItemId;
    delete Entitlement;
}

EOS_DECLARE_FUNC(uint32_t) EOS_Ecom_GetOfferCount(EOS_HEcom, const EOS_Ecom_GetOfferCountOptions*) {
    return 0;
}

EOS_DECLARE_FUNC(void) EOS_Ecom_CatalogOffer_Release(EOS_Ecom_CatalogOffer* Offer) {
    delete Offer;
}

EOS_DECLARE_FUNC(uint32_t) EOS_Ecom_GetTransactionCount(EOS_HEcom, const EOS_Ecom_GetTransactionCountOptions*) {
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    return (uint32_t)g_transactions.size();
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_CopyTransactionByIndex(
    EOS_HEcom,
    const EOS_Ecom_CopyTransactionByIndexOptions* Options,
    EOS_Ecom_HTransaction* OutTransaction) {
    if (!Options || !OutTransaction) return EOS_EResult::EOS_InvalidParameters;
    *OutTransaction = nullptr;
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    if (Options->TransactionIndex >= g_transactions.size()) return EOS_EResult::EOS_NotFound;
    *OutTransaction = new EOS_Ecom_TransactionHandle(g_transactions[Options->TransactionIndex]);
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_CopyTransactionById(
    EOS_HEcom,
    const EOS_Ecom_CopyTransactionByIdOptions* Options,
    EOS_Ecom_HTransaction* OutTransaction) {
    if (!Options || !Options->TransactionId || !OutTransaction) return EOS_EResult::EOS_InvalidParameters;
    *OutTransaction = nullptr;
    std::lock_guard<std::mutex> lock(g_ecomMutex);
    for (const auto& tx : g_transactions) {
        if (tx.TxId == Options->TransactionId) {
            *OutTransaction = new EOS_Ecom_TransactionHandle(tx);
            return EOS_EResult::EOS_Success;
        }
    }
    return EOS_EResult::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_Transaction_GetTransactionId(
    EOS_Ecom_HTransaction Handle,
    char* OutBuffer,
    int32_t* InOutBufferLength) {
    if (!Handle || Handle->Magic != kMagicTransaction || !InOutBufferLength) return EOS_EResult::EOS_InvalidParameters;
    int32_t needed = (int32_t)Handle->TxId.size() + 1;
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_EResult::EOS_InvalidParameters;
    }
    memcpy(OutBuffer, Handle->TxId.c_str(), (size_t)needed);
    *InOutBufferLength = needed;
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(uint32_t) EOS_Ecom_Transaction_GetEntitlementsCount(
    EOS_Ecom_HTransaction Handle,
    const EOS_Ecom_Transaction_GetEntitlementsCountOptions*) {
    if (!Handle || Handle->Magic != kMagicTransaction) return 0;
    return (uint32_t)Handle->Entitlements.size();
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Ecom_Transaction_CopyEntitlementByIndex(
    EOS_Ecom_HTransaction Handle,
    const EOS_Ecom_Transaction_CopyEntitlementByIndexOptions* Options,
    EOS_Ecom_Entitlement** OutEntitlement) {
    if (!Handle || Handle->Magic != kMagicTransaction || !Options || !OutEntitlement) return EOS_EResult::EOS_InvalidParameters;
    *OutEntitlement = nullptr;
    if (Options->EntitlementIndex >= Handle->Entitlements.size()) return EOS_EResult::EOS_NotFound;
    *OutEntitlement = AllocEntitlement(Handle->Entitlements[Options->EntitlementIndex], (int32_t)Options->EntitlementIndex);
    return EOS_EResult::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_Ecom_Transaction_Release(EOS_Ecom_HTransaction Transaction) {
    delete Transaction;
}

namespace refix {

void RegisterEcomApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_Ecom_QueryOwnership);
    REFIX_BIND(reg, EOS_Ecom_QueryOwnershipBySandboxIds);
    REFIX_BIND(reg, EOS_Ecom_QueryOwnershipToken);
    REFIX_BIND(reg, EOS_Ecom_QueryEntitlements);
    REFIX_BIND(reg, EOS_Ecom_QueryEntitlementToken);
    REFIX_BIND(reg, EOS_Ecom_QueryOffers);
    REFIX_BIND(reg, EOS_Ecom_Checkout);
    REFIX_BIND(reg, EOS_Ecom_RedeemEntitlements);
    REFIX_BIND(reg, EOS_Ecom_GetEntitlementsCount);
    REFIX_BIND(reg, EOS_Ecom_GetEntitlementsByNameCount);
    REFIX_BIND(reg, EOS_Ecom_CopyEntitlementByIndex);
    REFIX_BIND(reg, EOS_Ecom_CopyEntitlementByNameAndIndex);
    REFIX_BIND(reg, EOS_Ecom_CopyEntitlementById);
    REFIX_BIND(reg, EOS_Ecom_Entitlement_Release);
    REFIX_BIND(reg, EOS_Ecom_GetOfferCount);
    REFIX_BIND(reg, EOS_Ecom_CatalogOffer_Release);
    REFIX_BIND(reg, EOS_Ecom_GetTransactionCount);
    REFIX_BIND(reg, EOS_Ecom_CopyTransactionByIndex);
    REFIX_BIND(reg, EOS_Ecom_CopyTransactionById);
    REFIX_BIND(reg, EOS_Ecom_Transaction_GetTransactionId);
    REFIX_BIND(reg, EOS_Ecom_Transaction_GetEntitlementsCount);
    REFIX_BIND(reg, EOS_Ecom_Transaction_CopyEntitlementByIndex);
    REFIX_BIND(reg, EOS_Ecom_Transaction_Release);
}

} // namespace refix
