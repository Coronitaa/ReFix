// =============================================================================
// ReFix Identity Provider Abstraction
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace ReFixIdentity {

enum class IdentityMode {
    Valve,
    Goldberg
};

struct AuthCredentialData {
    int32_t credentialType = 0; // EOS_ECT_STEAM_SESSION_TICKET (3), EOS_ECT_DEVICEID_ACCESS_TOKEN (5), EOS_ECT_EXTERNAL_ACCOUNT (18)
    std::string token;          // Hex encoded or raw token
    std::vector<uint8_t> rawTicket;
    uint32_t ticketHandle = 0;
};

class IOnlineIdentityProvider {
public:
    virtual ~IOnlineIdentityProvider() = default;

    virtual IdentityMode GetMode() const = 0;
    virtual uint64_t GetSteamId() = 0;
    virtual std::string GetDisplayName() = 0;
    virtual AuthCredentialData GetAuthCredential() = 0;
    virtual bool ValidateCredential(int32_t credentialType, const char* token) = 0;
    virtual std::string GetProductUserIdString() = 0;
    virtual bool IsAuthenticated() = 0;

    virtual void SetCapturedSteamTicket(const uint8_t* data, size_t size, uint32_t handle) = 0;
    virtual void SetCapturedSteamId(uint64_t steamId) = 0;
    virtual void SetCapturedDisplayName(const std::string& name) = 0;
};

std::shared_ptr<IOnlineIdentityProvider> GetActiveIdentityProvider();
void SetActiveIdentityProvider(std::shared_ptr<IOnlineIdentityProvider> provider);
void ResetActiveIdentityProvider();

std::shared_ptr<IOnlineIdentityProvider> CreateSteamOnlineIdentityProvider();
std::shared_ptr<IOnlineIdentityProvider> CreateGoldbergIdentityProvider();

} // namespace ReFixIdentity
