#include "game_profiles.h"
#include <windows.h>
#include <algorithm>

namespace ReFix::Photon::Profiles {

    ProfileRegistry& ProfileRegistry::Instance() {
        static ProfileRegistry s_instance;
        return s_instance;
    }

    ProfileRegistry::ProfileRegistry() {
        LoadBuiltInProfiles();
    }

    void ProfileRegistry::LoadBuiltInProfiles() {
        // R.E.P.O. Profile
        GameProfile repo;
        repo.gameId = "REPO";
        repo.displayName = "R.E.P.O.";
        repo.engine = "Unity";
        repo.primaryProduct = PhotonProduct::PUN;
        repo.preferredTransport = PhotonTransportType::UDP;
        repo.defaultRegion = "sa";
        repo.defaultServerVersion = "1.0";
        repo.requiresVoice = true;
        repo.requiresChat = false;
        repo.defaultSendRate = 25;
        RegisterProfile(repo);

        // Phasmophobia Profile
        GameProfile phasmo;
        phasmo.gameId = "Phasmophobia";
        phasmo.displayName = "Phasmophobia";
        phasmo.engine = "Unity";
        phasmo.primaryProduct = PhotonProduct::PUN;
        phasmo.preferredTransport = PhotonTransportType::UDP;
        phasmo.defaultRegion = "sa";
        phasmo.defaultServerVersion = "1.0";
        phasmo.requiresVoice = true;
        phasmo.requiresChat = false;
        phasmo.defaultSendRate = 20;
        RegisterProfile(phasmo);

        // Roadside Research Profile
        GameProfile roadside;
        roadside.gameId = "RoadsideResearch";
        roadside.displayName = "Roadside Research";
        roadside.engine = "Unity";
        roadside.primaryProduct = PhotonProduct::Realtime;
        roadside.preferredTransport = PhotonTransportType::UDP;
        roadside.defaultRegion = "sa";
        roadside.defaultServerVersion = "1.0";
        roadside.requiresVoice = false;
        roadside.requiresChat = false;
        roadside.defaultSendRate = 20;
        RegisterProfile(roadside);

        // Tabletop Simulator Profile
        GameProfile tts;
        tts.gameId = "TabletopSimulator";
        tts.displayName = "Tabletop Simulator";
        tts.engine = "Unity";
        tts.primaryProduct = PhotonProduct::Realtime;
        tts.preferredTransport = PhotonTransportType::UDP;
        tts.defaultRegion = "sa";
        tts.defaultServerVersion = "1.0";
        tts.requiresVoice = false;
        tts.requiresChat = true;
        tts.defaultSendRate = 20;
        RegisterProfile(tts);
    }

    void ProfileRegistry::RegisterProfile(const GameProfile& profile) {
        m_profiles[profile.gameId] = profile;
    }

    bool ProfileRegistry::HasProfile(const std::string& gameId) const {
        return m_profiles.find(gameId) != m_profiles.end();
    }

    GameProfile ProfileRegistry::GetProfile(const std::string& gameId) const {
        auto it = m_profiles.find(gameId);
        if (it != m_profiles.end()) return it->second;

        // Default Fallback Profile
        GameProfile generic;
        generic.gameId = "Generic";
        generic.displayName = "Generic Photon Game";
        generic.engine = "Unity";
        generic.primaryProduct = PhotonProduct::Realtime;
        generic.preferredTransport = PhotonTransportType::UDP;
        generic.defaultRegion = "sa";
        generic.defaultServerVersion = "1.0";
        return generic;
    }

    std::vector<GameProfile> ProfileRegistry::GetAllProfiles() const {
        std::vector<GameProfile> res;
        for (const auto& [k, v] : m_profiles) {
            res.push_back(v);
        }
        return res;
    }

    GameProfile ProfileRegistry::DetectProfileFromEnvironment() const {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string sPath(exePath);
        std::transform(sPath.begin(), sPath.end(), sPath.begin(), ::tolower);

        if (sPath.find("repo") != std::string::npos || sPath.find("r.e.p.o") != std::string::npos) {
            return GetProfile("REPO");
        }
        if (sPath.find("phasmophobia") != std::string::npos) {
            return GetProfile("Phasmophobia");
        }
        if (sPath.find("roadside") != std::string::npos) {
            return GetProfile("RoadsideResearch");
        }
        if (sPath.find("tabletop") != std::string::npos) {
            return GetProfile("TabletopSimulator");
        }

        return GetProfile("Generic");
    }

} // namespace ReFix::Photon::Profiles
