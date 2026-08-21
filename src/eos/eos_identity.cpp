// =============================================================================
// ReFix EOS Online v2 - Logical & Persistent Identity Implementation
// =============================================================================
#include "eos_identity.h"
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <objbase.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cctype>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace ReFixEOS {

static std::string ComputeSHA256Hex(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result = "";

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
                BYTE hashBytes[32];
                DWORD hashLen = sizeof(hashBytes);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0)) {
                    std::stringstream ss;
                    for (DWORD i = 0; i < hashLen; ++i) {
                        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hashBytes[i];
                    }
                    result = ss.str();
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    if (result.empty()) {
        uint64_t h1 = 14695981039346656037ULL;
        uint64_t h2 = 1099511628211ULL;
        for (char c : input) {
            h1 = (h1 ^ (uint8_t)c) * 1099511628211ULL;
            h2 = (h2 ^ (uint8_t)c) * 14695981039346656037ULL;
        }
        char buf[65];
        sprintf_s(buf, "%016llx%016llx", h1, h2);
        result = buf;
    }

    return result;
}

static std::string GenerateRandomUUID() {
    GUID guid;
    if (CoCreateGuid(&guid) == S_OK) {
        char guidStr[64];
        sprintf_s(guidStr, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return guidStr;
    }

    HCRYPTPROV hProv = 0;
    BYTE bytes[16];
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, bytes);
        CryptReleaseContext(hProv, 0);
    } else {
        for (int i = 0; i < 16; ++i) bytes[i] = (BYTE)(rand() & 0xFF);
    }
    char buf[64];
    sprintf_s(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return buf;
}

IdentityManager& IdentityManager::Get() {
    static IdentityManager s_instance;
    return s_instance;
}

IdentityManager::IdentityManager() {
    Initialize();
}

bool IdentityManager::IsValidHex32String(const char* str) {
    if (!str) return false;
    size_t len = strlen(str);
    if (len != 32) return false;
    for (size_t i = 0; i < 32; ++i) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

std::string IdentityManager::LoadOrCreatePersistentAccountUuid() {
    char appDataPath[MAX_PATH] = { 0 };
    std::string profileFilePath;

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string refixDir = std::string(appDataPath) + "\\ReFix";
        CreateDirectoryA(refixDir.c_str(), NULL);
        profileFilePath = refixDir + "\\user_profile.json";
    } else {
        profileFilePath = "refix_user_profile.json";
    }

    std::ifstream inFile(profileFilePath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            size_t pos = line.find("\"account_uuid\":");
            if (pos != std::string::npos) {
                size_t firstQuote = line.find("\"", pos + 15);
                if (firstQuote != std::string::npos) {
                    size_t secondQuote = line.find("\"", firstQuote + 1);
                    if (secondQuote != std::string::npos) {
                        std::string uuid = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                        if (uuid.length() >= 16) {
                            return uuid;
                        }
                    }
                }
            }
        }
        inFile.close();
    }

    std::string newUuid = GenerateRandomUUID();

    std::ofstream outFile(profileFilePath);
    if (outFile.is_open()) {
        outFile << "{\n";
        outFile << "  \"account_uuid\": \"" << newUuid << "\",\n";
        outFile << "  \"created_at\": " << GetTickCount64() << ",\n";
        outFile << "  \"display_name\": \"ReFix Player\"\n";
        outFile << "}\n";
        outFile.close();
    }

    return newUuid;
}

std::string IdentityManager::DerivePuidFromUuid(const std::string& uuid) {
    std::string hash = ComputeSHA256Hex("EOS_PUID:" + uuid);
    return hash.substr(0, 32);
}

std::string IdentityManager::DeriveEaidFromUuid(const std::string& uuid) {
    std::string hash = ComputeSHA256Hex("EOS_EAID:" + uuid);
    return hash.substr(0, 32);
}

