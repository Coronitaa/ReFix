#include "photon_config.h"
#include <windows.h>
#include <algorithm>

namespace ReFix::Photon {

    static std::string ReadIniString(const char* section, const char* key, const char* defaultVal, const char* path) {
        char buf[1024];
        GetPrivateProfileStringA(section, key, defaultVal, buf, sizeof(buf), path);
        return std::string(buf);
    }

    static bool ReadIniBool(const char* section, const char* key, bool defaultVal, const char* path) {
        std::string val = ReadIniString(section, key, defaultVal ? "true" : "false", path);
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }

    static int ReadIniInt(const char* section, const char* key, int defaultVal, const char* path) {
        return GetPrivateProfileIntA(section, key, defaultVal, path);
    }

    PhotonConfiguration PhotonConfiguration::LoadFromIni(const std::string& iniPath) {
        PhotonConfiguration cfg;
        const char* p = iniPath.c_str();

        // [RePhoton]
        cfg.enabled = ReadIniBool("RePhoton", "Enabled", true, p);
        std::string backendStr = ReadIniString("RePhoton", "Backend", "ReFixCloud", p);
        if (backendStr == "CustomPhoton" || backendStr == "Custom") {
            cfg.backend = BackendMode::CustomPhoton;
        } else if (backendStr == "OfficialPhoton" || backendStr == "Official") {
            cfg.backend = BackendMode::OfficialPhoton;
        } else {
            cfg.backend = BackendMode::ReFixCloud;
        }
        cfg.targetProfile = ReadIniString("RePhoton", "Profile", "Auto", p);

        // [RePhoton.Custom]
        cfg.custom.realtimeAppId = ReadIniString("RePhoton.Custom", "RealtimeAppId", "", p);
        cfg.custom.voiceAppId = ReadIniString("RePhoton.Custom", "VoiceAppId", "", p);
        cfg.custom.chatAppId = ReadIniString("RePhoton.Custom", "ChatAppId", "", p);
        cfg.custom.region = ReadIniString("RePhoton.Custom", "Region", "sa", p);
        cfg.custom.serverAddress = ReadIniString("RePhoton.Custom", "ServerAddress", "127.0.0.1", p);
        cfg.custom.serverPort = static_cast<uint16_t>(ReadIniInt("RePhoton.Custom", "ServerPort", 5055, p));
        std::string protoStr = ReadIniString("RePhoton.Custom", "Protocol", "UDP", p);
        if (protoStr == "TCP") cfg.custom.protocol = PhotonTransportType::TCP;
        else if (protoStr == "WebSocket" || protoStr == "WS") cfg.custom.protocol = PhotonTransportType::WebSocket;
        else if (protoStr == "SecureWebSocket" || protoStr == "WSS") cfg.custom.protocol = PhotonTransportType::SecureWebSocket;
        else cfg.custom.protocol = PhotonTransportType::UDP;
        cfg.custom.serverVersion = ReadIniString("RePhoton.Custom", "ServerVersion", "1.0", p);

        // [RePhoton.Cloud]
        cfg.cloud.nameServerEndpoint = ReadIniString("RePhoton.Cloud", "NameServer", "127.0.0.1:5058", p);
        cfg.cloud.masterServerEndpoint = ReadIniString("RePhoton.Cloud", "MasterServer", "127.0.0.1:5055", p);
        cfg.cloud.gameServerEndpoint = ReadIniString("RePhoton.Cloud", "GameServer", "127.0.0.1:5056", p);
        cfg.cloud.preferredRegion = ReadIniString("RePhoton.Cloud", "PreferredRegion", "sa", p);
        cfg.cloud.autoSelectLowestPing = ReadIniBool("RePhoton.Cloud", "AutoSelectLowestPing", true, p);

        // [RePhoton.Diagnostics]
        cfg.diagnostics.logLevel = ReadIniString("RePhoton.Diagnostics", "LogLevel", "Info", p);
        cfg.diagnostics.logAuth = ReadIniBool("RePhoton.Diagnostics", "LogAuth", true, p);
        cfg.diagnostics.logTransport = ReadIniBool("RePhoton.Diagnostics", "LogTransport", false, p);
        cfg.diagnostics.logRealtime = ReadIniBool("RePhoton.Diagnostics", "LogRealtime", true, p);
        cfg.diagnostics.logRoom = ReadIniBool("RePhoton.Diagnostics", "LogRoom", true, p);
        cfg.diagnostics.logVoice = ReadIniBool("RePhoton.Diagnostics", "LogVoice", false, p);
        cfg.diagnostics.logChat = ReadIniBool("RePhoton.Diagnostics", "LogChat", false, p);
        cfg.diagnostics.redactAppIds = ReadIniBool("RePhoton.Diagnostics", "RedactAppIds", true, p);

        return cfg;
    }

    void PhotonConfiguration::SaveToIni(const std::string& iniPath) const {
        const char* p = iniPath.c_str();

        WritePrivateProfileStringA("RePhoton", "Enabled", enabled ? "true" : "false", p);
        WritePrivateProfileStringA("RePhoton", "Backend", BackendModeToString(backend), p);
        WritePrivateProfileStringA("RePhoton", "Profile", targetProfile.c_str(), p);

        WritePrivateProfileStringA("RePhoton.Custom", "RealtimeAppId", custom.realtimeAppId.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "VoiceAppId", custom.voiceAppId.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "ChatAppId", custom.chatAppId.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "Region", custom.region.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "ServerAddress", custom.serverAddress.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "ServerPort", std::to_string(custom.serverPort).c_str(), p);
        WritePrivateProfileStringA("RePhoton.Custom", "Protocol", PhotonTransportToString(custom.protocol), p);

        WritePrivateProfileStringA("RePhoton.Cloud", "NameServer", cloud.nameServerEndpoint.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Cloud", "MasterServer", cloud.masterServerEndpoint.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Cloud", "GameServer", cloud.gameServerEndpoint.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Cloud", "PreferredRegion", cloud.preferredRegion.c_str(), p);
        WritePrivateProfileStringA("RePhoton.Cloud", "AutoSelectLowestPing", cloud.autoSelectLowestPing ? "true" : "false", p);
    }

} // namespace ReFix::Photon
