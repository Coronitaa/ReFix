#pragma once

#include "photon_types.h"
#include <string>

namespace ReFix::Photon {

    struct CustomPhotonConfig {
        std::string realtimeAppId;
        std::string voiceAppId;
        std::string chatAppId;
        std::string region = "sa";
        std::string serverAddress = "127.0.0.1";
        uint16_t serverPort = 5055;
        PhotonTransportType protocol = PhotonTransportType::UDP;
        std::string serverVersion = "1.0";
    };

    struct ReFixCloudConfig {
        std::string nameServerEndpoint = "127.0.0.1:5058";
        std::string masterServerEndpoint = "127.0.0.1:5055";
        std::string gameServerEndpoint = "127.0.0.1:5056";
        std::string preferredRegion = "sa";
        bool autoSelectLowestPing = true;
    };

    struct DiagnosticsConfig {
        bool enabled = true;
        std::string logLevel = "Info";
        bool logAuth = true;
        bool logTransport = false;
        bool logRealtime = true;
        bool logRoom = true;
        bool logVoice = false;
        bool logChat = false;
        bool redactAppIds = true;
    };

    struct PhotonConfiguration {
        bool enabled = true;
        BackendMode backend = BackendMode::ReFixCloud;
        std::string targetProfile = "Auto";

        CustomPhotonConfig custom;
        ReFixCloudConfig cloud;
        DiagnosticsConfig diagnostics;

        static PhotonConfiguration LoadFromIni(const std::string& iniPath);
        void SaveToIni(const std::string& iniPath) const;
    };

} // namespace ReFix::Photon
