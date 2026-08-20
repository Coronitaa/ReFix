#include "udp_transport.h"
#include "../diagnostics/photon_diagnostics.h"

#pragma comment(lib, "ws2_32.lib")

namespace ReFix::Photon::Transport {

    UdpTransport::UdpTransport() : m_socket(INVALID_SOCKET), m_connected(false) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    UdpTransport::~UdpTransport() {
        Disconnect();
        WSACleanup();
    }

    bool UdpTransport::Connect(const std::string& host, uint16_t port) {
        std::lock_guard<std::mutex> lock(m_socketMutex);

        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            Diagnostics::LogError(Diagnostics::LogChannel::Transport, "UdpTransport socket creation failed (WSA error %d)", WSAGetLastError());
            return false;
        }

        // Set non-blocking socket mode
        u_long mode = 1;
        ioctlsocket(m_socket, FIONBIO, &mode);

        std::memset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
        m_remoteAddr.sin_family = AF_INET;
        m_remoteAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &m_remoteAddr.sin_addr) <= 0) {
            struct hostent* he = gethostbyname(host.c_str());
            if (!he || !he->h_addr_list[0]) {
                Diagnostics::LogError(Diagnostics::LogChannel::Transport, "UdpTransport failed to resolve host '%s'", host.c_str());
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
                return false;
            }
            std::memcpy(&m_remoteAddr.sin_addr, he->h_addr_list[0], sizeof(in_addr));
        }

        m_connected = true;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "UdpTransport initialized for %s:%u", host.c_str(), port);
        return true;
    }

    void UdpTransport::Disconnect() {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
    }

    bool UdpTransport::IsConnected() const {
        return m_connected && (m_socket != INVALID_SOCKET);
    }

    bool UdpTransport::SendPacket(const std::vector<uint8_t>& data, bool reliable) {
        if (!IsConnected() || data.empty()) return false;

        std::lock_guard<std::mutex> lock(m_socketMutex);
        int sent = sendto(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()),
                          0, reinterpret_cast<const sockaddr*>(&m_remoteAddr), sizeof(m_remoteAddr));

        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                Diagnostics::LogError(Diagnostics::LogChannel::Transport, "UdpTransport sendto error: %d", err);
                return false;
            }
            return false;
        }

        Diagnostics::DiagnosticsEngine::Instance().RecordPacketSent(data.size());
        return true;
    }

    bool UdpTransport::PollPacket(std::vector<uint8_t>& outData) {
        if (!IsConnected()) return false;

        uint8_t recvBuffer[4096];
        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);

        std::lock_guard<std::mutex> lock(m_socketMutex);
        int bytesRead = recvfrom(m_socket, reinterpret_cast<char*>(recvBuffer), sizeof(recvBuffer),
                                 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (bytesRead == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAECONNRESET) {
                Diagnostics::LogDebug(Diagnostics::LogChannel::Transport, "UdpTransport recvfrom status: %d", err);
            }
            return false;
        }

        if (bytesRead > 0) {
            outData.assign(recvBuffer, recvBuffer + bytesRead);
            Diagnostics::DiagnosticsEngine::Instance().RecordPacketReceived(bytesRead);
            return true;
        }

        return false;
    }

    NetworkMetrics UdpTransport::GetMetrics() const {
        return Diagnostics::DiagnosticsEngine::Instance().GetMetrics();
    }

} // namespace ReFix::Photon::Transport
