#pragma once

#include "../core/photon_types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ReFix::Photon::Profiles {

    struct GameProfile {
        std::string gameId;
        std::string displayName;
        std::string engine = "Unity";
        PhotonProduct primaryProduct = PhotonProduct::Realtime;
        PhotonTransportType preferredTransport = PhotonTransportType::UDP;
        std::string defaultRegion = "sa";
        std::string defaultServerVersion = "1.0";
        bool requiresVoice = false;
        bool requiresChat = false;
        uint32_t defaultSendRate = 20;
    };

    class ProfileRegistry {
    public:
        static ProfileRegistry& Instance();

        void RegisterProfile(const GameProfile& profile);
        bool HasProfile(const std::string& gameId) const;
        GameProfile GetProfile(const std::string& gameId) const;
        std::vector<GameProfile> GetAllProfiles() const;

        GameProfile DetectProfileFromEnvironment() const;

    private:
        ProfileRegistry();
        void LoadBuiltInProfiles();

        std::unordered_map<std::string, GameProfile> m_profiles;
    };

} // namespace ReFix::Photon::Profiles