void IdentityManager::Initialize() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_initialized) return;
    m_initialized = true;

    m_localUser.accountUuid = LoadOrCreatePersistentAccountUuid();
    m_localUser.puidString = DerivePuidFromUuid(m_localUser.accountUuid);
    m_localUser.eaidString = DeriveEaidFromUuid(m_localUser.accountUuid);
    m_localUser.displayName = "ReFix Player";

    uint64_t hashVal = 0;
    for (size_t i = 0; i < 16 && i < m_localUser.puidString.length(); ++i) {
        hashVal = (hashVal << 4) | (m_localUser.puidString[i] >= 'a' ? (m_localUser.puidString[i] - 'a' + 10) : (m_localUser.puidString[i] - '0'));
    }
    uint32_t accountId = (uint32_t)(hashVal & 0x0FFFFFFF);
    if (accountId == 0) accountId = 100001;
    m_localUser.steamId64 = 76561197960265728ULL + accountId;

    auto* puidHandle = new OpaqueProductUserIdHandle();
    puidHandle->magic = PUID_HANDLE_MAGIC;
    strcpy_s(puidHandle->hexString, sizeof(puidHandle->hexString), m_localUser.puidString.c_str());
    m_localUser.handlePUID = puidHandle;
    m_allocatedHandles.push_back(puidHandle);

    auto* eaidHandle = new OpaqueEpicAccountIdHandle();
    eaidHandle->magic = EAID_HANDLE_MAGIC;
    strcpy_s(eaidHandle->hexString, sizeof(eaidHandle->hexString), m_localUser.eaidString.c_str());
    m_localUser.handleEAID = eaidHandle;
    m_allocatedHandles.push_back(eaidHandle);

    ExternalAccountData epicData;
    epicData.accountType = EOS_EAT_EPIC;
    epicData.accountId = m_localUser.eaidString;
    epicData.displayName = m_localUser.displayName;
    m_localUser.externalAccounts.push_back(epicData);

    ExternalAccountData steamData;
    steamData.accountType = EOS_EAT_STEAM;
    steamData.accountId = std::to_string(m_localUser.steamId64);
    steamData.displayName = m_localUser.displayName;
    m_localUser.externalAccounts.push_back(steamData);

    m_recordsByPuidStr[m_localUser.puidString] = m_localUser;
    m_puidToPuidStr[m_localUser.handlePUID] = m_localUser.puidString;
    m_eaidToPuidStr[m_localUser.handleEAID] = m_localUser.puidString;
    m_steamIdToPuidStr[m_localUser.steamId64] = m_localUser.puidString;

    RefreshFromEnvironment();
}

void IdentityManager::RefreshFromEnvironment() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    char envName[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", envName, sizeof(envName)) > 0 && envName[0] != '\0') {
        SetLocalDisplayName(envName);
    } else if (GetEnvironmentVariableA("SteamPersonaName", envName, sizeof(envName)) > 0 && envName[0] != '\0') {
        SetLocalDisplayName(envName);
    }

    char envSteamId[64] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_ID", envSteamId, sizeof(envSteamId)) > 0 && envSteamId[0] != '\0') {
        uint64_t sid = _strtoui64(envSteamId, nullptr, 10);
        if (sid != 0) SetLocalSteamId(sid);
    } else if (GetEnvironmentVariableA("SteamId", envSteamId, sizeof(envSteamId)) > 0 && envSteamId[0] != '\0') {
        uint64_t sid = _strtoui64(envSteamId, nullptr, 10);
        if (sid != 0) SetLocalSteamId(sid);
    }
}

EOS_ProductUserId IdentityManager::GetLocalProductUserId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.handlePUID;
}

EOS_EpicAccountId IdentityManager::GetLocalEpicAccountId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.handleEAID;
}

const std::string& IdentityManager::GetLocalProductUserIdString() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.puidString;
}

const std::string& IdentityManager::GetLocalEpicAccountIdString() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.eaidString;
}

const std::string& IdentityManager::GetLocalDisplayName() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.displayName;
}

uint64_t IdentityManager::GetLocalSteamId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.steamId64;
}

const std::string& IdentityManager::GetLocalAccountUuid() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_localUser.accountUuid;
}

void IdentityManager::SetLocalDisplayName(const std::string& name) {
    if (name.empty()) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_localUser.displayName = name;
    for (auto& ext : m_localUser.externalAccounts) {
        ext.displayName = name;
    }
    m_recordsByPuidStr[m_localUser.puidString] = m_localUser;
}

void IdentityManager::SetLocalSteamId(uint64_t steamId) {
    if (steamId == 0) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_steamIdToPuidStr.erase(m_localUser.steamId64);
    m_localUser.steamId64 = steamId;
    m_steamIdToPuidStr[steamId] = m_localUser.puidString;
    for (auto& ext : m_localUser.externalAccounts) {
        if (ext.accountType == EOS_EAT_STEAM) {
            ext.accountId = std::to_string(steamId);
        }
    }
    m_recordsByPuidStr[m_localUser.puidString] = m_localUser;
}

