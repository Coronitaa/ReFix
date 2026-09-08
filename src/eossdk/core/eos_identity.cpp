#include "eos_identity.h"
#include "refix_config.h"
#include "refix_hash.h"
#include "refix_log.h"
#include "eos_online.h"

namespace refix {

Identity& Identity::Get() { static Identity i; return i; }

Identity::Identity() {
    // Ids are scoped to the title so the same Steam account gets a different
    // ProductUserId in every game, exactly as real EOS does. The scope is the
    // EOS product id the game itself declared - nothing is hardcoded, and the
    // fallbacks in OnlineScope() cover titles that declare nothing.
    m_productScope = ToLower(OnlineScope());
    m_instanceTag  = Config::Get().GetString("User", "Instance", "");

    RFLOG(Auth, "Identity: productScope='%s' instance='%s'",
          m_productScope.c_str(), m_instanceTag.c_str());
}

std::vector<uint8_t> Identity::HexToBytes(const char* hex) {
    std::vector<uint8_t> out;
    if (!hex) return out;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    size_t n = std::strlen(hex);
    out.reserve(n / 2);
    for (size_t i = 0; i + 1 < n; i += 2) {
        int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}

// A Steam app/session ticket starts with a 20-byte GC block:
//   uint32 blockLength (20) | uint64 gcToken | uint64 steamId | uint32 timestamp
// so the owning account is available without contacting Steam at all.
uint64_t Identity::SteamIdFromTicket(const void* data, size_t len) {
    if (!data || len < 20) return 0;
    const uint8_t* p = (const uint8_t*)data;
    uint32_t blockLen = 0;
    std::memcpy(&blockLen, p, 4);
    if (blockLen != 20 || len < 24) return 0;
    uint64_t steamId = 0;
    std::memcpy(&steamId, p + 12, 8);
    // Individual accounts on the public universe: 0x0110000100000000 | accountId.
    if ((uint32_t)(steamId >> 32) != 0x01100001u) return 0;
    return steamId;
}

std::string Identity::HardwareFingerprint() const {
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD n = sizeof(name);
    GetComputerNameA(name, &n);
    DWORD volumeSerial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s|%08lX", name, (unsigned long)volumeSerial);
    return buf;
}

std::string Identity::DerivePuidFor(const std::string& externalId) const {
    return DerivedId("refix.eos.puid|" + m_productScope + "|" + externalId + "|", 16);
}

std::string Identity::DeriveEaidFor(const std::string& externalId) const {
    return DerivedId("refix.eos.eaid|" + m_productScope + "|" + externalId + "|", 16);
}

UserRecord Identity::BuildUser(const std::string& externalId, EOS_EExternalAccountType type,
                               const std::string& displayName) {
    const std::string salt = m_productScope + "|" + externalId + "|" + m_instanceTag;
    UserRecord rec;
    rec.Puid                 = DerivedId("refix.eos.puid|" + salt, 16);
    rec.Eaid                 = DerivedId("refix.eos.eaid|" + salt, 16);
    rec.DisplayName          = displayName;
    rec.External.Valid       = m_externalValid;
    rec.External.Type        = type;
    rec.External.AccountId   = externalId;
    rec.External.DisplayName = displayName;
    rec.External.LastLoginTime = UnixSeconds();
    rec.LoggedIn             = true;
    return rec;
}

const UserRecord& Identity::ResolveLocalUser(int32_t credentialType, const char* token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_resolved) return m_local;

    auto& cfg = Config::Get();

    std::string displayName = cfg.GetString("User", "Name", "");
    if (displayName.empty() || ToLower(displayName) == "player") {
        char persona[128] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", persona, sizeof(persona)) > 0 && persona[0])
            displayName = persona;
    }

    std::string externalId;
    EOS_EExternalAccountType type = EAT::EOS_EAT_STEAM;
    m_externalValid = true;
    const char* source = "none";

    // 1. Steam ticket handed to us by the title.
    if (token && *token) {
        std::vector<uint8_t> bytes = HexToBytes(token);
        uint64_t steamId = SteamIdFromTicket(bytes.data(), bytes.size());
        if (!steamId) steamId = SteamIdFromTicket(token, std::strlen(token));
        if (steamId) {
            externalId = std::to_string(steamId);
            source = "steam-ticket";
        }
    }

    // 2. An explicit [User] SteamId. Deliberately NOT [Unreal.Steam] SteamId:
    //    that key ships with a placeholder value, so honouring it would hand
    //    every player on every machine the same account.
    if (externalId.empty()) {
        std::string configured = Trim(cfg.GetString("User", "SteamId", ""));
        const bool numeric = !configured.empty() &&
                             configured.find_first_not_of("0123456789") == std::string::npos;
        if (numeric && configured != "76561198000000001") {
            externalId = configured;
            source = "config";
        }
    }

    // 3. Hardware fingerprint (DeviceId-style logins, or Steam unavailable).
    if (externalId.empty()) {
        // No external account system is involved: the player is identified by
        // the machine itself, exactly like an EOS DeviceId login.
        externalId = DerivedId("refix.device|" + HardwareFingerprint(), 8);
        type = EAT::EOS_EAT_EPIC;
        m_externalValid = false;
        source = "device";
    }

    if (displayName.empty())
        displayName = "Player-" + externalId.substr(externalId.size() > 4 ? externalId.size() - 4 : 0);

    m_local = BuildUser(externalId, type, displayName);
    m_resolved = true;

    if (credentialType < 0) {
        // Something asked who the player is before the title presented a
        // credential. The answer is then a fallback, not the real account, so
        // make that impossible to miss in the trace.
        RFLOG(Auth, "WARNING: identity resolved BEFORE EOS_Connect_Login, so the Steam "
                    "ticket was not available and a fallback was used.");
    }
    RFLOG(Auth, "Identity resolved via %s: external=%s type=%d name='%s' PUID=%s EAID=%s (credType=%d)",
          source, externalId.c_str(), (int)type, displayName.c_str(),
          m_local.Puid.c_str(), m_local.Eaid.c_str(), credentialType);
    return m_local;
}

const UserRecord& Identity::LocalUser() {
    if (!m_resolved) ResolveLocalUser(-1, nullptr);
    return m_local;
}

EOS_ProductUserId Identity::LocalPuid() { return IdRegistry::Get().Puid(LocalUser().Puid); }
EOS_EpicAccountId Identity::LocalEaid() { return IdRegistry::Get().Eaid(LocalUser().Eaid); }

void Identity::RememberPeer(const UserRecord& rec) {
    if (rec.Puid.empty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_peers[rec.Puid] = rec;
    IdRegistry::Get().Puid(rec.Puid);
}

const UserRecord& Identity::RememberExternalAccount(const std::string& externalId,
                                                   EOS_EExternalAccountType type,
                                                   const std::string& displayName) {
    // Derived without the instance tag on purpose: this is the id that player
    // derives for themselves on their own machine, which is what makes the
    // mapping agree across installs.
    const std::string puid = DerivePuidFor(externalId);
    const std::string eaid = DeriveEaidFor(externalId);
    std::lock_guard<std::mutex> lock(m_mutex);
    UserRecord& rec = m_peers[puid];
    rec.Puid = puid;
    rec.Eaid = eaid;
    if (!displayName.empty()) rec.DisplayName = displayName;
    rec.External.Valid     = true;
    rec.External.Type      = type;
    rec.External.AccountId = externalId;
    if (!displayName.empty()) rec.External.DisplayName = displayName;
    IdRegistry::Get().Puid(rec.Puid);
    IdRegistry::Get().Eaid(rec.Eaid);
    return rec;
}

const UserRecord* Identity::FindByPuid(const std::string& puid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_resolved && m_local.Puid == puid) return &m_local;
    auto it = m_peers.find(puid);
    return it == m_peers.end() ? nullptr : &it->second;
}

} // namespace refix
