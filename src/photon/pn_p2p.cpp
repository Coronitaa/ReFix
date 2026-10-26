#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "pn_p2p.h"
#include "pn_dns.h"
#include "pn_enet.h"
#include "pn_server.h"
#include "../refix_steam_flat.h"
#include "../steam_p2p_hook.h"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPhotonP2P {

namespace {

// Photon Cloud UDP ports per role (default and "alternative" sets of PhotonPortDefinition).
const uint16_t kNameServerPorts[] = { 5058, 27000 };
const uint16_t kMasterPorts[] = { 5055, 27001 };
const uint16_t kGamePorts[] = { 5056, 27002 };
constexpr const char* kDefaultHostSuffixes = "photonengine.io;photonengine.com;exitgames.com";
constexpr const char* kPresenceKey = "refix_photon_room";
constexpr const char* kP2PBackend = "ReFixCloud";     // canonical ecosystem token: local Photon emulator over Steam P2P
constexpr const char* kP2PBackendLegacy = "ReFixP2P"; // back-compat alias for configs written before the rename
constexpr uint8_t kToHost = 1, kToClient = 2;  // Steam envelope: [kind][clientPort:2][Photon datagram]
constexpr uint64_t kSteamIdMin = 76561197960265728ull, kSteamIdMax = 76561202255233023ull;
constexpr uint64_t kRemoteKeyFlag = 1ull << 63;
constexpr int kMaxDatagram = 65536;
constexpr int kSelectTimeoutUs = 1000;
constexpr int kSteamBatch = 64;
constexpr uint8_t kForwarderNet0 = 127, kForwarderNet1 = 80;
constexpr int kRegionPingSize = 13;
constexpr uint8_t kRegionPingByte = 0x7D;

struct Listener {
    SOCKET socket;
    PnServer::Role role;
    uint16_t port;
};

struct Connection {
    std::unique_ptr<PnEnetConnection> enet;
};

// One loopback address per remote host; local Photon clients talk to it as if it were that host's GameServer.
// It listens on every GameServer port: Photon clients may swap the port the Master returned for their
// alternative one (5056 <-> 27002, e.g. R.E.P.O.), and nothing would answer on the other port.
struct Forwarder {
    std::vector<SOCKET> sockets;               // one per kGamePorts entry
    std::map<uint16_t, SOCKET> clientSocket;   // local client port -> socket it talks to
    std::string address; // "127.80.x.y:port" handed to the client
};

std::atomic<bool> g_started{ false };
std::atomic<bool> g_photonActive{ false };  // true once the game actually uses Photon (hit our
                                            // loopback listeners or hosts a room); gates all background
                                            // Steam calls so non-Photon games never touch Steamworks here.
int g_channel = 9;
uint64_t g_self = 0;
std::vector<Listener> g_listeners;
std::map<uint64_t, Connection> g_connections;   // peer key -> transport
std::map<uint64_t, Forwarder> g_forwarders;     // host SteamID -> forwarder
std::unique_ptr<PnServer::Emulator> g_emulator;
std::string g_masterAddress, g_gameAddress;
uint16_t g_gamePort = 0;

std::string IniPath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
}

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    ReFixLog("%s", buf);
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    for (std::string item; std::getline(ss, item, sep);) if (!item.empty()) out.push_back(item);
    return out;
}

sockaddr_in Loopback(uint32_t ipHostOrder, uint16_t port) {
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ipHostOrder);
    a.sin_port = htons(port);
    return a;
}

SOCKET OpenUdp(uint32_t ipHostOrder, uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;
    SteamP2PHook::ExcludeSocket(s); // the legacy UDP-over-SDR layer must not reclassify emulator sockets
    sockaddr_in a = Loopback(ipHostOrder, port);
    u_long nonBlocking = 1;
    BOOL noConnReset = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, _WSAIOW(IOC_VENDOR, 12) /* SIO_UDP_CONNRESET */, &noConnReset, sizeof(noConnReset), nullptr, 0, &bytes, nullptr, nullptr);
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0 || ioctlsocket(s, FIONBIO, &nonBlocking) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

