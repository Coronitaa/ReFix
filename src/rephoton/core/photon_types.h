#pragma once

#include <cstdint>
#include <string>

namespace ReFix::Photon {

    enum class PhotonProduct {
        Unknown = 0,
        Realtime,
        PUN,
        Fusion,
        FusionShared,
        Quantum,
        Voice,
        Chat
    };

    enum class PhotonTransportType {
        Unknown = 0,
        UDP,
        TCP,
        WebSocket,
        SecureWebSocket,
        Loopback
    };

    enum class BackendMode {
        ReFixCloud = 0,
        CustomPhoton,
        OfficialPhoton
    };

    enum class PhotonRegion {
        Unknown = 0,
        SouthAmerica,   // "sa"
        NorthAmerica,   // "us" / "usw" / "use"
        Europe,         // "eu"
        Asia,           // "asia" / "jp"
        Custom
    };

    enum class ConnectionState {
        Disconnected = 0,
        ConnectingToNameServer,
        ConnectedToNameServer,
        Authenticating,
        ConnectingToMasterServer,
        ConnectedToMasterServer,
        InLobby,
        ConnectingToGameServer,
        ConnectedToGameServer,
        InRoom,
        Disconnecting
    };

    // Helper functions for string representations
    inline const char* PhotonProductToString(PhotonProduct prod) {
        switch (prod) {
            case PhotonProduct::Realtime:     return "Realtime";
            case PhotonProduct::PUN:          return "PUN";
            case PhotonProduct::Fusion:       return "Fusion";
            case PhotonProduct::FusionShared: return "FusionShared";
            case PhotonProduct::Quantum:      return "Quantum";
            case PhotonProduct::Voice:        return "Voice";
            case PhotonProduct::Chat:         return "Chat";
            default:                          return "Unknown";
        }
    }

    inline const char* PhotonTransportToString(PhotonTransportType trans) {
        switch (trans) {
            case PhotonTransportType::UDP:             return "UDP";
            case PhotonTransportType::TCP:             return "TCP";
            case PhotonTransportType::WebSocket:       return "WebSocket";
            case PhotonTransportType::SecureWebSocket: return "SecureWebSocket";
            case PhotonTransportType::Loopback:        return "Loopback";
            default:                                   return "Unknown";
        }
    }

    inline const char* BackendModeToString(BackendMode mode) {
        switch (mode) {
            case BackendMode::ReFixCloud:     return "ReFixCloud";
            case BackendMode::CustomPhoton:   return "CustomPhoton";
            case BackendMode::OfficialPhoton: return "OfficialPhoton";
            default:                          return "Unknown";
        }
    }

    inline const char* PhotonRegionToString(PhotonRegion reg) {
        switch (reg) {
            case PhotonRegion::SouthAmerica: return "sa";
            case PhotonRegion::NorthAmerica: return "us";
            case PhotonRegion::Europe:       return "eu";
            case PhotonRegion::Asia:         return "asia";
            case PhotonRegion::Custom:       return "custom";
            default:                         return "unknown";
        }
    }

    inline PhotonRegion StringToPhotonRegion(const std::string& str) {
        if (str == "sa" || str == "SouthAmerica") return PhotonRegion::SouthAmerica;
        if (str == "us" || str == "usw" || str == "use" || str == "NorthAmerica") return PhotonRegion::NorthAmerica;
        if (str == "eu" || str == "Europe") return PhotonRegion::Europe;
        if (str == "asia" || str == "jp" || str == "kr" || str == "Asia") return PhotonRegion::Asia;
        if (str == "custom" || str == "Custom") return PhotonRegion::Custom;
        return PhotonRegion::Unknown;
    }

    struct NetworkMetrics {
        uint64_t packetsSent = 0;
        uint64_t packetsReceived = 0;
        uint64_t bytesSent = 0;
        uint64_t bytesReceived = 0;
        uint32_t roundTripTimeMs = 0;
        float packetLossPercent = 0.0f;
        uint32_t reconnectAttempts = 0;
    };

} // namespace ReFix::Photon