bool IdentityManager::IsValidProductUserId(EOS_ProductUserId puid) {
    if (!puid) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_puidToPuidStr.find(puid);
    if (it == m_puidToPuidStr.end()) return false;
    auto* handle = (const OpaqueProductUserIdHandle*)puid;
    return (handle->magic == PUID_HANDLE_MAGIC);
}

bool IdentityManager::IsValidEpicAccountId(EOS_EpicAccountId eaid) {
    if (!eaid) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_eaidToPuidStr.find(eaid);
    if (it == m_eaidToPuidStr.end()) return false;
    auto* handle = (const OpaqueEpicAccountIdHandle*)eaid;
    return (handle->magic == EAID_HANDLE_MAGIC);
}

std::string IdentityManager::ProductUserIdToString(EOS_ProductUserId puid) {
    if (!puid) return "";
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_puidToPuidStr.find(puid);
    if (it != m_puidToPuidStr.end()) return it->second;
    return "";
}

std::string IdentityManager::EpicAccountIdToString(EOS_EpicAccountId eaid) {
    if (!eaid) return "";
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_eaidToPuidStr.find(eaid);
    if (it != m_eaidToPuidStr.end()) {
        auto recIt = m_recordsByPuidStr.find(it->second);
        if (recIt != m_recordsByPuidStr.end()) return recIt->second.eaidString;
    }
    return "";
}

EOS_ProductUserId IdentityManager::ProductUserIdFromString(const char* puidStr) {
    if (!puidStr || puidStr[0] == '\0') return nullptr;
    if (!IsValidHex32String(puidStr)) return nullptr;
    return GetOrCreateProductUserId(puidStr);
}

EOS_EpicAccountId IdentityManager::EpicAccountIdFromString(const char* eaidStr) {
    if (!eaidStr || eaidStr[0] == '\0') return nullptr;
    if (!IsValidHex32String(eaidStr)) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& pair : m_recordsByPuidStr) {
        if (pair.second.eaidString == eaidStr) {
            return pair.second.handleEAID;
        }
    }
    std::string puidStr = DerivePuidFromUuid("EAID_MAPPING:" + std::string(eaidStr));
    auto puid = GetOrCreateProductUserId(puidStr);
    auto recIt = m_recordsByPuidStr.find(puidStr);
    if (recIt != m_recordsByPuidStr.end()) {
        recIt->second.eaidString = eaidStr;
        return recIt->second.handleEAID;
    }
    return nullptr;
}

EOS_ProductUserId IdentityManager::GetOrCreateProductUserId(const std::string& puidStr) {
    if (puidStr.empty()) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = m_recordsByPuidStr.find(puidStr);
    if (it != m_recordsByPuidStr.end()) {
        return it->second.handlePUID;
    }

    UserIdentityRecord rec;
    rec.accountUuid = "";
    rec.puidString = puidStr;
    rec.eaidString = DeriveEaidFromUuid("PEER_EAID:" + puidStr);
    rec.displayName = "Player_" + (puidStr.length() >= 4 ? puidStr.substr(puidStr.length() - 4) : puidStr);
    rec.steamId64 = 0;

    auto* puidHandle = new OpaqueProductUserIdHandle();
    puidHandle->magic = PUID_HANDLE_MAGIC;
    strcpy_s(puidHandle->hexString, sizeof(puidHandle->hexString), rec.puidString.c_str());
    rec.handlePUID = puidHandle;
    m_allocatedHandles.push_back(puidHandle);

    auto* eaidHandle = new OpaqueEpicAccountIdHandle();
    eaidHandle->magic = EAID_HANDLE_MAGIC;
    strcpy_s(eaidHandle->hexString, sizeof(eaidHandle->hexString), rec.eaidString.c_str());
    rec.handleEAID = eaidHandle;
    m_allocatedHandles.push_back(eaidHandle);

    ExternalAccountData epicData;
    epicData.accountType = EOS_EAT_EPIC;
    epicData.accountId = rec.eaidString;
    epicData.displayName = rec.displayName;
    rec.externalAccounts.push_back(epicData);

    m_recordsByPuidStr[puidStr] = rec;
    m_puidToPuidStr[rec.handlePUID] = puidStr;
    m_eaidToPuidStr[rec.handleEAID] = puidStr;

    return rec.handlePUID;
}