std::string AddressString(uint32_t ipHostOrder, uint16_t port) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", ipHostOrder >> 24, (ipHostOrder >> 16) & 0xFF, (ipHostOrder >> 8) & 0xFF,
             ipHostOrder & 0xFF, port);
    return buf;
}

// --- Room directory ----------------------------------------------------------------------------------------

uint64_t SteamIdPrefix(const std::string& name) {
    size_t digits = 0;
    while (digits < name.size() && isdigit((unsigned char)name[digits])) digits++;
    if (digits < 17 || digits > 20) return 0;
    uint64_t id = strtoull(name.substr(0, digits).c_str(), nullptr, 10);
    if (id >= kSteamIdMin && id <= kSteamIdMax) return id;  // "<hostSteamId>-..." names
    return ReFixSteamFlat::LobbyOwner(id);                    // lobby id as room name, also "<lobbyId>_voice_"
}

uint64_t ResolveHost(const std::string& name) {
    if (uint64_t id = SteamIdPrefix(name)) {
        Log("[Photon] Room '%s' hosted by %llu (name/lobby)", name.c_str(), (unsigned long long)id);
        return id;
    }
    for (const auto& entry : ReFixSteamFlat::FriendsWithRichPresence(kPresenceKey)) {
        if (entry.value == name) return entry.steamID;
    }
    return 0;
}

Forwarder* ForwarderFor(uint64_t host) {
    auto it = g_forwarders.find(host);
    if (it != g_forwarders.end()) return &it->second;
    uint32_t index = (uint32_t)g_forwarders.size() + 1;
    uint32_t ip = (uint32_t)kForwarderNet0 << 24 | (uint32_t)kForwarderNet1 << 16 | (index & 0xFFFF);
    Forwarder f;
    for (uint16_t port : kGamePorts) {
        SOCKET s = OpenUdp(ip, port);
        if (s == INVALID_SOCKET) {
            Log("[Photon] Could not open forwarder %s for host %llu (%d)", AddressString(ip, port).c_str(),
                (unsigned long long)host, WSAGetLastError());
            continue;
        }
        f.sockets.push_back(s);
        if (f.address.empty() || port == g_gamePort) f.address = AddressString(ip, port);
    }
    if (f.sockets.empty()) return nullptr;
    Log("[Photon] Host %llu reachable at %s via Steam P2P", (unsigned long long)host, f.address.c_str());
    return &g_forwarders.emplace(host, f).first->second;
}

std::string GameAddress(const std::string& name) {
    uint64_t host = ResolveHost(name);
    if (!host) return "";
    if (host == g_self) return g_gameAddress;
    Forwarder* f = ForwarderFor(host);
    return f ? f->address : "";
}

std::string NewRoomName() {
    static std::mt19937 rng{ std::random_device{}() };
    char buf[64];
    snprintf(buf, sizeof(buf), "%llu-%08x", (unsigned long long)g_self, (unsigned)rng());
    return buf;
}

std::vector<std::string> JoinableRooms() {
    std::vector<std::string> rooms;
    for (const auto& entry : ReFixSteamFlat::FriendsWithRichPresence(kPresenceKey)) rooms.push_back(entry.value);
    return rooms;
}

void RoomHosted(const std::string& name, bool open) {
    if (open) g_photonActive.store(true, std::memory_order_relaxed);
    ReFixSteamFlat::SetRichPresence(kPresenceKey, open ? name : std::string());
}

// --- Connections -------------------------------------------------------------------------------------------

uint64_t LocalKey(size_t listenerIndex, const sockaddr_in& from) {
    return (uint64_t)listenerIndex << 48 | (uint64_t)ntohl(from.sin_addr.s_addr) << 16 | ntohs(from.sin_port);
}

