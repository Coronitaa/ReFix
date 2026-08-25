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

std::string BytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

bool HexToBytes(const char* hexStr, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    if (!hexStr) return false;
    size_t len = strlen(hexStr);
    if (len == 0 || (len % 2) != 0) return false;
    outBytes.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        char c1 = hexStr[i];
        char c2 = hexStr[i + 1];
        int v1 = -1, v2 = -1;
        if (c1 >= '0' && c1 <= '9') v1 = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f') v1 = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F') v1 = c1 - 'A' + 10;
        else return false;

        if (c2 >= '0' && c2 <= '9') v2 = c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f') v2 = c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F') v2 = c2 - 'A' + 10;
        else return false;

        outBytes.push_back((uint8_t)((v1 << 4) | v2));
    }
    return true;
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
        if (len == 0) return false;

        std::lock_guard<std::mutex> lock(m_mutex);

        // In Valve mode, we expect a real Steam auth session ticket or external account credential
        if (credentialType == 1 /* EOS_ECT_STEAM_APP_TICKET / STEAM_SESSION_TICKET */ ||
            credentialType == 3 /* STEAM_SESSION_TICKET */ ||
            credentialType == 4 /* STEAM_APP_TICKET */ ||
            credentialType == 18 /* EXTERNAL_ACCOUNT / EPIC_ID_TOKEN */) {

            // If no Steam ticket was captured yet, try to refresh from environment/steam_api64.dll
            if (m_ticketBytes.empty()) {
                RefreshFromEnvironment();
            }

            // If still no Steam ticket was captured from Steam client or handle is invalid, reject!
            if (m_ticketBytes.empty() || m_ticketHandle == 0) {
                return false;
            }

            // 1. Try hex decoding the incoming token
            std::vector<uint8_t> candidateBytes;
            if (HexToBytes(token, candidateBytes)) {
                // Exact length and byte match against captured ticket
                if (candidateBytes.size() == m_ticketBytes.size() && candidateBytes == m_ticketBytes) {
                    return true;
                }
            }

            // 2. Direct binary memory match
            if (len == m_ticketBytes.size()) {
                if (memcmp(token, m_ticketBytes.data(), len) == 0) {
                    return true;
                }
            }

            // If token does not match the real Steam ticket from Steam client, reject!
            return false;
        } else if (credentialType == 5 || credentialType == 11 /* DEVICEID_ACCESS_TOKEN */) {
            return len >= 4;
        }

        return false;
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

    void InvalidateCapturedTicket(uint32_t handle = 0) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (handle == 0 || handle == m_ticketHandle) {
            m_ticketBytes.clear();
            m_ticketHandle = 0;
            SetEnvironmentVariableA("REFIX_STEAM_AUTH_TICKET", "");
            SetEnvironmentVariableA("REFIX_STEAM_AUTH_HANDLE", "0");
        }
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

    std::string GetCapturedTicketHex() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ticketBytes.empty()) return "";
        return BytesToHex(m_ticketBytes.data(), m_ticketBytes.size());
    }

    uint32_t GetCapturedTicketHandle() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_ticketHandle;
    }

    bool HasCapturedTicket() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_ticketBytes.empty();
    }

private:
    void RefreshFromEnvironment() {
        char envBuf[512] = { 0 };
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0) {
            uint64_t sid = _strtoui64(envBuf, nullptr, 10);
            if (sid != 0) m_steamId = sid;
        }
        if (GetEnvironmentVariableA("REFIX_USERNAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            m_displayName = envBuf;
        }

        if (m_ticketBytes.empty()) {
            char ticketHex[4096] = { 0 };
            if (GetEnvironmentVariableA("REFIX_STEAM_AUTH_TICKET", ticketHex, sizeof(ticketHex)) > 0 && ticketHex[0] != '\0') {
                std::vector<uint8_t> tb;
                if (HexToBytes(ticketHex, tb) && !tb.empty()) {
                    m_ticketBytes = std::move(tb);
                }
            }
            char handleBuf[32] = { 0 };
            if (GetEnvironmentVariableA("REFIX_STEAM_AUTH_HANDLE", handleBuf, sizeof(handleBuf)) > 0) {
                m_ticketHandle = (uint32_t)strtoul(handleBuf, nullptr, 10);
            }
        }

        if (m_ticketBytes.empty()) {
            HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
            if (hSteam) {
                typedef bool (*fn_GetTicketData_t)(uint8_t* outBuf, size_t maxLen, size_t* outLen, uint32_t* outHandle);
                auto pfn = (fn_GetTicketData_t)GetProcAddress(hSteam, "ReFix_Steam_GetCapturedTicketData");
                if (pfn) {
                    uint8_t buf[2048] = { 0 };
                    size_t len = 0;
                    uint32_t handle = 0;
                    if (pfn(buf, sizeof(buf), &len, &handle) && len > 0) {
                        m_ticketBytes.assign(buf, buf + len);
                        m_ticketHandle = handle;
                    }
                }
            }
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
        size_t len = strlen(token);
        if (len < 4) return false;

        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t sid = m_steamId;
        if (sid == 0) {
            uint64_t mHash = ComputeMachineHash();
            uint32_t accountId = (uint32_t)(mHash & 0x0FFFFFFF);
            if (accountId == 0) accountId = 100001;
            sid = 76561197960265728ULL + accountId;
        }

        std::string expectedToken = "goldberg_token_" + std::to_string(sid);
        if (_stricmp(token, expectedToken.c_str()) == 0) return true;

        if (strncmp(token, "goldberg_", 9) == 0) return true;

        std::vector<uint8_t> candidateBytes;
        if (HexToBytes(token, candidateBytes)) {
            if (!m_ticketBytes.empty() && candidateBytes == m_ticketBytes) return true;
        }

        return false;
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

    void InvalidateCapturedTicket(uint32_t handle = 0) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (handle == 0 || handle == m_ticketHandle) {
            m_ticketBytes.clear();
            m_ticketHandle = 0;
            SetEnvironmentVariableA("REFIX_STEAM_AUTH_TICKET", "");
            SetEnvironmentVariableA("REFIX_STEAM_AUTH_HANDLE", "0");
        }
    }

    void SetCapturedSteamId(uint64_t steamId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (steamId != 0) m_steamId = steamId;
    }

    void SetCapturedDisplayName(const std::string& name) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!name.empty()) m_displayName = name;
    }

    std::string GetCapturedTicketHex() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ticketBytes.empty()) {
            std::string t = "goldberg_ticket_" + std::to_string(m_steamId);
            return BytesToHex((const uint8_t*)t.data(), t.size());
        }
        return BytesToHex(m_ticketBytes.data(), m_ticketBytes.size());
    }

    uint32_t GetCapturedTicketHandle() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_ticketHandle;
    }

    bool HasCapturedTicket() override {
        return true;
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
