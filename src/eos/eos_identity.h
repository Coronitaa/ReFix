// =============================================================================
// ReFix EOS Online v2 - Logical & Persistent Identity Manager (Opaque Handles)
// =============================================================================
#pragma once

#include "eos_types.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace ReFixEOS {

// Magic constants for opaque handle validation
constexpr uint32_t PUID_HANDLE_MAGIC = 0x50554944; // 'PUID'
constexpr uint32_t EAID_HANDLE_MAGIC = 0x45414944; // 'EAID'

#pragma pack(push, 8)
struct OpaqueProductUserIdHandle {
    uint32_t magic;
    char hexString[33];
};

struct OpaqueEpicAccountIdHandle {
    uint32_t magic;
    char hexString[33];
};
#pragma pack(pop)

struct ExternalAccountData {
    int32_t accountType;
    std::string accountId;
    std::string displayName;
};

struct UserIdentityRecord {
    std::string accountUuid;
    std::string puidString;
    std::string eaidString;
    std::string displayName;
    uint64_t steamId64;
    std::vector<ExternalAccountData> externalAccounts;
    OpaqueProductUserIdHandle* handlePUID;
    OpaqueEpicAccountIdHandle* handleEAID;
};

class IdentityManager {
public:
    static IdentityManager& Get();

    void Initialize();
    void RefreshFromEnvironment();

    // Local user identity
    EOS_ProductUserId GetLocalProductUserId();
    EOS_EpicAccountId GetLocalEpicAccountId();
    const std::string& GetLocalProductUserIdString();
    const std::string& GetLocalEpicAccountIdString();
    const std::string& GetLocalDisplayName();
    uint64_t GetLocalSteamId();
    const std::string& GetLocalAccountUuid();

    void SetLocalDisplayName(const std::string& name);
    void SetLocalSteamId(uint64_t steamId);

    // PUID <-> Handle & String conversions (Opaque Handle Model)
    bool IsValidProductUserId(EOS_ProductUserId puid);
    bool IsValidEpicAccountId(EOS_EpicAccountId eaid);
    std::string ProductUserIdToString(EOS_ProductUserId puid);
    std::string EpicAccountIdToString(EOS_EpicAccountId eaid);
    EOS_ProductUserId ProductUserIdFromString(const char* puidStr);
    EOS_EpicAccountId EpicAccountIdFromString(const char* eaidStr);

    // External account resolution
    EOS_ProductUserId GetOrCreateProductUserIdFromSteamId(uint64_t steamId, const std::string& personaName = "");
    EOS_ProductUserId GetOrCreateProductUserIdFromExternal(int32_t accountType, const std::string& externalId, const std::string& displayName = "");
    EOS_ProductUserId GetOrCreateProductUserId(const std::string& puidStr);
    EOS_EpicAccountId GetOrCreateEpicAccountId(const std::string& eaidStr);

    // Record lookups
    const UserIdentityRecord* GetRecordByPUID(EOS_ProductUserId puid);
    const UserIdentityRecord* GetRecordByEAID(EOS_EpicAccountId eaid);

    // External Account Info memory helpers
    void* AllocateExternalAccountInfo(EOS_ProductUserId puid, int32_t accountTypeIndex = -1, int32_t targetAccountType = -1);
    void FreeExternalAccountInfo(void* info);

private:
    IdentityManager();
    ~IdentityManager() = default;

    std::string LoadOrCreatePersistentAccountUuid();
    std::string DerivePuidFromUuid(const std::string& uuid);
    std::string DeriveEaidFromUuid(const std::string& uuid);
    bool IsValidHex32String(const char* str);

    std::recursive_mutex m_mutex;
    bool m_initialized = false;

    UserIdentityRecord m_localUser;
    std::unordered_map<std::string, UserIdentityRecord> m_recordsByPuidStr;
    std::unordered_map<void*, std::string> m_puidToPuidStr;
    std::unordered_map<void*, std::string> m_eaidToPuidStr;
    std::unordered_map<uint64_t, std::string> m_steamIdToPuidStr;
    std::vector<void*> m_allocatedHandles;
};

} // namespace ReFixEOS

// C API Export declarations
extern "C" {
    EOS_EResult EOS_ProductUserId_IsValid(EOS_ProductUserId Id);
    EOS_EResult EOS_EpicAccountId_IsValid(EOS_EpicAccountId Id);
    EOS_EResult EOS_ProductUserId_ToString(EOS_ProductUserId Id, char* OutBuffer, int32_t* InOutBufferLength);
    EOS_EResult EOS_EpicAccountId_ToString(EOS_EpicAccountId Id, char* OutBuffer, int32_t* InOutBufferLength);
    EOS_ProductUserId EOS_ProductUserId_FromString(const char* ProductUserIdString);
    EOS_EpicAccountId EOS_EpicAccountId_FromString(const char* EpicAccountIdString);
}
