#pragma once

#include "photon_interfaces.h"
#include "photon_config.h"
#include "../profiles/game_profiles.h"
#include "../realtime/realtime_client.h"
#include "../pun/pun_adapter.h"
#include <memory>
#include <mutex>

namespace ReFix::Photon {

    class PhotonManager {
    public:
        static PhotonManager& Instance();

        bool Initialize(const std::string& iniPath = "");
        void Shutdown();
        void Update();

        bool IsEnabled() const { return m_config.enabled; }
        PhotonConfiguration GetConfig() const { return m_config; }
        Profiles::GameProfile GetActiveProfile() const { return m_activeProfile; }

        std::shared_ptr<IPhotonBackend> GetBackend() const { return m_backend; }
        std::shared_ptr<Realtime::RealtimeClient> GetRealtimeClient() const { return m_realtimeClient; }
        std::shared_ptr<PUN::PUNAdapter> GetPUNAdapter() const { return m_punAdapter; }

        void SwitchBackend(BackendMode mode);

    private:
        PhotonManager();
        ~PhotonManager();

        void SetupActiveBackend();

        bool m_initialized = false;
        PhotonConfiguration m_config;
        Profiles::GameProfile m_activeProfile;

        std::shared_ptr<IPhotonBackend> m_backend;
        std::shared_ptr<Realtime::RealtimeClient> m_realtimeClient;
        std::shared_ptr<PUN::PUNAdapter> m_punAdapter;

        mutable std::mutex m_mutex;
    };

} // namespace ReFix::Photon
