#include "pun_adapter.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::PUN {

    // Standard PUN RPC Event Code is 200
    constexpr uint8_t PUN_RPC_EVENT_CODE = 200;
    // Standard PUN Instantiation Event Code is 202
    constexpr uint8_t PUN_INSTANTIATE_EVENT_CODE = 202;
    // Standard PUN Serialization Event Code is 206
    constexpr uint8_t PUN_SYNC_EVENT_CODE = 206;

    PUNAdapter::PUNAdapter(std::shared_ptr<Realtime::RealtimeClient> client)
        : m_client(client) {
        if (m_client) {
            m_client->OnEventReceived([this](uint8_t code, int32_t sender, const Protocol::PhotonValue& data) {
                HandlePUNEvent(code, sender, data);
            });
        }
    }

    PUNAdapter::~PUNAdapter() = default;

    int32_t PUNAdapter::AllocateViewId() {
        int32_t actorNr = m_client ? m_client->GetLocalActorNumber() : 1;
        int32_t viewId = (actorNr * 1000) + (m_nextLocalViewId++ % 1000);

        RegisterPhotonView(viewId, "AllocatedView");
        return viewId;
    }

    bool PUNAdapter::RegisterPhotonView(int32_t viewId, const std::string& prefabName) {
        int32_t actorNr = m_client ? m_client->GetLocalActorNumber() : 1;

        PhotonViewInfo info;
        info.viewId = viewId;
        info.ownerActorNr = actorNr;
        info.creatorActorNr = actorNr;
        info.prefabName = prefabName;
        info.isMine = true;

        m_views[viewId] = info;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Realtime, "PUN: Registered PhotonView ID %d (Prefab: '%s', Owner: %d)",
                             viewId, prefabName.c_str(), actorNr);
        return true;
    }

    bool PUNAdapter::TransferOwnership(int32_t viewId, int32_t newOwnerActorNr) {
        auto it = m_views.find(viewId);
        if (it == m_views.end()) return false;

        it->second.ownerActorNr = newOwnerActorNr;
        it->second.isMine = (m_client && m_client->GetLocalActorNumber() == newOwnerActorNr);

        Diagnostics::LogInfo(Diagnostics::LogChannel::Realtime, "PUN: Transferred PhotonView %d ownership -> Actor %d",
                             viewId, newOwnerActorNr);
        return true;
    }

    bool PUNAdapter::RPC(int32_t viewId, const std::string& methodName, const Protocol::PhotonArray& parameters,
                         uint8_t targetGroup) {
        if (!m_client) return false;

        Diagnostics::LogDebug(Diagnostics::LogChannel::Realtime, "PUN: RPC '%s' called on ViewId %d (Params: %zu)",
                              methodName.c_str(), viewId, parameters.size());

        // Package PUN RPC Hashtable / Array payload
        Protocol::PhotonHashtable rpcData;
        rpcData[Protocol::PhotonValue(static_cast<uint8_t>(0))] = Protocol::PhotonValue(viewId);
        rpcData[Protocol::PhotonValue(static_cast<uint8_t>(1))] = Protocol::PhotonValue(methodName);
        rpcData[Protocol::PhotonValue(static_cast<uint8_t>(2))] = Protocol::PhotonValue(parameters);

        return m_client->SendEvent(PUN_RPC_EVENT_CODE, Protocol::PhotonValue(rpcData), targetGroup);
    }

    bool PUNAdapter::SendSynchronizationData(int32_t viewId, const std::vector<uint8_t>& streamData) {
        if (!m_client) return false;

        Protocol::PhotonHashtable syncData;
        syncData[Protocol::PhotonValue(static_cast<uint8_t>(0))] = Protocol::PhotonValue(viewId);
        syncData[Protocol::PhotonValue(static_cast<uint8_t>(1))] = Protocol::PhotonValue(streamData);

        return m_client->SendEvent(PUN_SYNC_EVENT_CODE, Protocol::PhotonValue(syncData), Protocol::ReceiverGroup::Others);
    }

    void PUNAdapter::HandlePUNEvent(uint8_t code, int32_t sender, const Protocol::PhotonValue& data) {
        if (code == PUN_RPC_EVENT_CODE) {
            auto ht = data.AsHashtable();
            int32_t viewId = ht[Protocol::PhotonValue(static_cast<uint8_t>(0))].AsInt(0);
            std::string methodName = ht[Protocol::PhotonValue(static_cast<uint8_t>(1))].AsString("");
            Protocol::PhotonArray params = ht[Protocol::PhotonValue(static_cast<uint8_t>(2))].AsArray();

            Diagnostics::LogDebug(Diagnostics::LogChannel::Realtime, "PUN: RPC received '%s' for View %d from Actor %d",
                                  methodName.c_str(), viewId, sender);

            if (m_rpcHandler) {
                m_rpcHandler(viewId, methodName, params, sender);
            }
        }
    }

    void PUNAdapter::OnRPCReceived(std::function<void(int32_t, const std::string&, const Protocol::PhotonArray&, int32_t)> handler) {
        m_rpcHandler = handler;
    }

} // namespace ReFix::Photon::PUN
