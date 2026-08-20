#include "peer_connection.h"

namespace ReFix::Photon::Server {

    PeerConnection::PeerConnection(uint16_t peerId, const sockaddr_in& addr)
        : m_peerId(peerId), m_address(addr), m_lastActivity(GetTickCount()) {}

    std::string PeerConnection::GetAddressString() const {
        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &m_address.sin_addr, ipStr, sizeof(ipStr));
        return std::string(ipStr) + ":" + std::to_string(ntohs(m_address.sin_port));
    }

    uint32_t PeerConnection::GetNextOutgoingSequence(uint8_t channelId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return ++m_outgoingSeq[channelId];
    }

    uint32_t PeerConnection::GetLastIncomingSequence(uint8_t channelId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_incomingSeq[channelId];
    }

    void PeerConnection::SetLastIncomingSequence(uint8_t channelId, uint32_t seq) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_incomingSeq[channelId] = seq;
    }

    bool PeerConnection::ProcessFragment(uint8_t channelId, uint32_t seq, uint32_t fragCount, uint32_t fragNum,
                                         uint32_t totalLen, uint32_t fragOffset, const uint8_t* fragData, size_t fragLen,
                                         std::vector<uint8_t>& outAssembledPayload) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& assembly = m_fragmentAssembly[channelId];
        if (assembly.sequenceNumber != seq) {
            assembly.sequenceNumber = seq;
            assembly.fragmentCount = fragCount;
            assembly.totalLength = totalLen;
            assembly.fragments.clear();
        }

        assembly.fragments[fragNum] = std::vector<uint8_t>(fragData, fragData + fragLen);

        if (assembly.fragments.size() == fragCount) {
            outAssembledPayload.resize(totalLen);
            size_t currentOffset = 0;
            for (uint32_t i = 0; i < fragCount; ++i) {
                const auto& f = assembly.fragments[i];
                if (currentOffset + f.size() <= totalLen) {
                    std::memcpy(outAssembledPayload.data() + currentOffset, f.data(), f.size());
                    currentOffset += f.size();
                }
            }
            assembly.fragments.clear();
            return true;
        }

        return false;
    }

} // namespace ReFix::Photon::Server
