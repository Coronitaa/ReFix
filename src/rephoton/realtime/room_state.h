#pragma once

#include "../core/photon_interfaces.h"
#include <string>
#include <map>
#include <mutex>
#include <vector>

namespace ReFix::Photon::Realtime {

    class RoomState {
    public:
        RoomState();
        explicit RoomState(const std::string& name, const RoomOptions& options);

        std::string GetName() const { return m_name; }
        uint8_t GetMaxPlayers() const { return m_maxPlayers; }
        bool IsOpen() const { return m_isOpen; }
        bool IsVisible() const { return m_isVisible; }
        int32_t GetMasterClientId() const { return m_masterClientId; }

        void SetOpen(bool open) { m_isOpen = open; }
        void SetVisible(bool visible) { m_isVisible = visible; }

        int32_t AddActor(const std::string& userId, const std::string& nickname = "");
        bool RemoveActor(int32_t actorNr);

        bool HasActor(int32_t actorNr) const;
        ActorState GetActor(int32_t actorNr) const;
        std::vector<ActorState> GetAllActors() const;
        std::vector<int32_t> GetActorNumbers() const;
        size_t GetActorCount() const;

        bool UpdateCustomProperties(const Protocol::PhotonHashtable& props);
        Protocol::PhotonHashtable GetCustomProperties() const;

        bool UpdateActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& props);
        Protocol::PhotonHashtable GetActorProperties(int32_t actorNr) const;

        RoomStateInfo GetInfo(int32_t localActorNr = 0) const;

    private:
        int32_t AllocateActorNumber();
        void ElectMasterClient();

        std::string m_name;
        uint8_t m_maxPlayers = 4;
        bool m_isOpen = true;
        bool m_isVisible = true;
        uint32_t m_emptyRoomTtl = 0;
        uint32_t m_playerTtl = 0;
        int32_t m_masterClientId = 1;
        int32_t m_nextActorNr = 1;

        std::map<int32_t, ActorState> m_actors;
        Protocol::PhotonHashtable m_customProperties;
        mutable std::mutex m_mutex;
    };

} // namespace ReFix::Photon::Realtime
