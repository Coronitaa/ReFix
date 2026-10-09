#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <string_view>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <iomanip>

namespace refix::lan {

// =============================================================================
// PeerId: 128-bit Universally Unique Peer Identifier
// =============================================================================
struct PeerId {
    uint64_t high = 0;
    uint64_t low = 0;

    bool IsValid() const noexcept { return high != 0 || low != 0; }
    bool operator==(const PeerId& o) const noexcept { return high == o.high && low == o.low; }
    bool operator!=(const PeerId& o) const noexcept { return !(*this == o); }
    bool operator<(const PeerId& o) const noexcept {
        return high != o.high ? high < o.high : low < o.low;
    }

    std::string ToString() const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(16) << high
           << std::setw(16) << low;
        return ss.str();
    }

    static PeerId FromString(std::string_view str) {
        if (str.size() < 32) return {};
        PeerId id;
        try {
            id.high = std::stoull(std::string(str.substr(0, 16)), nullptr, 16);
            id.low = std::stoull(std::string(str.substr(16, 16)), nullptr, 16);
        } catch (...) {
            return {};
        }
        return id;
    }
};

// Machine Hardware Fingerprint Hash
using MachineId = uint64_t;

// =============================================================================
// LanEndpoint: IPv4 Host-Byte-Order Address & Port
// =============================================================================
struct LanEndpoint {
    uint32_t ipv4 = 0; // Host byte order (e.g. 0x7F000001 = 127.0.0.1)
    uint16_t port = 0; // Host byte order

    bool IsValid() const noexcept { return ipv4 != 0 && port != 0; }
    bool operator==(const LanEndpoint& o) const noexcept { return ipv4 == o.ipv4 && port == o.port; }
    bool operator!=(const LanEndpoint& o) const noexcept { return !(*this == o); }
    bool operator<(const LanEndpoint& o) const noexcept {
        return ipv4 != o.ipv4 ? ipv4 < o.ipv4 : port < o.port;
    }

    std::string ToString() const {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u",
                 (ipv4 >> 24) & 0xFF,
                 (ipv4 >> 16) & 0xFF,
                 (ipv4 >> 8) & 0xFF,
                 ipv4 & 0xFF,
                 port);
        return std::string(buf);
    }

    std::string ToIpString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 (ipv4 >> 24) & 0xFF,
                 (ipv4 >> 16) & 0xFF,
                 (ipv4 >> 8) & 0xFF,
                 ipv4 & 0xFF);
        return std::string(buf);
    }

    static LanEndpoint Parse(std::string_view hostPort) {
        size_t colon = hostPort.find(':');
        if (colon == std::string_view::npos) return {};
        std::string host(hostPort.substr(0, colon));
        std::string pstr(hostPort.substr(colon + 1));
        unsigned int a, b, c, d, p;
        if (sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return {};
        p = std::stoul(pstr);
        LanEndpoint ep;
        ep.ipv4 = ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | (d & 0xFF);
        ep.port = static_cast<uint16_t>(p);
        return ep;
    }
};

// =============================================================================
// External Identity Mapping (Steam, EOS, Photon, Unity)
// =============================================================================
enum class ExternalPlatform : uint8_t {
    None = 0,
    Steam = 1,
    EOS = 2,
    Photon = 3,
    Unity = 4,
    Custom = 5
};

struct ExternalId {
    ExternalPlatform platform = ExternalPlatform::None;
    uint64_t numericId = 0; // SteamID64, Unity ClientId
    std::string stringId;   // EOS PUID (32 hex chars), Photon UserID
};

// =============================================================================
// Lobby Attributes & Types
// =============================================================================
enum class AttributeType : uint8_t {
    String,
    Int64,
    Double,
    Boolean
};

struct AttributeValue {
    AttributeType type = AttributeType::String;
    int64_t asInt64 = 0;
    double asDouble = 0.0;
    bool asBool = false;
    std::string asString;

    AttributeValue() = default;
    explicit AttributeValue(std::string_view s) : type(AttributeType::String), asString(s) {}
    explicit AttributeValue(const char* s) : type(AttributeType::String), asString(s ? s : "") {}
    explicit AttributeValue(const std::string& s) : type(AttributeType::String), asString(s) {}
    explicit AttributeValue(int64_t v) : type(AttributeType::Int64), asInt64(v) {}
    explicit AttributeValue(int32_t v) : type(AttributeType::Int64), asInt64(v) {}
    explicit AttributeValue(double v) : type(AttributeType::Double), asDouble(v) {}
    explicit AttributeValue(bool v) : type(AttributeType::Boolean), asBool(v) {}

    bool operator==(const AttributeValue& o) const {
        if (type != o.type) return false;
        switch (type) {
            case AttributeType::String:  return asString == o.asString;
            case AttributeType::Int64:   return asInt64 == o.asInt64;
            case AttributeType::Double:  return asDouble == o.asDouble;
            case AttributeType::Boolean: return asBool == o.asBool;
        }
        return false;
    }

    int Compare(const AttributeValue& o) const {
        if (type != o.type) return (type < o.type) ? -1 : 1;
        switch (type) {
            case AttributeType::String:  return asString.compare(o.asString);
            case AttributeType::Int64:   return (asInt64 < o.asInt64) ? -1 : ((asInt64 > o.asInt64) ? 1 : 0);
            case AttributeType::Double:  return (asDouble < o.asDouble) ? -1 : ((asDouble > o.asDouble) ? 1 : 0);
            case AttributeType::Boolean: return (asBool == o.asBool) ? 0 : (asBool ? 1 : -1);
        }
        return 0;
    }
};

enum class ComparisonOp : uint8_t {
    Equal,
    NotEqual,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    Contains,
    AnyOf,
    NotAnyOf
};

enum class LobbyPermissionLevel : uint8_t {
    PublicAdvertised,
    InviteOnly,
    FriendsOnly
};

enum class MemberLeaveReason : uint8_t {
    LeftGracefully,
    Disconnected,
    Kicked,
    HostClosed
};

enum class TrafficType : uint8_t {
    Discovery = 0,
    Signaling = 1,
    Game = 2,
    Voice = 3
};

} // namespace refix::lan

namespace std {
template <>
struct hash<refix::lan::PeerId> {
    size_t operator()(const refix::lan::PeerId& id) const noexcept {
        return static_cast<size_t>(id.high ^ (id.low * 0x9E3779B97F4A7C15ULL));
    }
};

template <>
struct hash<refix::lan::LanEndpoint> {
    size_t operator()(const refix::lan::LanEndpoint& ep) const noexcept {
        return static_cast<size_t>(ep.ipv4 ^ (static_cast<uint32_t>(ep.port) << 16));
    }
};
} // namespace std
