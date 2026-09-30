// ReFix EOS v3 - EOS_P2P_*.
//
// The game's own netcode runs over this interface, so it has to behave like the
// real thing in the details that matter:
//
//   * packets are delivered in order per (peer, socket, channel) and are read
//     one at a time through GetNextReceivedPacketSize / ReceivePacket;
//   * an unsolicited first packet from a peer raises a connection request that
//     the title must accept before traffic flows, which is what lets a lobby
//     host reject strangers;
//   * a peer is addressed by its ProductUserId - the transport looks up where
//     that id actually lives, so the game never sees an IP address.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../core/eos_online.h"
#include "../net/lobby_directory.h"
#include "../eos_module.h"
#include <deque>

using namespace refix;

namespace {

struct IncomingPacket {
    std::string          FromPuid;
    std::string          SocketName;
    uint8_t              Channel = 0;
    std::vector<uint8_t> Data;
};

struct P2PState {
    std::mutex                 Mutex;
    std::deque<IncomingPacket> Queue;
    // (peer, socket) pairs the title has accepted, plus those we have already
    // raised a request for so a burst of packets does not raise it repeatedly.
    std::vector<std::pair<std::string, std::string>> Accepted;
    std::vector<std::pair<std::string, std::string>> Requested;
    uint64_t MaxQueueBytes = 64 * 1024 * 1024;
    uint64_t QueuedBytes = 0;
};

P2PState& State() { static P2PState s; return s; }

bool Contains(const std::vector<std::pair<std::string, std::string>>& v,
              const std::string& a, const std::string& b) {
    for (const auto& e : v) if (e.first == a && e.second == b) return true;
    return false;
}

std::string PuidString(EOS_ProductUserId id) {
    const char* s = IdRegistry::ToString(id);
    return s ? std::string(s) : std::string();
}

void RaiseConnectionRequest(const std::string& fromPuid, const std::string& socketName) {
    EOS_ProductUserId local  = Identity::Get().LocalPuid();
    EOS_ProductUserId remote = IdRegistry::Get().Puid(fromPuid);
    Dispatcher::Get().Broadcast<EOS_P2P_OnIncomingConnectionRequestInfo>(
        NotifyKind::P2PIncomingConnectionRequest,
        [local, remote, socketName](void* clientData) {
            EOS_P2P_OnIncomingConnectionRequestInfo info{};
            info.ClientData   = clientData;
            info.LocalUserId  = local;
            info.RemoteUserId = remote;
            static thread_local EOS_P2P_SocketId socket{};
            std::memset(&socket, 0, sizeof(socket));
            socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
            std::strncpy(socket.SocketName, socketName.c_str(), sizeof(socket.SocketName) - 1);
            info.SocketId = &socket;
            return info;
        });
}

void RaiseConnectionEstablished(const std::string& peerPuid, const std::string& socketName) {
    EOS_ProductUserId local  = Identity::Get().LocalPuid();
    EOS_ProductUserId remote = IdRegistry::Get().Puid(peerPuid);
    Dispatcher::Get().Broadcast<EOS_P2P_OnPeerConnectionEstablishedInfo>(
        NotifyKind::P2PConnectionEstablished,
        [local, remote, socketName](void* clientData) {
            EOS_P2P_OnPeerConnectionEstablishedInfo info{};
            info.ClientData        = clientData;
            info.LocalUserId       = local;
            info.RemoteUserId      = remote;
            info.ConnectionType    = EOS_EConnectionEstablishedType::EOS_CET_NewConnection;
            info.NetworkType       = EOS_ENetworkConnectionType::EOS_NCT_DirectConnection;
            static thread_local EOS_P2P_SocketId socket{};
            std::memset(&socket, 0, sizeof(socket));
            socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
            std::strncpy(socket.SocketName, socketName.c_str(), sizeof(socket.SocketName) - 1);
            info.SocketId = &socket;
            return info;
        });
}

// Installed once, on first P2P use: hands packets from the transport into the
// receive queue and raises the connection request for unknown peers.
void InstallPacketHook() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    LobbyDirectory::Get().SetP2PHandler(
        [](const std::string& fromPuid, const std::string& socketName, uint8_t channel,
           const uint8_t* data, size_t len) {
            if (fromPuid.empty()) return;             // unknown sender, drop
            auto& st = State();
            bool needRequest = false;
            {
                std::lock_guard<std::mutex> lock(st.Mutex);
                if (st.QueuedBytes + len > st.MaxQueueBytes) return;
                if (!Contains(st.Accepted, fromPuid, socketName)) {
                    if (!Contains(st.Requested, fromPuid, socketName)) {
                        st.Requested.emplace_back(fromPuid, socketName);
                        needRequest = true;
                    }
                }
                IncomingPacket p;
                p.FromPuid   = fromPuid;
                p.SocketName = socketName;
                p.Channel    = channel;
                p.Data.assign(data, data + len);
                st.QueuedBytes += len;
                st.Queue.push_back(std::move(p));
            }
            if (needRequest) {
                std::string puid = fromPuid, sock = socketName;
                Dispatcher::Get().Post([puid, sock]() { RaiseConnectionRequest(puid, sock); });
            }
        });
}

