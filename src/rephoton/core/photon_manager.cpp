#include "photon_manager.h"
#include "../diagnostics/photon_diagnostics.h"
#include "../backend/refix_cloud_backend.h"
#include "../backend/custom_photon_backend.h"
#include <windows.h>

namespace ReFix::Photon {

    PhotonManager& PhotonManager::Instance() {
        static PhotonManager s_instance;
        return s_instance;
    }

    PhotonManager::PhotonManager() = default;

    PhotonManager::~PhotonManager() {
        Shutdown();
    }

    bool PhotonManager::Initialize(const std::string& iniPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return true;

        std::string targetIni = iniPath;
        if (targetIni.empty()) {
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            std::string sPath(exePath);
            size_t pos = sPath.find_last_of("\\/");
            if (pos != std::string::npos) {
                targetIni = sPath.substr(0, pos + 1) + "ReFix.ini";
            } else {
                targetIni = "ReFix.ini";
            }
        }

        m_config = PhotonConfiguration::LoadFromIni(targetIni);

        // Apply diagnostics settings
        auto& diag = Diagnostics::DiagnosticsEngine::Instance();
        diag.SetLoggingEnabled(m_config.diagnostics.enabled);
        diag.SetRedactAppIds(m_config.diagnostics.redactAppIds);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Auth, m_config.diagnostics.logAuth);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Transport, m_config.diagnostics.logTransport);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Realtime, m_config.diagnostics.logRealtime);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Room, m_config.diagnostics.logRoom);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Voice, m_config.diagnostics.logVoice);
        diag.SetChannelEnabled(Diagnostics::LogChannel::Chat, m_config.diagnostics.logChat);

        if (!m_config.enabled) {
            Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Re:Photon subsystem is disabled in configuration");
            m_initialized = true;
            return true;
        }

        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Initializing Re:Photon Subsystem (Backend: %s)",
                             BackendModeToString(m_config.backend));

        // Detect or load game profile
        if (m_config.targetProfile == "Auto" || m_config.targetProfile.empty()) {
            m_activeProfile = Profiles::ProfileRegistry::Instance().DetectProfileFromEnvironment();
        } else {
            m_activeProfile = Profiles::ProfileRegistry::Instance().GetProfile(m_config.targetProfile);
        }

        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Active Game Profile: %s (Engine: %s, Product: %s)",
                             m_activeProfile.displayName.c_str(), m_activeProfile.engine.c_str(),
                             PhotonProductToString(m_activeProfile.primaryProduct));

        SetupActiveBackend();
        m_initialized = true;
        return true;
    }

    void PhotonManager::SetupActiveBackend() {
        if (m_config.backend == BackendMode::CustomPhoton) {
            m_backend = std::make_shared<Backend::CustomPhotonBackend>(m_config.custom);
        } else {
            m_backend = std::make_shared<Backend::ReFixCloudBackend>(m_config.cloud);
        }

        m_realtimeClient = std::make_shared<Realtime::RealtimeClient>(m_backend);
        m_punAdapter = std::make_shared<PUN::PUNAdapter>(m_realtimeClient);

        m_backend->Initialize();
    }

    void PhotonManager::SwitchBackend(BackendMode mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_config.backend == mode && m_backend) return;

        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Switching backend: %s -> %s",
                             BackendModeToString(m_config.backend), BackendModeToString(mode));

        if (m_backend) {
            m_backend->Shutdown();
        }

        m_config.backend = mode;
        SetupActiveBackend();
    }

    void PhotonManager::Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return;

        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Shutting down Re:Photon Subsystem");

        if (m_backend) {
            m_backend->Shutdown();
            m_backend.reset();
        }
        m_punAdapter.reset();
        m_realtimeClient.reset();

        m_initialized = false;
    }

    void PhotonManager::Update() {
        if (!m_initialized || !m_config.enabled) return;

        if (m_backend) {
            m_backend->Update();
        }
    }

} // namespace ReFix::Photon