EOS_ProductUserId IdentityManager::GetOrCreateProductUserIdFromSteamId(uint64_t steamId, const std::string& personaName) {
    if (steamId == 0) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = m_steamIdToPuidStr.find(steamId);
    if (it != m_steamIdToPuidStr.end()) {
        if (!personaName.empty()) {
            m_recordsByPuidStr[it->second].displayName = personaName;
        }
        return m_recordsByPuidStr[it->second].handlePUID;
    }

    std::string puidStr = DerivePuidFromUuid("STEAMID:" + std::to_string(steamId));
    auto puid = GetOrCreateProductUserId(puidStr);

    auto& rec = m_recordsByPuidStr[puidStr];
    rec.steamId64 = steamId;
    if (!personaName.empty()) rec.displayName = personaName;

    ExternalAccountData steamData;
    steamData.accountType = EOS_EAT_STEAM;
    steamData.accountId = std::to_string(steamId);
    steamData.displayName = rec.displayName;
    rec.externalAccounts.push_back(steamData);

    m_steamIdToPuidStr[steamId] = puidStr;
    return puid;
}

EOS_ProductUserId IdentityManager::GetOrCreateProductUserIdFromExternal(int32_t accountType, const std::string& externalId, const std::string& displayName) {
    if (externalId.empty()) return nullptr;
    if (accountType == EOS_EAT_STEAM) {
        uint64_t sid = _strtoui64(externalId.c_str(), nullptr, 10);
        return GetOrCreateProductUserIdFromSteamId(sid, displayName);
    }

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string puidStr = DerivePuidFromUuid("EXT:" + std::to_string(accountType) + ":" + externalId);
    auto puid = GetOrCreateProductUserId(puidStr);
    auto& rec = m_recordsByPuidStr[puidStr];
    if (!displayName.empty()) rec.displayName = displayName;

    ExternalAccountData extData;
    extData.accountType = accountType;
    extData.accountId = externalId;
    extData.displayName = rec.displayName;
    rec.externalAccounts.push_back(extData);

    return puid;
}

const UserIdentityRecord* IdentityManager::GetRecordByPUID(EOS_ProductUserId puid) {
    if (!puid) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_puidToPuidStr.find(puid);
    if (it != m_puidToPuidStr.end()) {
        auto recIt = m_recordsByPuidStr.find(it->second);
        if (recIt != m_recordsByPuidStr.end()) return &recIt->second;
    }
    return nullptr;
}

const UserIdentityRecord* IdentityManager::GetRecordByEAID(EOS_EpicAccountId eaid) {
    if (!eaid) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_eaidToPuidStr.find(eaid);
    if (it != m_eaidToPuidStr.end()) {
        auto recIt = m_recordsByPuidStr.find(it->second);
        if (recIt != m_recordsByPuidStr.end()) return &recIt->second;
    }
    return nullptr;
}

struct EOS_Connect_ExternalAccountInfo_Layout {
    int32_t ApiVersion;
    EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    int32_t AccountIdType;
    const char* AccountId;
    int64_t LastLoginTime;
};

void* IdentityManager::AllocateExternalAccountInfo(EOS_ProductUserId puid, int32_t accountTypeIndex, int32_t targetAccountType) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const UserIdentityRecord* rec = GetRecordByPUID(puid);
    if (!rec) rec = &m_localUser;

    const ExternalAccountData* selectedExt = nullptr;
    if (targetAccountType >= 0) {
        for (const auto& ext : rec->externalAccounts) {
            if (ext.accountType == targetAccountType) {
                selectedExt = &ext;
                break;
            }
        }
    } else if (accountTypeIndex >= 0 && (size_t)accountTypeIndex < rec->externalAccounts.size()) {
        selectedExt = &rec->externalAccounts[accountTypeIndex];
    }

    if (!selectedExt && !rec->externalAccounts.empty()) {
        selectedExt = &rec->externalAccounts[0];
    }

    if (!selectedExt) return nullptr;

    auto* info = (EOS_Connect_ExternalAccountInfo_Layout*)malloc(sizeof(EOS_Connect_ExternalAccountInfo_Layout));
    if (!info) return nullptr;

    info->ApiVersion = 1;
    info->ProductUserId = rec->handlePUID;
    info->DisplayName = _strdup(selectedExt->displayName.c_str());
    info->AccountIdType = selectedExt->accountType;
    info->AccountId = _strdup(selectedExt->accountId.c_str());
    info->LastLoginTime = 1600000000;

    return info;
}

