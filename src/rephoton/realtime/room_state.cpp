#include "room_state.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Realtime {

    RoomState::RoomState()
        : m_name("DefaultRoom"), m_maxPlayers(4), m_isOpen(true), m_isVisible(true), m_masterClientId(1), m_nextActorNr(1) {}

    RoomState::RoomState(const std::string& name, const RoomOptions& options)
        : m_name(name),
          m_maxPlayers(options.maxPlayers),
          m_isOpen(options.isOpen),
          m_isVisible(options.isVisible),
          m_emptyRoomTtl(options.emptyRoomTtl),
          m_playerTtl(options.playerTtl),
          m_masterClientId(1),
          m_nextActorNr(1),
          m_customProperties(options.customRoomProperties) {}

    int32_t RoomState::AllocateActorNumber() {
        return m_nextActorNr++;
    }

    int32_t RoomState::AddActor(const std::string& userId, const std::string& nickname) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_actors.size() >= m_maxPlayers || !m_isOpen) {
            Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Cannot add actor: Room '%s' is full or closed", m_name.c_str());
            return 0;
        }

        int32_t actorNr = AllocateActorNumber();
        ActorState state;
        state.actorNr = actorNr;
        state.userId = userId;
        state.nickname = nickname.empty() ? ("Player " + std::to_string(actorNr)) : nickname;
        state.isMasterClient = (m_actors.empty());
        state.isInactive = false;

        m_actors[actorNr] = state;

        if (state.isMasterClient) {
            m_masterClientId = actorNr;
        }

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Actor %d ('%s') entered room '%s' (Total actors: %zu, Master: %d)",
                             actorNr, state.nickname.c_str(), m_name.c_str(), m_actors.size(), m_masterClientId);

        return actorNr;
    }

    bool RoomState::RemoveActor(int32_t actorNr) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_actors.find(actorNr);
        if (it == m_actors.end()) return false;

        bool wasMaster = it->second.isMasterClient;
        m_actors.erase(it);

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Actor %d left room '%s' (Remaining actors: %zu)",
                             actorNr, m_name.c_str(), m_actors.size());

        if (wasMaster && !m_actors.empty()) {
            ElectMasterClient();
        }

        return true;
    }

    void RoomState::ElectMasterClient() {
        // Master Client is the lowest active actor number
        if (m_actors.empty()) return;

        auto firstIt = m_actors.begin();
        firstIt->second.isMasterClient = true;
        m_masterClientId = firstIt->first;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Room '%s': New Master Client elected -> Actor %d",
                             m_name.c_str(), m_masterClientId);
    }

    bool RoomState::HasActor(int32_t actorNr) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_actors.find(actorNr) != m_actors.end();
    }

    ActorState RoomState::GetActor(int32_t actorNr) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_actors.find(actorNr);
        if (it != m_actors.end()) return it->second;
        return ActorState();
    }

    std::vector<ActorState> RoomState::GetAllActors() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ActorState> res;
        for (const auto& [k, v] : m_actors) {
            res.push_back(v);
        }
        return res;
    }

    std::vector<int32_t> RoomState::GetActorNumbers() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<int32_t> res;
        for (const auto& [k, v] : m_actors) {
            res.push_back(k);
        }
        return res;
    }

    size_t RoomState::GetActorCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_actors.size();
    }

    bool RoomState::UpdateCustomProperties(const Protocol::PhotonHashtable& props) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [k, v] : props) {
            m_customProperties[k] = v;
        }
        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Room '%s': Updated %zu custom properties",
                             m_name.c_str(), props.size());
        return true;
    }

    Protocol::PhotonHashtable RoomState::GetCustomProperties() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_customProperties;
    }

    bool RoomState::UpdateActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& props) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_actors.find(actorNr);
        if (it == m_actors.end()) return false;

        for (const auto& [k, v] : props) {
            it->second.customProperties[k] = v;
        }
        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Room '%s': Updated %zu properties for Actor %d",
                             m_name.c_str(), props.size(), actorNr);
        return true;
    }

    Protocol::PhotonHashtable RoomState::GetActorProperties(int32_t actorNr) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_actors.find(actorNr);
        if (it != m_actors.end()) return it->second.customProperties;
        return {};
    }

    Protocol::PhotonHashtable RoomState::GetLobbyProperties() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        Protocol::PhotonHashtable props = m_customProperties;
        props[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::MaxPlayers))] = Protocol::PhotonValue(static_cast<uint8_t>(m_maxPlayers));
        props[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::IsOpen))] = Protocol::PhotonValue(m_isOpen);
        props[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::IsVisible))] = Protocol::PhotonValue(m_isVisible);
        props[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::PlayerCount))] = Protocol::PhotonValue(static_cast<uint8_t>(m_actors.size()));
        props[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::MasterClientId))] = Protocol::PhotonValue(static_cast<int32_t>(m_masterClientId));
        return props;
    }

    RoomStateInfo RoomState::GetInfo(int32_t localActorNr) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        RoomStateInfo info;
        info.name = m_name;
        info.maxPlayers = m_maxPlayers;
        info.isOpen = m_isOpen;
        info.isVisible = m_isVisible;
        info.masterClientId = m_masterClientId;
        info.localActorNumber = localActorNr;
        info.actors = m_actors;
        info.customProperties = m_customProperties;
        return info;
    }

} // namespace ReFix::Photon::Realtime
