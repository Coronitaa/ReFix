#include "protocol_adapter.h"
#include <iostream>

namespace refix::mods {

const char* ProtocolTypeToString(NetworkProtocolType type) {
    switch (type) {
        case NetworkProtocolType::Steam:      return "Steam Networking";
        case NetworkProtocolType::EOS:        return "Epic Online Services";
        case NetworkProtocolType::Photon:     return "Photon PUN/Fusion";
        case NetworkProtocolType::LanCore:    return "ReFix LAN Core";
        case NetworkProtocolType::RawSockets: return "Raw Sockets (UDP/TCP)";
        case NetworkProtocolType::Discovery:  return "LAN Discovery";
        default:                              return "Unknown";
    }
}

// -----------------------------------------------------------------------------
// Base Generic Protocol Adapter Template
// -----------------------------------------------------------------------------
class GenericProtocolAdapter : public IProtocolAdapter {
public:
    GenericProtocolAdapter(NetworkProtocolType type, const char* name)
        : type_(type), name_(name) {}

    NetworkProtocolType GetType() const override { return type_; }
    const char* GetName() const override { return name_.c_str(); }
    bool IsActive() const override { return is_active_; }
    void SetActive(bool active) { is_active_ = active; }

    bool OnSendPacket(PeerHandle peer, uint32_t channel, const uint8_t* in_data, size_t in_len,
                     std::vector<uint8_t>& out_data, bool& drop) override {
        drop = false;
        if (in_len > 0 && in_data) {
            out_data.assign(in_data, in_data + in_len);
        }
        return true;
    }

    bool OnRecvPacket(PeerHandle peer, uint32_t channel, const uint8_t* in_data, size_t in_len,
                     std::vector<uint8_t>& out_data, bool& drop) override {
        drop = false;
        if (in_len > 0 && in_data) {
            out_data.assign(in_data, in_data + in_len);
        }
        return true;
    }

private:
    NetworkProtocolType type_;
    std::string name_;
    bool is_active_ = true;
};

// -----------------------------------------------------------------------------
// ProtocolAdapterManager
// -----------------------------------------------------------------------------
ProtocolAdapterManager& ProtocolAdapterManager::GetInstance() {
    static ProtocolAdapterManager s_instance;
    return s_instance;
}

ProtocolAdapterManager::ProtocolAdapterManager() {
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::Steam, "Steam Networking"));
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::EOS, "Epic Online Services"));
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::Photon, "Photon Engine"));
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::LanCore, "ReFix LAN Core"));
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::RawSockets, "Raw UDP/TCP Sockets"));
    RegisterAdapter(std::make_unique<GenericProtocolAdapter>(NetworkProtocolType::Discovery, "LAN Discovery"));
}

ProtocolAdapterManager::~ProtocolAdapterManager() = default;

void ProtocolAdapterManager::RegisterAdapter(std::unique_ptr<IProtocolAdapter> adapter) {
    if (adapter) {
        adapters_[adapter->GetType()] = std::move(adapter);
    }
}

IProtocolAdapter* ProtocolAdapterManager::GetAdapter(NetworkProtocolType type) const {
    auto it = adapters_.find(type);
    if (it != adapters_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ProtocolAdapterManager::ProcessSend(NetworkProtocolType proto, PeerHandle peer, uint32_t channel,
                                        const uint8_t* in_data, size_t in_len,
                                        std::vector<uint8_t>& out_data, bool& drop) {
    IProtocolAdapter* adapter = GetAdapter(proto);
    if (!adapter || !adapter->IsActive()) {
        drop = false;
        if (in_len > 0 && in_data) {
            out_data.assign(in_data, in_data + in_len);
        }
        return true;
    }

    return adapter->OnSendPacket(peer, channel, in_data, in_len, out_data, drop);
}

bool ProtocolAdapterManager::ProcessRecv(NetworkProtocolType proto, PeerHandle peer, uint32_t channel,
                                        const uint8_t* in_data, size_t in_len,
                                        std::vector<uint8_t>& out_data, bool& drop) {
    IProtocolAdapter* adapter = GetAdapter(proto);
    if (!adapter || !adapter->IsActive()) {
        drop = false;
        if (in_len > 0 && in_data) {
            out_data.assign(in_data, in_data + in_len);
        }
        return true;
    }

    return adapter->OnRecvPacket(peer, channel, in_data, in_len, out_data, drop);
}

} // namespace refix::mods