uint64_t RemoteKey(uint64_t steamId, uint16_t clientPort) {
    return kRemoteKeyFlag | (steamId & 0xFFFFFFFFull) << 16 | clientPort;
}

Connection& Connect(uint64_t key, PnServer::Role role, const std::string& userId, PnEnetConnection::SendFn send) {
    auto it = g_connections.find(key);
    if (it != g_connections.end()) return it->second;
    Connection& c = g_connections[key];
    c.enet = std::make_unique<PnEnetConnection>(
        (int16_t)(g_connections.size() & 0x7FFF), std::move(send),
        [key](const std::string& m, bool reliable, uint8_t channel) { g_emulator->OnMessage(key, m, reliable, channel); });
    PnEnetConnection* enet = c.enet.get();
    g_emulator->OnConnect(key, role, userId,
                          [enet](const std::string& m, bool reliable, uint8_t channel) { enet->SendPayload(m, reliable, channel); });
    return c;
}

// Tunnel diagnostics: per-peer message and byte counts, logged by the pump while they change.
struct TunnelStats {
    uint32_t sent = 0, sentBytes = 0, sentMax = 0, received = 0, receivedBytes = 0, receivedMax = 0;
    uint32_t loggedSent = 0, loggedReceived = 0;
};
std::map<uint64_t, TunnelStats> g_tunnelStats;

void CountTunnel(uint32_t& messages, uint32_t& bytes, uint32_t& largest, size_t size) {
    messages++;
    bytes += (uint32_t)size;
    if (size > largest) largest = (uint32_t)size;
}

void LogTunnelStats(DWORD now) {
    constexpr DWORD kIntervalMs = 5000;
    static DWORD s_last = 0;
    if (now - s_last < kIntervalMs) return;
    s_last = now;
    for (auto& [peer, st] : g_tunnelStats) {
        if (st.sent == st.loggedSent && st.received == st.loggedReceived) continue;
        st.loggedSent = st.sent;
        st.loggedReceived = st.received;
        Log("[Photon] Steam tunnel %llu: sent %u (%u bytes, largest %u), received %u (%u bytes, largest %u)",
            (unsigned long long)peer, st.sent, st.sentBytes, st.sentMax, st.received, st.receivedBytes, st.receivedMax);
    }
}

void SendSteam(uint64_t steamId, uint8_t kind, uint16_t clientPort, const char* data, size_t size) {
    std::string envelope{ (char)kind, (char)(clientPort >> 8), (char)clientPort };
    envelope.append(data, size);
    bool sent = ReFixSteamFlat::SendMessage(steamId, envelope.data(), (uint32_t)envelope.size(), g_channel);
    TunnelStats& stats = g_tunnelStats[steamId];
    if (sent) CountTunnel(stats.sent, stats.sentBytes, stats.sentMax, size);
    // Tunnel diagnostics: the first packet to each peer and every failed send (rate-limited per peer).
    static std::map<uint64_t, uint32_t> s_sent, s_failed;
    uint32_t& count = sent ? s_sent[steamId] : s_failed[steamId];
    if (count++ == 0 || (!sent && count % 100 == 0))
        Log("[Photon] Steam P2P %s to %llu (%s, %zu bytes, %u so far)", sent ? "first send" : "SEND FAILED",
            (unsigned long long)steamId, kind == kToHost ? "to host" : "to client", size, count);
}

// Region ping (Photon.Realtime.PhotonPing): 12 x 0x7D + ping id, answered by echoing it.
bool IsRegionPing(const char* data, int size) {
    if (size != kRegionPingSize) return false;
    for (int i = 0; i < kRegionPingSize - 1; i++) if ((uint8_t)data[i] != kRegionPingByte) return false;
    return true;
}