std::string SocketNameOf(const EOS_P2P_SocketId* id) {
    return (id && id->SocketName[0]) ? std::string(id->SocketName) : std::string();
}

} // namespace

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_SendPacket(EOS_HP2P Handle, const EOS_P2P_SendPacketOptions* Options) {
    if (!Options || !Options->Data) return ER::EOS_InvalidParameters;
    if (!EnsureOnline()) return ER::EOS_NoConnection;
    InstallPacketHook();

    std::string to = PuidString(Options->RemoteUserId);
    if (to.empty()) return ER::EOS_InvalidUser;

    std::string socketName = SocketNameOf(Options->SocketId);
    if (!LobbyDirectory::Get().SendP2P(to, socketName, (uint8_t)Options->Channel,
                                       Options->Data, Options->DataLengthBytes)) {
        return ER::EOS_NoConnection;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_GetNextReceivedPacketSize(EOS_HP2P Handle, const EOS_P2P_GetNextReceivedPacketSizeOptions* Options, uint32_t* OutPacketSizeBytes) {
    if (!OutPacketSizeBytes) return ER::EOS_InvalidParameters;
    InstallPacketHook();
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    for (const auto& p : st.Queue) {
        if (Options && Options->RequestedChannel && *Options->RequestedChannel != p.Channel) continue;
        *OutPacketSizeBytes = (uint32_t)p.Data.size();
        return ER::EOS_Success;
    }
    return ER::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_ReceivePacket(EOS_HP2P Handle, const EOS_P2P_ReceivePacketOptions* Options, EOS_ProductUserId* OutPeerId, EOS_P2P_SocketId* OutSocketId, uint8_t* OutChannel, void* OutData, uint32_t* OutBytesWritten) {
    if (!Options || !OutData || !OutBytesWritten) return ER::EOS_InvalidParameters;
    InstallPacketHook();
    auto& st = State();

    IncomingPacket packet;
    {
        std::lock_guard<std::mutex> lock(st.Mutex);
        bool found = false;
        for (auto it = st.Queue.begin(); it != st.Queue.end(); ++it) {
            if (Options->RequestedChannel && *Options->RequestedChannel != it->Channel) continue;
            if (it->Data.size() > Options->MaxDataSizeBytes) return ER::EOS_InvalidParameters;
            packet = std::move(*it);
            st.QueuedBytes -= packet.Data.size();
            st.Queue.erase(it);
            found = true;
            break;
        }
        if (!found) return ER::EOS_NotFound;
    }

    if (OutPeerId)  *OutPeerId  = IdRegistry::Get().Puid(packet.FromPuid);
    if (OutChannel) *OutChannel = packet.Channel;
    if (OutSocketId) {
        std::memset(OutSocketId, 0, sizeof(*OutSocketId));
        OutSocketId->ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        std::strncpy(OutSocketId->SocketName, packet.SocketName.c_str(), sizeof(OutSocketId->SocketName) - 1);
    }
    std::memcpy(OutData, packet.Data.data(), packet.Data.size());
    *OutBytesWritten = (uint32_t)packet.Data.size();
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_AcceptConnection(EOS_HP2P Handle, const EOS_P2P_AcceptConnectionOptions* Options) {
    if (!Options) return ER::EOS_InvalidParameters;
    std::string peer = PuidString(Options->RemoteUserId);
    std::string sock = SocketNameOf(Options->SocketId);
    if (peer.empty()) return ER::EOS_InvalidUser;
    {
        auto& st = State();
        std::lock_guard<std::mutex> lock(st.Mutex);
        if (!Contains(st.Accepted, peer, sock)) st.Accepted.emplace_back(peer, sock);
    }
    RFLOG(P2P, "accepted connection from %s on socket '%s'", peer.c_str(), sock.c_str());
    Dispatcher::Get().Post([peer, sock]() { RaiseConnectionEstablished(peer, sock); });
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_CloseConnection(EOS_HP2P Handle, const EOS_P2P_CloseConnectionOptions* Options) {
    if (!Options) return ER::EOS_InvalidParameters;
    std::string peer = PuidString(Options->RemoteUserId);
    std::string sock = SocketNameOf(Options->SocketId);
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    for (size_t i = 0; i < st.Accepted.size(); ) {
        if (st.Accepted[i].first == peer && (sock.empty() || st.Accepted[i].second == sock))
            st.Accepted.erase(st.Accepted.begin() + i);
        else ++i;
    }
    for (size_t i = 0; i < st.Requested.size(); ) {
        if (st.Requested[i].first == peer && (sock.empty() || st.Requested[i].second == sock))
            st.Requested.erase(st.Requested.begin() + i);
        else ++i;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_CloseConnections(EOS_HP2P Handle, const EOS_P2P_CloseConnectionsOptions* Options) {
    std::string sock = Options ? SocketNameOf(Options->SocketId) : std::string();
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    for (size_t i = 0; i < st.Accepted.size(); ) {
        if (sock.empty() || st.Accepted[i].second == sock) st.Accepted.erase(st.Accepted.begin() + i);
        else ++i;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_ClearPacketQueue(EOS_HP2P Handle, const EOS_P2P_ClearPacketQueueOptions* Options) {
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    st.Queue.clear();
    st.QueuedBytes = 0;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_GetPacketQueueInfo(EOS_HP2P Handle, const EOS_P2P_GetPacketQueueInfoOptions* Options, EOS_P2P_PacketQueueInfo* OutPacketQueueInfo) {
    if (!OutPacketQueueInfo) return ER::EOS_InvalidParameters;
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    std::memset(OutPacketQueueInfo, 0, sizeof(*OutPacketQueueInfo));
    OutPacketQueueInfo->IncomingPacketQueueMaxSizeBytes     = st.MaxQueueBytes;
    OutPacketQueueInfo->IncomingPacketQueueCurrentSizeBytes = st.QueuedBytes;
    OutPacketQueueInfo->IncomingPacketQueueCurrentPacketCount = (uint64_t)st.Queue.size();
    OutPacketQueueInfo->OutgoingPacketQueueMaxSizeBytes     = st.MaxQueueBytes;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_SetPacketQueueSize(EOS_HP2P Handle, const EOS_P2P_SetPacketQueueSizeOptions* Options) {
    if (!Options) return ER::EOS_InvalidParameters;
    auto& st = State();
    std::lock_guard<std::mutex> lock(st.Mutex);
    st.MaxQueueBytes = Options->IncomingPacketQueueMaxSizeBytes;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_P2P_GetNATType(EOS_HP2P Handle, const EOS_P2P_GetNATTypeOptions* Options, EOS_ENATType* OutNATType) {
    // Traffic is direct on the local network, which is what "open" describes.
    if (OutNATType) *OutNATType = EOS_ENATType::EOS_NAT_Open;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_P2P_QueryNATType(EOS_HP2P Handle, const EOS_P2P_QueryNATTypeOptions* Options, void* ClientData, const EOS_P2P_OnQueryNATTypeCompleteCallback CompletionDelegate) {
    if (!CompletionDelegate) return;
    Dispatcher::Get().Post([CompletionDelegate, ClientData]() {
        EOS_P2P_OnQueryNATTypeCompleteInfo info{};
        info.ResultCode = ER::EOS_Success;
        info.ClientData = ClientData;
        info.NATType    = EOS_ENATType::EOS_NAT_Open;
        CompletionDelegate(&info);
    });
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_P2P_AddNotifyPeerConnectionRequest(EOS_HP2P Handle, const EOS_P2P_AddNotifyPeerConnectionRequestOptions* Options, void* ClientData, const EOS_P2P_OnIncomingConnectionRequestCallback ConnectionRequestHandler) {
    EnsureOnline();
    InstallPacketHook();
    return Dispatcher::Get().Register(NotifyKind::P2PIncomingConnectionRequest, (void*)ConnectionRequestHandler, ClientData);
}

EOS_DECLARE_FUNC(void) EOS_P2P_RemoveNotifyPeerConnectionRequest(EOS_HP2P Handle, EOS_NotificationId NotificationId) {
    Dispatcher::Get().Unregister(NotificationId);
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_P2P_AddNotifyPeerConnectionEstablished(EOS_HP2P Handle, const EOS_P2P_AddNotifyPeerConnectionEstablishedOptions* Options, void* ClientData, const EOS_P2P_OnPeerConnectionEstablishedCallback ConnectionEstablishedHandler) {
    return Dispatcher::Get().Register(NotifyKind::P2PConnectionEstablished, (void*)ConnectionEstablishedHandler, ClientData);
}

EOS_DECLARE_FUNC(void) EOS_P2P_RemoveNotifyPeerConnectionEstablished(EOS_HP2P Handle, EOS_NotificationId NotificationId) {
    Dispatcher::Get().Unregister(NotificationId);
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_P2P_AddNotifyPeerConnectionClosed(EOS_HP2P Handle, const EOS_P2P_AddNotifyPeerConnectionClosedOptions* Options, void* ClientData, const EOS_P2P_OnRemoteConnectionClosedCallback ConnectionClosedHandler) {
    return Dispatcher::Get().Register(NotifyKind::P2PConnectionClosed, (void*)ConnectionClosedHandler, ClientData);
}

EOS_DECLARE_FUNC(void) EOS_P2P_RemoveNotifyPeerConnectionClosed(EOS_HP2P Handle, EOS_NotificationId NotificationId) {
    Dispatcher::Get().Unregister(NotificationId);
}

namespace refix {

void RegisterP2PApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_P2P_SendPacket);
    REFIX_BIND(reg, EOS_P2P_GetNextReceivedPacketSize);
    REFIX_BIND(reg, EOS_P2P_ReceivePacket);
    REFIX_BIND(reg, EOS_P2P_AcceptConnection);
    REFIX_BIND(reg, EOS_P2P_CloseConnection);
    REFIX_BIND(reg, EOS_P2P_CloseConnections);
    REFIX_BIND(reg, EOS_P2P_ClearPacketQueue);
    REFIX_BIND(reg, EOS_P2P_GetPacketQueueInfo);
    REFIX_BIND(reg, EOS_P2P_SetPacketQueueSize);
    REFIX_BIND(reg, EOS_P2P_GetNATType);
    REFIX_BIND(reg, EOS_P2P_QueryNATType);
    REFIX_BIND(reg, EOS_P2P_AddNotifyPeerConnectionRequest);
    REFIX_BIND(reg, EOS_P2P_RemoveNotifyPeerConnectionRequest);
    REFIX_BIND(reg, EOS_P2P_AddNotifyPeerConnectionEstablished);
    REFIX_BIND(reg, EOS_P2P_RemoveNotifyPeerConnectionEstablished);
    REFIX_BIND(reg, EOS_P2P_AddNotifyPeerConnectionClosed);
    REFIX_BIND(reg, EOS_P2P_RemoveNotifyPeerConnectionClosed);
}

} // namespace refix
