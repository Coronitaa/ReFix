#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "refix_pa_backend.h"
#include "refix_il2cpp_scene.h"
#include "refix_pa_services.h"
#include "minhook/MinHook.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPABackend {

namespace {

constexpr size_t kMaxHeaderBytes = 64 * 1024;
constexpr size_t kMaxBodyBytes = 8 * 1024 * 1024;
constexpr size_t kMaxLoggedBodyChars = 700;
constexpr DWORD kSocketTimeoutMs = 15000;

std::atomic<uint16_t> g_port{ 0 };
std::atomic<bool> g_started{ false };
std::string g_hostFilter; // backend hosts contain this substring

typedef void* (*fn_NewUri_t)(void* str, void* method);
fn_NewUri_t g_origNewUri = nullptr;

std::string IniPath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
}

std::string Printable(const std::string& s) {
    if (s.size() >= 2 && (uint8_t)s[0] == 0x1F && (uint8_t)s[1] == 0x8B) return "<gzip " + std::to_string(s.size()) + " bytes>";
    std::string out = s.substr(0, kMaxLoggedBodyChars);
    for (char& c : out) {
        if (c == '\r' || c == '\n') c = ' ';
        else if ((uint8_t)c < 0x20) c = '.';
    }
    return s.size() > kMaxLoggedBodyChars ? out + "..." : out;
}

// --- Request handling -------------------------------------------------------------------------

Response Dispatch(const Request& req) {
    ReFixLog("[PA-Backend] %s %s%s body=%s", req.method.c_str(), req.host.c_str(), req.path.c_str(), Printable(req.body).c_str());
    ReFixPAServices::Result served = ReFixPAServices::HandleRequest(req.body);
    Response res;
    if (served.handled) {
        res.status = 200;
        res.body = served.body;
        return res;
    }
    // Not served locally: drop the connection so the game sees the same network failure as the unreachable
    // cloud. Any HTTP 503 would instead start MaintenanceManager's "scheduled maintenance" screen.
    res.status = 0;
    return res;
}

// --- Minimal HTTP/1.1 server --------------------------------------------------------------------

bool RecvUntilHeadersEnd(SOCKET s, std::string& data, size_t& headerEnd) {
    char buf[4096];
    while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos) {
        if (data.size() > kMaxHeaderBytes) return false;
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        data.append(buf, n);
    }
    return true;
}

size_t ContentLength(const std::string& headers) {
    std::string lower = headers;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    size_t pos = lower.find("\r\ncontent-length:");
    if (pos == std::string::npos) return 0;
    return (size_t)strtoull(lower.c_str() + pos + 17, nullptr, 10);
}

void SendResponse(SOCKET s, const Response& res) {
    const char* reason = res.status == 200 ? "OK" : (res.status == 404 ? "Not Found" : "Service Unavailable");
    std::string out = "HTTP/1.1 " + std::to_string(res.status) + " " + reason +
                      "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                      std::to_string(res.body.size()) + "\r\nConnection: close\r\n\r\n" + res.body;
    size_t sent = 0;
    while (sent < out.size()) {
        int n = send(s, out.data() + sent, (int)(out.size() - sent), 0);
        if (n <= 0) return;
        sent += (size_t)n;
    }
}

DWORD WINAPI ConnectionThread(LPVOID param) {
    SOCKET s = (SOCKET)(uintptr_t)param;
    DWORD timeout = kSocketTimeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    std::string data;
    size_t headerEnd = 0;
    if (RecvUntilHeadersEnd(s, data, headerEnd)) {
        Request req;
        std::string head = data.substr(0, headerEnd);
        size_t lineEnd = head.find("\r\n");
        std::string requestLine = head.substr(0, lineEnd);
        req.headers = lineEnd == std::string::npos ? "" : head.substr(lineEnd);
        size_t sp1 = requestLine.find(' ');
        size_t sp2 = requestLine.find(' ', sp1 + 1);
        req.method = requestLine.substr(0, sp1);
        std::string target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1); // "/<host>/<path>"
        size_t slash = target.find('/', 1);
        req.host = target.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
        req.path = slash == std::string::npos ? "/" : target.substr(slash);

        size_t length = ContentLength(req.headers);
        req.body = data.substr(headerEnd + 4);
        char buf[8192];
        while (req.body.size() < length && length <= kMaxBodyBytes) {
            int n = recv(s, buf, sizeof(buf), 0);
            if (n <= 0) break;
            req.body.append(buf, n);
        }
        Response res = Dispatch(req);
        if (res.status != 0) SendResponse(s, res);
    }
    shutdown(s, SD_BOTH);
    closesocket(s);
    return 0;
}

