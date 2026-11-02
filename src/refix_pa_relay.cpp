#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "refix_pa_relay.h"
#include "refix_pa_gql.h"
#include "refix_steam_flat.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPARelay {

namespace {

constexpr uint8_t kJsonFlag = 0x01;
constexpr uint8_t kBoltFlag = 0x02;
constexpr size_t kEndpointBytes = 6; // ipv4:4 + port:2
constexpr uint8_t kToHost = 0x01;    // Steam envelope: client -> host Bolt payload
constexpr uint8_t kToClient = 0x02;  // Steam envelope: host -> client Bolt payload
constexpr int kMaxDatagram = 65536;
constexpr int kSelectTimeoutUs = 1000;
constexpr int kMaxSteamBatch = 64;
constexpr DWORD kAcceptSweepMs = 2000;
constexpr uint8_t kVirtualNet0 = 10, kVirtualNet1 = 80;

std::atomic<bool> g_started{ false };
std::atomic<uint16_t> g_port{ 0 };
int g_channel = 7;
SOCKET g_socket = INVALID_SOCKET;

// Relay state; only touched by the relay thread.
sockaddr_in g_serverProxy = {}; // P2PServerProxy socket (source of SERVER_LOGIN)
sockaddr_in g_clientProxy = {}; // P2PClientProxy socket (source of CLIENT_LOGIN / CLIENT_QUERY)
bool g_hasServerProxy = false, g_hasClientProxy = false;
std::map<uint64_t, uint32_t> g_virtualBySteam; // SteamID -> virtual IPv4 (host byte order)
std::map<uint32_t, uint64_t> g_steamByVirtual;

std::string IniPath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
}

uint32_t VirtualAddress(uint64_t steamID) {
    auto it = g_virtualBySteam.find(steamID);
    if (it != g_virtualBySteam.end()) return it->second;
    uint32_t index = (uint32_t)g_virtualBySteam.size() + 1;
    uint32_t ip = ((uint32_t)kVirtualNet0 << 24) | ((uint32_t)kVirtualNet1 << 16) | (index & 0xFFFF);
    g_virtualBySteam[steamID] = ip;
    g_steamByVirtual[ip] = steamID;
    ReFixLog("[PA-Relay] Peer %llu -> virtual endpoint %u.%u.%u.%u", (unsigned long long)steamID, ip >> 24, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);
    return ip;
}

std::string DottedAddress(uint32_t ip) {
    return std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 0xFF) + "." + std::to_string((ip >> 8) & 0xFF) + "." +
           std::to_string(ip & 0xFF);
}

uint32_t HeaderAddress(const uint8_t* header) {
    return ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) | header[3];
}

void SendTo(const sockaddr_in& to, const std::string& data) {
    sendto(g_socket, data.data(), (int)data.size(), 0, (const sockaddr*)&to, sizeof(to));
}

void SendJson(const sockaddr_in& to, const std::string& json) {
    SendTo(to, std::string(1, (char)kJsonFlag) + json);
}

std::string EndpointReply(const char* msgType, bool ok, const std::string& ip, uint16_t port) {
    return std::string("{\"msgType\":\"") + msgType + "\",\"isSuccess\":" + (ok ? "true" : "false") + ",\"ip\":" +
           ReFixPAGql::JsonString(ip) + ",\"port\":" + std::to_string(port) + "}";
}

// --- Packets from the local proxies -------------------------------------------------------------------

void HandleJson(const sockaddr_in& from, const std::string& json) {
    std::string type = ReFixPAGql::JsonStringMember(json, "msgType");
    if (type == "SERVER_LOGIN") {
        g_serverProxy = from;
        g_hasServerProxy = true;
        ReFixLog("[PA-Relay] Host proxy logged in (%s), local port %u", json.c_str(), ntohs(from.sin_port));
        SendJson(from, EndpointReply("SERVER_LOGIN_REPLAY", true, "127.0.0.1", g_port.load()));
    } else if (type == "CLIENT_LOGIN") {
        g_clientProxy = from;
        g_hasClientProxy = true;
        ReFixLog("[PA-Relay] Client proxy logged in (%s), local port %u", json.c_str(), ntohs(from.sin_port));
    } else if (type == "CLIENT_QUERY") {
        g_clientProxy = from;
        g_hasClientProxy = true;
        std::string target = ReFixPAGql::JsonStringMember(json, "targetId");
        uint64_t steamID = strtoull(target.c_str(), nullptr, 10);
        bool ok = steamID != 0 && steamID != ReFixSteamFlat::LocalSteamID();
        uint32_t ip = ok ? VirtualAddress(steamID) : 0;
        ReFixLog("[PA-Relay] CLIENT_QUERY target '%s' -> %s", target.c_str(), ok ? DottedAddress(ip).c_str() : "rejected");
        SendJson(from, EndpointReply("CLIENT_QUERY_REPLY", ok, ok ? DottedAddress(ip) : "0", g_port.load()));
    } else if (type != "PING_PONG") {
        ReFixLog("[PA-Relay] Unhandled control message: %s", json.c_str());
    }
}

