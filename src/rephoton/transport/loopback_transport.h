#pragma once

#include "../core/photon_interfaces.h"
#include <queue>
#include <mutex>
#include <memory>

namespace ReFix::Photon::Transport {

    class LoopbackTransport : public IPhotonTransport {
    public:
        LoopbackTransport();
        virtual ~LoopbackTransport();

        // Connect to a peer loopback instance (simulating direct wire)
        void ConnectPeer(std::shared_ptr<LoopbackTransport> peer);

        bool Connect(const std::string& host, uint16_t port) override;
        void Disconnect() override;
        bool IsConnected() const override;

        bool SendPacket(const std::vector<uint8_t>& data, bool reliable = true) override;
        bool PollPacket(std::vector<uint8_t>& outData) override;

        PhotonTransportType GetType() const override { return PhotonTransportType::Loopback; }
        NetworkMetrics GetMetrics() const override;

        // Internal packet injection from peer
        void PushIncomingPacket(const std::vector<uint8_t>& data);

    private:
        bool m_connected = false;
        std::weak_ptr<LoopbackTransport> m_peer;
        std::queue<std::vector<uint8_t>> m_inbox;
        mutable std::mutex m_queueMutex;
    };

} // namespace ReFix::Photon::Transport