void OnListenerDatagram(size_t index, const sockaddr_in& from, const char* data, int size) {
    const Listener& l = g_listeners[index];
    g_photonActive.store(true, std::memory_order_relaxed); // the game is speaking Photon to us
    SOCKET s = l.socket;
    if (IsRegionPing(data, size)) {
        sendto(s, data, size, 0, (const sockaddr*)&from, sizeof(from));
        return;
    }
    Connection& c = Connect(LocalKey(index, from), l.role, std::to_string(g_self), [s, from](const std::string& d) {
        sendto(s, d.data(), (int)d.size(), 0, (const sockaddr*)&from, sizeof(from));
    });
    c.enet->OnDatagram((const uint8_t*)data, (size_t)size, GetTickCount());
}

void OnSteamMessage(uint64_t from, const uint8_t* data, uint32_t size) {
    if (size < 4) return;
    static std::set<uint64_t> s_seen;
    if (s_seen.insert(from).second)
        Log("[Photon] Steam P2P first message from %llu (%s)", (unsigned long long)from, data[0] == kToHost ? "to host" : "to client");
    TunnelStats& stats = g_tunnelStats[from];
    CountTunnel(stats.received, stats.receivedBytes, stats.receivedMax, size - 3);
    uint16_t port = (uint16_t)(data[1] << 8 | data[2]);
    if (data[0] == kToHost) {
        Connection& c = Connect(RemoteKey(from, port), PnServer::Role::Game, std::to_string(from), [from, port](const std::string& d) {
            SendSteam(from, kToClient, port, d.data(), d.size());
        });
        c.enet->OnDatagram(data + 3, size - 3, GetTickCount());
    } else if (data[0] == kToClient) {
        auto it = g_forwarders.find(from);
        if (it == g_forwarders.end() || it->second.sockets.empty()) return;
        auto cs = it->second.clientSocket.find(port);
        SOCKET s = cs != it->second.clientSocket.end() ? cs->second : it->second.sockets[0];
        sockaddr_in client = Loopback(INADDR_LOOPBACK, port);
        sendto(s, (const char*)data + 3, (int)size - 3, 0, (const sockaddr*)&client, sizeof(client));
    }
}

