// =============================================================================
// ReFix Identity Provider Implementation
// =============================================================================
#include "online_identity_provider.h"
#include <windows.h>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace ReFixIdentity {

static std::string BytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

static uint64_t ComputeMachineHash() {
    char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD compLen = sizeof(compName);
    GetComputerNameA(compName, &compLen);

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);

    uint64_t h = 14695981039346656037ULL;
    for (const char* p = compName; *p; ++p) {
        h ^= (uint8_t)*p;
        h *= 1099511628211ULL;
    }
    h ^= (uint64_t)volSerial;
    h *= 1099511628211ULL;
    return h ? h : 0x123456789ABCDEF0ULL;
}

static std::string ComputeDeterministicPuid(uint64_t steamId, const std::string& prefix = "steam") {
    // 32-character hex EOS ProductUserId format
    uint64_t p1 = 0xcbf29ce484222325ULL;
    uint64_t p2 = 0x100000001b3ULL;

    for (char c : prefix) {
        p1 = (p1 ^ (uint8_t)c) * 1099511628211ULL;
    }
    p1 = (p1 ^ (steamId & 0xFFFFFFFF)) * 1099511628211ULL;
    p2 = (p2 ^ ((steamId >> 32) & 0xFFFFFFFF)) * 1099511628211ULL;
    p2 = (p2 ^ (steamId & 0xFFFFFFFF)) * 1099511628211ULL;

    char buf[64];
    sprintf_s(buf, "%016llx%016llx", (unsigned long long)p1, (unsigned long long)p2);
    return std::string(buf);
}

// -----------------------------------------------------------------------------
// SteamOnlineIdentityProvider (Mode = valve)
// -----------------------------------------------------------------------------
class SteamOnlineIdentityProvider : public IOnlineIdentityProvider {
public:
    SteamOnlineIdentityProvider()
        : m_steamId(0), m_ticketHandle(0), m_displayName("Player") {
        RefreshFromEnvironment();
    }

    IdentityMode GetMode() const override { return IdentityMode::Valve; }

    uint64_t GetSteamId() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_steamId;
    }

    std::string GetDisplayName() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_displayName;
    }

    AuthCredentialData GetAuthCredential() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        AuthCredentialData cred;
        cred.credentialType = 3; // EOS_ECT_STEAM_SESSION_TICKET
        cred.rawTicket = m_ticketBytes;
        cred.ticketHandle = m_ticketHandle;
        if (!m_ticketBytes.empty()) {
            cred.token = BytesToHex(m_ticketBytes.data(), m_ticketBytes.size());
        }
        return cred;
    }

    bool ValidateCredential(int32_t credentialType, const char* token) override {
        if (!token) return false;
        size_t len = strlen(token);
        if (len < 4) return false;

        // In Valve mode, external auth / steam tickets / device ID fallback are supported
        if (credentialType == 3 /* STEAM_SESSION_TICKET */ ||
            credentialType == 4 /* STEAM_APP_TICKET */ ||
            credentialType == 18 /* EXTERNAL_ACCOUNT */) {
            // Must have valid non-trivial token
            return len >= 8;
        } else if (credentialType == 5 /* DEVICEID_ACCESS_TOKEN */) {
            return len >= 4;
        }
        return len >= 4;
    }

    std::string GetProductUserIdString() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t sid = m_steamId;
        if (sid == 0) {
            sid = 76561197960265728ULL + (uint32_t)(ComputeMachineHash() & 0x0FFFFFFF);
        }
        return ComputeDeterministicPuid(sid, "valve");
    }

    bool IsAuthenticated() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return (m_steamId != 0);
    }

    void SetCapturedSteamTicket(const uint8_t* data, size_t size, uint32_t handle) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (data && size > 0) {
            m_ticketBytes.assign(data, data + size);
        }
        m_ticketHandle = handle;
    }

    void SetCapturedSteamId(uint64_t steamId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (steamId != 0) {
            m_steamId = steamId;
        }
    }

    void SetCapturedDisplayName(const std::string& name) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!name.empty()) {
            m_displayName = name;
        }
    }

private:
    void RefreshFromEnvironment() {
        char envBuf[128] = { 0 };
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0) {
            uint64_t sid = _strtoui64(envBuf, nullptr, 10);
            if (sid != 0) m_steamId = sid;
        }
        if (GetEnvironmentVariableA("REFIX_USERNAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            m_displayName = envBuf;
        }
    }

    std::mutex m_mutex;
    uint64_t m_steamId;
    uint32_t m_ticketHandle;
    std::string m_displayName;
    std::vector<uint8_t> m_ticketBytes;
};

