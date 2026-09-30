// ReFix EOS v3 - who the local player is.
//
// The emulator never invents an identity out of thin air. In order of
// preference it derives one from
//
//   1. the Steam session ticket the title itself hands to EOS_Connect_Login
//      (the SteamID64 is embedded in the ticket, so this works for *any* Steam
//      account on *any* machine, with no configuration);
//   2. an explicit [User] SteamId in ReFix.ini;
//   3. a stable hardware fingerprint, for titles that authenticate with
//      DeviceId instead of Steam.
//
// The resulting EOS ids are a SHA-256 of that external account id, so two
// machines running the same build always agree on what a given player's
// ProductUserId is - which is the whole reason peers can recognise each other.
#pragma once
#include "refix_common.h"
#include "eos_ids.h"

namespace refix {

struct ExternalAccount {
    bool                     Valid = false;   // false for DeviceId-style logins
    EOS_EExternalAccountType Type = EAT::EOS_EAT_STEAM;
    std::string              AccountId;    // e.g. SteamID64 in decimal
    std::string              DisplayName;
    int64_t                  LastLoginTime = 0;
};

struct UserRecord {
    std::string      Puid;                 // 32 hex chars
    std::string      Eaid;                 // 32 hex chars
    std::string      DisplayName;
    ExternalAccount  External;
    bool             LoggedIn = false;
};

class Identity {
public:
    static Identity& Get();

    // Called from EOS_Connect_Login. `credentialType` is EOS_EExternalCredentialType;
    // `token` is whatever the title supplied (hex ticket, device id, ...).
    // Returns the resolved local user, creating it on first use.
    const UserRecord& ResolveLocalUser(int32_t credentialType, const char* token);

    const UserRecord& LocalUser();
    bool              HasLocalUser() const { return m_resolved; }

    EOS_ProductUserId LocalPuid();
    EOS_EpicAccountId LocalEaid();

    // Remote players learned from the network. Returns the interned id.
    void              RememberPeer(const UserRecord& rec);

    // Records a player we know only by their external account - a Steam friend,
    // or an id a title asked us to map. Without this the derived ProductUserId
    // exists but nothing is known about it, and a title asking that id for its
    // external account count is told zero, which reads as "this account is not
    // linked to anything".
    const UserRecord& RememberExternalAccount(const std::string& externalId,
                                              EOS_EExternalAccountType type,
                                              const std::string& displayName);
    const UserRecord* FindByPuid(const std::string& puid) const;

    // The product scope every derived id is salted with, so two different games
    // never produce the same ProductUserId for the same Steam account.
    const std::string& ProductScope() const { return m_productScope; }

    // Instance suffix (REFIX_USER_INSTANCE). Empty for a normal single install;
    // set it to run a second, independent player on the same PC.
    const std::string& InstanceTag() const { return m_instanceTag; }

    // The ids another player with this external account would derive for
    // themselves. Because the derivation is pure, we can name a friend's
    // ProductUserId from their SteamID alone, without ever having met them.
    std::string DerivePuidFor(const std::string& externalId) const;
    std::string DeriveEaidFor(const std::string& externalId) const;

    static uint64_t SteamIdFromTicket(const void* data, size_t len);
    static std::vector<uint8_t> HexToBytes(const char* hex);

private:
    Identity();
    UserRecord BuildUser(const std::string& externalId, EOS_EExternalAccountType type,
                         const std::string& displayName);
    std::string HardwareFingerprint() const;

    mutable std::mutex                 m_mutex;
    std::string                        m_productScope;
    std::string                        m_instanceTag;
    UserRecord                         m_local;
    bool                               m_resolved = false;
    bool                               m_externalValid = true;
    std::map<std::string, UserRecord>  m_peers;
};

} // namespace refix