DWORD WINAPI AcceptThread(LPVOID param) {
    SOCKET listener = (SOCKET)(uintptr_t)param;
    for (;;) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            ReFixLog("[PA-Backend] accept failed (%d); local backend stopped", WSAGetLastError());
            return 0;
        }
        HANDLE h = CreateThread(nullptr, 0, ConnectionThread, (LPVOID)(uintptr_t)client, 0, nullptr);
        if (h) CloseHandle(h);
        else closesocket(client);
    }
}

bool StartServer() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return false;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral: avoids the game/voice ports the P2P Winsock layer classifies
    int len = sizeof(addr);
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listener, SOMAXCONN) != 0 ||
        getsockname(listener, (sockaddr*)&addr, &len) != 0) {
        closesocket(listener);
        return false;
    }
    g_port.store(ntohs(addr.sin_port));
    HANDLE h = CreateThread(nullptr, 0, AcceptThread, (LPVOID)(uintptr_t)listener, 0, nullptr);
    if (!h) {
        closesocket(listener);
        g_port.store(0);
        return false;
    }
    CloseHandle(h);
    return true;
}

// --- URL redirection -----------------------------------------------------------------------------

constexpr const char* kUnconfiguredHost = "lambda.unconfigured";
std::mutex g_seenHostsMutex;
std::set<std::string> g_seenHosts;

void LogHostOnce(const std::string& host, bool redirected) {
    std::lock_guard<std::mutex> lock(g_seenHostsMutex);
    if (g_seenHosts.insert(host).second) {
        ReFixLog("[PA-Backend] URL host '%s' %s", host.c_str(), redirected ? "-> local backend" : "left untouched");
    }
}

// "https://host/path?q" -> "http://127.0.0.1:<port>/host/path?q" for backend hosts; empty otherwise.
// An empty URL is what the game builds when its remote config center (lambda endpoints) is unreachable;
// it would fail anyway, so it is served locally too.
std::string RewriteUrl(const std::string& url) {
    std::string base = "http://127.0.0.1:" + std::to_string(g_port.load()) + "/";
    if (url.empty()) return base + kUnconfiguredHost + "/";
    size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "";
    size_t hostStart = scheme + 3;
    size_t pathStart = url.find('/', hostStart);
    std::string host = url.substr(hostStart, pathStart == std::string::npos ? std::string::npos : pathStart - hostStart);
    bool redirect = !g_hostFilter.empty() && host.find(g_hostFilter) != std::string::npos;
    LogHostOnce(host, redirect);
    if (!redirect) return "";
    std::string path = pathStart == std::string::npos ? "/" : url.substr(pathStart);
    return base + host + path;
}

void* Hooked_NewUri(void* str, void* method) {
    if (g_port.load()) {
        std::string url = str ? ReFixIL2CPPScene::StringToUtf8(str) : std::string();
        std::string rewritten = RewriteUrl(url);
        if (!rewritten.empty()) {
            void* replacement = ReFixIL2CPPScene::NewString(rewritten.c_str());
            if (replacement) str = replacement;
        }
    }
    return g_origNewUri ? g_origNewUri(str, method) : nullptr;
}

bool HookNewUri() {
    void* klass = ReFixIL2CPPScene::FindClassByName("", "UriHelper");
    void* code = klass ? ReFixIL2CPPScene::GetMethodCode(klass, "NewUri", 1) : nullptr;
    if (!code) return false;
    return MH_CreateHook(code, (void*)&Hooked_NewUri, (void**)&g_origNewUri) == MH_OK && MH_EnableHook(code) == MH_OK;
}

} // namespace

void Initialize() {
    if (g_started.exchange(true)) return;
    char enabled[16] = { 0 };
    char filter[128] = { 0 };
    // Opt-in per game (Party Animals' ReFix.ini sets it); other games must not get a loopback HTTP server.
    GetPrivateProfileStringA("P2P", "LocalBackend", "false", enabled, sizeof(enabled), IniPath().c_str());
    GetPrivateProfileStringA("P2P", "LocalBackendHost", "partyanimalsgame.com", filter, sizeof(filter), IniPath().c_str());
    if (_stricmp(enabled, "true") != 0 && strcmp(enabled, "1") != 0) {
        ReFixLog("[PA-Backend] LocalBackend disabled in ReFix.ini");
        return;
    }
    g_hostFilter = filter;
    if (!StartServer()) {
        ReFixLog("[PA-Backend] Could not start loopback server (%d)", WSAGetLastError());
        return;
    }
    bool hooked = HookNewUri();
    ReFixLog("[PA-Backend] Local backend on 127.0.0.1:%u for hosts *%s* (UriHelper.NewUri hook %s)", g_port.load(),
             g_hostFilter.c_str(), hooked ? "installed" : "FAILED");
}

uint16_t Port() {
    return g_port.load();
}

} // namespace ReFixPABackend