// [0x02][peer endpoint][payload] -> Steam envelope [direction][payload] to the peer behind the endpoint.
void HandleBolt(const sockaddr_in& from, const uint8_t* data, int size) {
    if (size <= (int)(1 + kEndpointBytes)) return;
    auto it = g_steamByVirtual.find(HeaderAddress(data + 1));
    if (it == g_steamByVirtual.end()) return;
    bool fromHost = g_hasServerProxy && from.sin_port == g_serverProxy.sin_port;
    std::vector<uint8_t> envelope(1 + (size_t)size - 1 - kEndpointBytes);
    envelope[0] = fromHost ? kToClient : kToHost;
    memcpy(envelope.data() + 1, data + 1 + kEndpointBytes, envelope.size() - 1);
    ReFixSteamFlat::SendMessage(it->second, envelope.data(), (uint32_t)envelope.size(), g_channel);
}

void DrainLocalSocket() {
    static uint8_t buf[kMaxDatagram];
    for (;;) {
        sockaddr_in from = {};
        int fromLen = sizeof(from);
        int n = recvfrom(g_socket, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if (n <= 0) return;
        if (buf[0] == kJsonFlag) HandleJson(from, std::string((const char*)buf + 1, (size_t)n - 1));
        else if (buf[0] == kBoltFlag) HandleBolt(from, buf, n);
    }
}

// --- Packets from Steam peers --------------------------------------------------------------------------

void HandleSteamMessage(uint64_t steamID, const uint8_t* data, uint32_t size) {
    if (size < 2) return;
    bool toHost = data[0] == kToHost;
    const sockaddr_in& proxy = toHost ? g_serverProxy : g_clientProxy;
    if (!(toHost ? g_hasServerProxy : g_hasClientProxy)) return;
    uint32_t ip = VirtualAddress(steamID);
    std::string packet(1 + kEndpointBytes, '\0');
    packet[0] = (char)kBoltFlag;
    packet[1] = (char)(ip >> 24); packet[2] = (char)(ip >> 16); packet[3] = (char)(ip >> 8); packet[4] = (char)ip;
    packet[5] = (char)(g_port.load() >> 8); packet[6] = (char)g_port.load();
    packet.append((const char*)data + 1, size - 1);
    SendTo(proxy, packet);
}

DWORD WINAPI RelayThread(LPVOID) {
    DWORD lastSweep = 0;
    for (;;) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(g_socket, &readable);
        timeval timeout = { 0, kSelectTimeoutUs };
        if (select(0, &readable, nullptr, nullptr, &timeout) > 0) DrainLocalSocket();
        while (ReFixSteamFlat::ReceiveMessages(g_channel, kMaxSteamBatch, HandleSteamMessage) == kMaxSteamBatch) {}
        DWORD now = GetTickCount();
        if (g_hasServerProxy && now - lastSweep >= kAcceptSweepMs) { // hosts accept joining friends' sessions
            ReFixSteamFlat::AcceptFriendSessions();
            lastSweep = now;
        }
    }
}

bool OpenSocket(uint16_t port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket == INVALID_SOCKET) return false;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    u_long nonBlocking = 1;
    BOOL reportConnReset = FALSE; // a closed proxy socket must not surface as WSAECONNRESET on our recvfrom
    DWORD bytes = 0;
    WSAIoctl(g_socket, _WSAIOW(IOC_VENDOR, 12) /* SIO_UDP_CONNRESET */, &reportConnReset, sizeof(reportConnReset), nullptr, 0, &bytes,
             nullptr, nullptr);
    if (bind(g_socket, (sockaddr*)&addr, sizeof(addr)) != 0 || ioctlsocket(g_socket, FIONBIO, &nonBlocking) != 0) {
        int error = WSAGetLastError(); // closesocket resets it; the caller logs it
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        WSASetLastError(error);
        return false;
    }
    g_port.store(port);
    return true;
}

} // namespace

void Initialize() {
    if (g_started.exchange(true)) return;
    std::string ini = IniPath();
    uint16_t port = (uint16_t)GetPrivateProfileIntA("P2P", "RelayPort", 27100, ini.c_str());
    g_channel = GetPrivateProfileIntA("P2P", "RelayChannel", 7, ini.c_str());
    if (!OpenSocket(port)) {
        ReFixLog("[PA-Relay] Could not bind 127.0.0.1:%u (%d); P2P rooms unavailable", port, WSAGetLastError());
        return;
    }
    HANDLE h = CreateThread(nullptr, 0, RelayThread, nullptr, 0, nullptr);
    if (!h) {
        ReFixLog("[PA-Relay] Could not start relay thread");
        return;
    }
    CloseHandle(h);
    ReFixLog("[PA-Relay] ServerlessP2P relay on 127.0.0.1:%u, Steam channel %d", port, g_channel);
}

uint16_t Port() {
    return g_port.load();
}

} // namespace ReFixPARelay