// -----------------------------------------------------------------------------
// GoldbergIdentityProvider (Mode = goldberg)
// -----------------------------------------------------------------------------
class GoldbergIdentityProvider : public IOnlineIdentityProvider {
public:
    GoldbergIdentityProvider()
        : m_steamId(0), m_ticketHandle(1), m_displayName("Player") {
        RefreshFromEnvironment();
    }

    IdentityMode GetMode() const override { return IdentityMode::Goldberg; }

    uint64_t GetSteamId() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_steamId == 0) {
            uint64_t mHash = ComputeMachineHash();
            uint32_t accountId = (uint32_t)(mHash & 0x0FFFFFFF);
            if (accountId == 0) accountId = 100001;
            m_steamId = 76561197960265728ULL + accountId;
        }
        return m_steamId;
    }

    std::string GetDisplayName() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_displayName;
    }

    AuthCredentialData GetAuthCredential() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        AuthCredentialData cred;
        cred.credentialType = 5; // EOS_ECT_DEVICEID_ACCESS_TOKEN
        cred.ticketHandle = m_ticketHandle;
        cred.token = "goldberg_token_" + std::to_string(m_steamId);
        return cred;
    }

    bool ValidateCredential(int32_t credentialType, const char* token) override {
        if (!token) return false;
        return strlen(token) >= 4;
    }

    std::string GetProductUserIdString() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t sid = m_steamId;
        if (sid == 0) {
            uint64_t mHash = ComputeMachineHash();
            uint32_t accountId = (uint32_t)(mHash & 0x0FFFFFFF);
            if (accountId == 0) accountId = 100001;
            sid = 76561197960265728ULL + accountId;
        }
        return ComputeDeterministicPuid(sid, "goldberg");
    }

    bool IsAuthenticated() override {
        return true;
    }

    void SetCapturedSteamTicket(const uint8_t* data, size_t size, uint32_t handle) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (data && size > 0) {
            m_ticketBytes.assign(data, data + size);
        }
        m_ticketHandle = handle;
    }

    void SetCapturedSteamId(uint64_t steamId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (steamId != 0) m_steamId = steamId;
    }

    void SetCapturedDisplayName(const std::string& name) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!name.empty()) m_displayName = name;
    }

private:
    void RefreshFromEnvironment() {
        char envBuf[128] = { 0 };
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0) {
            uint64_t sid = _strtoui64(envBuf, nullptr, 10);
            if (sid != 0) m_steamId = sid;
        }
        if (GetEnvironmentVariableA("REFIX_USERNAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            m_displayName = envBuf;
        }
    }

    std::mutex m_mutex;
    uint64_t m_steamId;
    uint32_t m_ticketHandle;
    std::string m_displayName;
    std::vector<uint8_t> m_ticketBytes;
};

static std::shared_ptr<IOnlineIdentityProvider> g_activeProvider = nullptr;
static std::mutex g_providerMutex;

std::shared_ptr<IOnlineIdentityProvider> CreateSteamOnlineIdentityProvider() {
    return std::make_shared<SteamOnlineIdentityProvider>();
}

std::shared_ptr<IOnlineIdentityProvider> CreateGoldbergIdentityProvider() {
    return std::make_shared<GoldbergIdentityProvider>();
}

std::shared_ptr<IOnlineIdentityProvider> GetActiveIdentityProvider() {
    std::lock_guard<std::mutex> lock(g_providerMutex);
    if (!g_activeProvider) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string iniPath(exePath);
        size_t pos = iniPath.find_last_of("\\/");
        if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";
        else iniPath = "ReFix.ini";

        char modeBuf[32] = { 0 };
        GetPrivateProfileStringA("Online", "Mode", "valve", modeBuf, sizeof(modeBuf), iniPath.c_str());

        char envMode[32] = { 0 };
        if (GetEnvironmentVariableA("REFIX_ONLINE_MODE", envMode, sizeof(envMode)) > 0 && envMode[0] != '\0') {
            strcpy_s(modeBuf, sizeof(modeBuf), envMode);
        }

        if (_stricmp(modeBuf, "goldberg") == 0) {
            g_activeProvider = CreateGoldbergIdentityProvider();
        } else {
            g_activeProvider = CreateSteamOnlineIdentityProvider();
        }
    }
    return g_activeProvider;
}

void SetActiveIdentityProvider(std::shared_ptr<IOnlineIdentityProvider> provider) {
    std::lock_guard<std::mutex> lock(g_providerMutex);
    g_activeProvider = provider;
}

void ResetActiveIdentityProvider() {
    std::lock_guard<std::mutex> lock(g_providerMutex);
    g_activeProvider = nullptr;
}

} // namespace ReFixIdentity
