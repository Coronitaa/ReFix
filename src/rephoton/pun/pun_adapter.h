#pragma once

#include "../realtime/realtime_client.h"
#include <map>
#include <vector>
#include <string>

namespace ReFix::Photon::PUN {

    struct PhotonViewInfo {
        int32_t viewId = 0;
        int32_t ownerActorNr = 0;
        int32_t creatorActorNr = 0;
        std::string prefabName;
        bool isMine = false;
    };

    class PUNAdapter {
    public:
        explicit PUNAdapter(std::shared_ptr<Realtime::RealtimeClient> client);
        ~PUNAdapter();

        int32_t AllocateViewId();
        bool RegisterPhotonView(int32_t viewId, const std::string& prefabName = "");
        bool TransferOwnership(int32_t viewId, int32_t newOwnerActorNr);

        // RPC Dispatch
        bool RPC(int32_t viewId, const std::string& methodName, const Protocol::PhotonArray& parameters,
                 uint8_t targetGroup = Protocol::ReceiverGroup::Others);

        // Synchronize Object Transform / State
        bool SendSynchronizationData(int32_t viewId, const std::vector<uint8_t>& streamData);

        void OnRPCReceived(std::function<void(int32_t viewId, const std::string& methodName, const Protocol::PhotonArray& params, int32_t sender)> handler);

        std::shared_ptr<Realtime::RealtimeClient> GetClient() const { return m_client; }

    private:
        void HandlePUNEvent(uint8_t code, int32_t sender, const Protocol::PhotonValue& data);

        std::shared_ptr<Realtime::RealtimeClient> m_client;
        std::map<int32_t, PhotonViewInfo> m_views;
        int32_t m_nextLocalViewId = 1000;

        std::function<void(int32_t, const std::string&, const Protocol::PhotonArray&, int32_t)> m_rpcHandler;
    };

} // namespace ReFix::Photon::PUN
