#include "loopback_transport.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Transport {

    LoopbackTransport::LoopbackTransport() : m_connected(false) {}

    LoopbackTransport::~LoopbackTransport() {
        Disconnect();
    }

    void LoopbackTransport::ConnectPeer(std::shared_ptr<LoopbackTransport> peer) {
        m_peer = peer;
        m_connected = true;
    }

    bool LoopbackTransport::Connect(const std::string& host, uint16_t port) {
        m_connected = true;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Loopback transport connected (simulated endpoint %s:%u)", host.c_str(), port);
        return true;
    }

    void LoopbackTransport::Disconnect() {
        m_connected = false;
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_inbox.empty()) m_inbox.pop();
    }

    bool LoopbackTransport::IsConnected() const {
        return m_connected;
    }

    bool LoopbackTransport::SendPacket(const std::vector<uint8_t>& data, bool reliable) {
        if (!m_connected) return false;
        Diagnostics::DiagnosticsEngine::Instance().RecordPacketSent(data.size());

        auto peerLocked = m_peer.lock();
        if (peerLocked && peerLocked->IsConnected()) {
            peerLocked->PushIncomingPacket(data);
            return true;
        }

        // If no peer, we buffer to self for local echo test
        PushIncomingPacket(data);
        return true;
    }

    void LoopbackTransport::PushIncomingPacket(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_inbox.push(data);
        Diagnostics::DiagnosticsEngine::Instance().RecordPacketReceived(data.size());
    }

    bool LoopbackTransport::PollPacket(std::vector<uint8_t>& outData) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_inbox.empty()) return false;

        outData = std::move(m_inbox.front());
        m_inbox.pop();
        return true;
    }

    NetworkMetrics LoopbackTransport::GetMetrics() const {
        return Diagnostics::DiagnosticsEngine::Instance().GetMetrics();
    }

} // namespace ReFix::Photon::Transport
