#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "pn_shared.h"
#include <cstring>
#include <vector>

// Datagram protocol shared with src/fusion_shared/Bridge.cs (one message per loopback datagram, little-endian).
namespace PnShared {
namespace {

constexpr char MSG_CONFIG = 'C', MSG_JOIN = 'J', MSG_LEAVE = 'L', MSG_MASTER = 'M', MSG_DATA = 'D',
               MSG_KEEP_ALIVE = 'K', MSG_QUIT = 'Q', MSG_HELLO = 'H', MSG_RUNNING = 'R';
constexpr uint32_t kKeepAliveMs = 1000;
constexpr size_t kMaxDatagram = 65507;

void PutI32(std::string& s, int32_t v) {
    for (int i = 0; i < 4; i++) s.push_back((char)((uint32_t)v >> (8 * i)));
}

int32_t GetI32(const char* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)(uint8_t)p[i] << (8 * i);
    return (int32_t)v;
}

class ProcessLink : public Link {
public:
    ProcessLink(SOCKET sock, HANDLE job, HANDLE process, void (*log)(const char*, ...))
        : m_sock(sock), m_job(job), m_process(process), m_log(log) {}

    ~ProcessLink() override {
        Send(std::string(1, MSG_QUIT));
        closesocket(m_sock);
        // Closing the job (KILL_ON_JOB_CLOSE) ends the server even if it ignored the quit request.
        CloseHandle(m_process);
        CloseHandle(m_job);
    }

    void Config(int32_t maxPlayers, const std::string& json) override {
        std::string m(1, MSG_CONFIG);
        PutI32(m, maxPlayers);
        Send(m + json);
    }
    void Join(int32_t actor, const std::string& uniqueId) override {
        std::string m(1, MSG_JOIN);
        PutI32(m, actor);
        std::string id = uniqueId;
        id.resize(8, '\0');
        Send(m + id);
    }
    void Leave(int32_t actor) override { SendActor(MSG_LEAVE, actor); }
    void Master(int32_t actor) override { SendActor(MSG_MASTER, actor); }
    void Data(int32_t fromActor, const std::string& packet, bool reliable) override {
        std::string m(1, MSG_DATA);
        PutI32(m, fromActor);
        m.push_back(reliable ? 1 : 0);
        Send(m + packet);
    }

    bool Poll(uint32_t nowMs, const DataFn& onData) override {
        bool started = false;
        std::vector<char> buf(kMaxDatagram);
        for (;;) {
            sockaddr_in from = {};
            int fromLen = sizeof(from);
            int n = recvfrom(m_sock, buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
            if (n == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAECONNRESET) continue; // ICMP from a datagram sent before the server was up
                break;
            }
            if (n <= 0 || from.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;
            if (buf[0] == MSG_HELLO) {
                m_server = from;
                m_connected = true;
                m_log("[Photon] Fusion Shared server process connected (port %u)", ntohs(from.sin_port));
                for (const std::string& m : m_queue) SendNow(m);
                m_queue.clear();
            } else if (from.sin_port != m_server.sin_port) {
                continue;
            } else if (buf[0] == MSG_RUNNING) {
                started = true;
            } else if (buf[0] == MSG_DATA && n > 6) {
                onData(GetI32(&buf[1]), std::string(&buf[6], n - 6), buf[5] != 0);
            }
        }
        if (m_connected && nowMs - m_lastKeepAlive >= kKeepAliveMs) {
            m_lastKeepAlive = nowMs;
            SendNow(std::string(1, MSG_KEEP_ALIVE));
        }
        DWORD exitCode = 0;
        if (!m_exitLogged && GetExitCodeProcess(m_process, &exitCode) && exitCode != STILL_ACTIVE) {
            m_exitLogged = true;
            m_log("[Photon] Fusion Shared server process exited (code %lu); see ReFixFusionShared.log", exitCode);
        }
        return started;
    }

private:
    void SendActor(char kind, int32_t actor) {
        std::string m(1, kind);
        PutI32(m, actor);
        Send(m);
    }
    void Send(const std::string& m) {
        if (m_connected) SendNow(m);
        else m_queue.push_back(m);
    }
    void SendNow(const std::string& m) {
        if (m.size() > kMaxDatagram) return;
        sendto(m_sock, m.data(), (int)m.size(), 0, (const sockaddr*)&m_server, sizeof(m_server));
    }

    SOCKET m_sock;
    HANDLE m_job;
    HANDLE m_process;
    void (*m_log)(const char*, ...);
    sockaddr_in m_server = {};
    bool m_connected = false;
    bool m_exitLogged = false;
    uint32_t m_lastKeepAlive = 0;
    std::vector<std::string> m_queue; // sent once the server said hello
};

} // namespace

std::unique_ptr<Link> StartProcess(const std::string& exePath, const std::string& logPath,
                                   void (*log)(const char* fmt, ...)) {
    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        log("[Photon] Fusion Shared mode needs %s (not found)", exePath.c_str());
        return nullptr;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return nullptr;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int addrLen = sizeof(addr);
    u_long nonBlocking = 1;
    int bufSize = 4 << 20;
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0 || getsockname(sock, (sockaddr*)&addr, &addrLen) != 0 ||
        ioctlsocket(sock, FIONBIO, &nonBlocking) != 0) {
        closesocket(sock);
        return nullptr;
    }
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize));

    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    char fullLog[MAX_PATH] = { 0 };
    std::string actualLog = logPath;
    if (!logPath.empty() && GetFullPathNameA(logPath.c_str(), MAX_PATH, fullLog, nullptr)) actualLog = fullLog;
    std::string cmd = "\"" + exePath + "\" " + std::to_string(ntohs(addr.sin_port)) + " \"" + actualLog + "\"";
    size_t slash = exePath.find_last_of("\\/");
    std::string workDir = (slash != std::string::npos) ? exePath.substr(0, slash) : ".";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(exePath.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        nullptr, workDir.c_str(), &si, &pi)) {
        log("[Photon] Could not start the Fusion Shared server (error %lu)", GetLastError());
        closesocket(sock);
        if (job) CloseHandle(job);
        return nullptr;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    log("[Photon] Fusion Shared server started (pid %lu, emulator port %u)", pi.dwProcessId, ntohs(addr.sin_port));
    return std::make_unique<ProcessLink>(sock, job, pi.hProcess, log);
}

} // namespace PnShared