void Pump() {
    static char buf[kMaxDatagram];
    fd_set readable;
    FD_ZERO(&readable);
    for (const auto& l : g_listeners) FD_SET(l.socket, &readable);
    for (const auto& [host, f] : g_forwarders)
        for (SOCKET s : f.sockets) FD_SET(s, &readable);
    timeval timeout = { 0, kSelectTimeoutUs };
    if (select(0, &readable, nullptr, nullptr, &timeout) > 0) {
        for (size_t i = 0; i < g_listeners.size(); i++) {
            if (!FD_ISSET(g_listeners[i].socket, &readable)) continue;
            for (;;) {
                sockaddr_in from = {};
                int len = sizeof(from);
                int n = recvfrom(g_listeners[i].socket, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
                if (n <= 0) break;
                OnListenerDatagram(i, from, buf, n);
            }
        }
        for (auto& [host, f] : g_forwarders) {
            for (SOCKET s : f.sockets) {
                if (!FD_ISSET(s, &readable)) continue;
                for (;;) {
                    sockaddr_in from = {};
                    int len = sizeof(from);
                    int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
                    if (n <= 0) break;
                    f.clientSocket[ntohs(from.sin_port)] = s;
                    SendSteam(host, kToHost, ntohs(from.sin_port), buf, (size_t)n);
                }
            }
        }
    }
    if (g_photonActive.load(std::memory_order_relaxed)) { // only poll Steam once Photon is actually in use
        while (ReFixSteamFlat::ReceiveMessages(g_channel, kSteamBatch, OnSteamMessage) == kSteamBatch) {}
    }

    DWORD now = GetTickCount();
    g_emulator->Tick(now);
    LogTunnelStats(now);
    for (auto it = g_connections.begin(); it != g_connections.end();) {
        it->second.enet->Tick(now);
        if (it->second.enet->IsClosed()) {
            g_emulator->OnDisconnect(it->first);
            it = g_connections.erase(it);
        } else {
            ++it;
        }
    }
    // Incoming P2P sessions are accepted reactively: Steam posts SteamNetworkingMessagesSessionRequest_t
    // (callback 1251), handled on the game's own thread in steam_proxy via ReFixSteamFlat::OnSessionRequestCallback.
    // We must NOT enumerate the Steam friends list from this background pump thread: doing so races the game's
    // own Steam calls (e.g. the Steam auth burst during Photon room creation) and crashes inside the Valve DLL.
}

DWORD WINAPI Thread(LPVOID) {
    while (!(g_self = ReFixSteamFlat::LocalSteamID())) Sleep(200); // wait for Steam
    PnServer::Environment env;
    env.localUserId = std::to_string(g_self);
    env.masterAddress = g_masterAddress;
    env.localGameAddress = g_gameAddress;
    env.gameAddress = GameAddress;
    env.newRoomName = NewRoomName;
    env.joinableRooms = JoinableRooms;
    env.roomHosted = RoomHosted;
    env.log = Log;
    g_emulator = std::make_unique<PnServer::Emulator>(env);
    // Start watching friends' room presence now, so the presence cache (refreshed on the game thread) is warm by
    // the time a join needs it; a cold cache made the first join of a code-named room fail ("not found").
    ReFixSteamFlat::FriendsWithRichPresence(kPresenceKey);
    Log("[Photon] P2P emulator ready (self %llu, master %s, game %s, Steam channel %d)", (unsigned long long)g_self,
        g_masterAddress.c_str(), g_gameAddress.c_str(), g_channel);
    for (;;) Pump();
}

void OpenListeners(const uint16_t* ports, size_t count, PnServer::Role role, std::string* firstAddress) {
    for (size_t i = 0; i < count; i++) {
        SOCKET s = OpenUdp(INADDR_LOOPBACK, ports[i]);
        if (s == INVALID_SOCKET) {
            Log("[Photon] Port 127.0.0.1:%u unavailable (%d)", ports[i], WSAGetLastError());
            continue;
        }
        g_listeners.push_back({ s, role, ports[i] });
        if (firstAddress && firstAddress->empty()) *firstAddress = AddressString(INADDR_LOOPBACK, ports[i]);
        if (role == PnServer::Role::Game && !g_gamePort) g_gamePort = ports[i];
    }
}

} // namespace

void Initialize() {
    if (g_started.exchange(true)) return;
    std::string ini = IniPath();
    char backend[64] = { 0 };
    // Off unless ReFix.ini selects it: deploy_helper writes Backend=ReFixCloud only for games where it detected
    // a Photon stack, so other games never get the emulator's DNS hooks and loopback listeners.
    GetPrivateProfileStringA("Photon", "Backend", "OfficialPhoton", backend, sizeof(backend), ini.c_str());
    if (_stricmp(backend, kP2PBackend) != 0 && _stricmp(backend, kP2PBackendLegacy) != 0) {
        Log("[Photon] Backend '%s' selected in ReFix.ini; local Photon emulator off (real/self-hosted Photon)", backend);
        return;
    }
    char suffixes[512] = { 0 };
    GetPrivateProfileStringA("Photon", "HostSuffixes", kDefaultHostSuffixes, suffixes, sizeof(suffixes), ini.c_str());
    g_channel = GetPrivateProfileIntA("Photon", "SteamChannel", 9, ini.c_str());

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    OpenListeners(kNameServerPorts, _countof(kNameServerPorts), PnServer::Role::NameServer, nullptr);
    OpenListeners(kMasterPorts, _countof(kMasterPorts), PnServer::Role::Master, &g_masterAddress);
    OpenListeners(kGamePorts, _countof(kGamePorts), PnServer::Role::Game, &g_gameAddress);
    if (g_masterAddress.empty() || g_gameAddress.empty()) {
        Log("[Photon] Emulator ports unavailable; Photon P2P disabled");
        return;
    }
    PnDns::Install(Split(suffixes, ';'));
    HANDLE h = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

} // namespace ReFixPhotonP2P
