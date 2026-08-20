#pragma once

#include "../core/photon_interfaces.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>

namespace ReFix::Photon::Transport {

    class UdpTransport : public IPhotonTransport {
    public:
        UdpTransport();
        virtual ~UdpTransport();

        bool Connect(const std::string& host, uint16_t port) override;
        void Disconnect() override;
        bool IsConnected() const override;

        bool SendPacket(const std::vector<uint8_t>& data, bool reliable = true) override;
        bool PollPacket(std::vector<uint8_t>& outData) override;

        PhotonTransportType GetType() const override { return PhotonTransportType::UDP; }
        NetworkMetrics GetMetrics() const override;

    private:
        SOCKET m_socket = INVALID_SOCKET;
        sockaddr_in m_remoteAddr{};
        bool m_connected = false;
        mutable std::mutex m_socketMutex;
    };

} // namespace ReFix::Photon::Transport