void IdentityManager::FreeExternalAccountInfo(void* info) {
    if (!info) return;
    auto* layout = (EOS_Connect_ExternalAccountInfo_Layout*)info;
    if (layout->DisplayName) free((void*)layout->DisplayName);
    if (layout->AccountId) free((void*)layout->AccountId);
    free(layout);
}


void IdentityManager::LoadFromProfilePath(const std::string& customProfilePath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string uuid;
    std::string dispName = "ReFix Player";
    uint64_t steamId = 76561198000000001ULL;

    std::ifstream inFile(customProfilePath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            size_t pos = line.find("account_uuid");
            if (pos != std::string::npos) {
                size_t q1 = line.find('"', pos + 15);
                if (q1 != std::string::npos) {
                    size_t q2 = line.find('"', q1 + 1);
                    if (q2 != std::string::npos) {
                        uuid = line.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
            size_t npos = line.find("display_name");
            if (npos != std::string::npos) {
                size_t q1 = line.find('"', npos + 15);
                if (q1 != std::string::npos) {
                    size_t q2 = line.find('"', q1 + 1);
                    if (q2 != std::string::npos) {
                        dispName = line.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
        }
        inFile.close();
    }

    if (uuid.empty()) {
        uuid = GenerateRandomUUID();
    }

    m_localUser.accountUuid = uuid;
    m_localUser.displayName = dispName;
    m_localUser.steamId64 = steamId;
    m_localUser.puidString = DerivePuidFromUuid(uuid);
    m_localUser.eaidString = DeriveEaidFromUuid(uuid);

    m_localUser.handlePUID = (OpaqueProductUserIdHandle*)GetOrCreateProductUserId(m_localUser.puidString);
    m_localUser.handleEAID = m_recordsByPuidStr[m_localUser.puidString].handleEAID;

    m_localUser.externalAccounts.clear();
    m_localUser.externalAccounts.push_back({ EOS_EAT_EPIC, m_localUser.eaidString, m_localUser.displayName });
    m_localUser.externalAccounts.push_back({ EOS_EAT_STEAM, std::to_string(steamId), m_localUser.displayName });

    m_recordsByPuidStr[m_localUser.puidString] = m_localUser;
}

} // namespace ReFixEOS

extern "C" {

EOS_EResult EOS_ProductUserId_IsValid(EOS_ProductUserId Id) {
    return ReFixEOS::IdentityManager::Get().IsValidProductUserId(Id) ? 1 : 0;
}

EOS_EResult EOS_EpicAccountId_IsValid(EOS_EpicAccountId Id) {
    return ReFixEOS::IdentityManager::Get().IsValidEpicAccountId(Id) ? 1 : 0;
}

EOS_EResult EOS_ProductUserId_ToString(EOS_ProductUserId Id, char* OutBuffer, int32_t* InOutBufferLength) {
    if (!InOutBufferLength) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidProductUserId(Id)) return EOS_InvalidUser;
    std::string str = ReFixEOS::IdentityManager::Get().ProductUserIdToString(Id);
    if (str.empty()) return EOS_InvalidUser;
    int32_t needed = (int32_t)str.length() + 1;
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_LimitExceeded;
    }
    strcpy_s(OutBuffer, *InOutBufferLength, str.c_str());
    *InOutBufferLength = needed;
    return EOS_Success;
}

EOS_EResult EOS_EpicAccountId_ToString(EOS_EpicAccountId Id, char* OutBuffer, int32_t* InOutBufferLength) {
    if (!InOutBufferLength) return EOS_InvalidParameters;
    if (!ReFixEOS::IdentityManager::Get().IsValidEpicAccountId(Id)) return EOS_InvalidUser;
    std::string str = ReFixEOS::IdentityManager::Get().EpicAccountIdToString(Id);
    if (str.empty()) return EOS_InvalidUser;
    int32_t needed = (int32_t)str.length() + 1;
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_LimitExceeded;
    }
    strcpy_s(OutBuffer, *InOutBufferLength, str.c_str());
    *InOutBufferLength = needed;
    return EOS_Success;
}

EOS_ProductUserId EOS_ProductUserId_FromString(const char* ProductUserIdString) {
    return ReFixEOS::IdentityManager::Get().ProductUserIdFromString(ProductUserIdString);
}

EOS_EpicAccountId EOS_EpicAccountId_FromString(const char* EpicAccountIdString) {
    return ReFixEOS::IdentityManager::Get().EpicAccountIdFromString(EpicAccountIdString);
}

} // extern "C"