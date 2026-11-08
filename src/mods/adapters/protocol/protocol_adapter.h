#pragma once

#include "../privacy_gate.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace refix::mods {

enum class NetworkProtocolType {
    Unknown = 0,
    Steam,
    EOS,
    Photon,
    LanCore,
    RawSockets,
    Discovery
};

const char* ProtocolTypeToString(NetworkProtocolType type);

class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;
    virtual NetworkProtocolType GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual bool IsActive() const = 0;

    // Intercept send: mods can modify payload or drop
    virtual bool OnSendPacket(PeerHandle peer, uint32_t channel, const uint8_t* in_data, size_t in_len,
                             std::vector<uint8_t>& out_data, bool& drop) = 0;

    // Intercept receive: mods can modify payload or drop
    virtual bool OnRecvPacket(PeerHandle peer, uint32_t channel, const uint8_t* in_data, size_t in_len,
                             std::vector<uint8_t>& out_data, bool& drop) = 0;
};

class ProtocolAdapterManager {
public:
    static ProtocolAdapterManager& GetInstance();

    ProtocolAdapterManager();
    ~ProtocolAdapterManager();

    void RegisterAdapter(std::unique_ptr<IProtocolAdapter> adapter);
    IProtocolAdapter* GetAdapter(NetworkProtocolType type) const;

    // Route packet through active protocol adapters
    bool ProcessSend(NetworkProtocolType proto, PeerHandle peer, uint32_t channel,
                     const uint8_t* in_data, size_t in_len,
                     std::vector<uint8_t>& out_data, bool& drop);

    bool ProcessRecv(NetworkProtocolType proto, PeerHandle peer, uint32_t channel,
                     const uint8_t* in_data, size_t in_len,
                     std::vector<uint8_t>& out_data, bool& drop);

private:
    std::map<NetworkProtocolType, std::unique_ptr<IProtocolAdapter>> adapters_;
};

} // namespace refix::mods
