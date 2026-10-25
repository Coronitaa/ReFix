#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "pn_dns.h"
#include "../minhook/MinHook.h"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>

extern void ReFixLog(const char* fmt, ...);

namespace PnDns {

namespace {

constexpr const char* kLoopback = "127.0.0.1";
constexpr const wchar_t* kLoopbackW = L"127.0.0.1";

typedef int (WSAAPI* fn_getaddrinfo_t)(PCSTR, PCSTR, const ADDRINFOA*, PADDRINFOA*);
typedef int (WSAAPI* fn_GetAddrInfoW_t)(PCWSTR, PCWSTR, const ADDRINFOW*, PADDRINFOW*);
typedef hostent* (WSAAPI* fn_gethostbyname_t)(const char*);

fn_getaddrinfo_t g_origGetAddrInfo = nullptr;
fn_GetAddrInfoW_t g_origGetAddrInfoW = nullptr;
fn_gethostbyname_t g_origGetHostByName = nullptr;
std::vector<std::string> g_suffixes;
std::mutex g_logMutex;
std::set<std::string> g_logged;

bool Matches(std::string host) {
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return (char)tolower(c); });
    for (const auto& suffix : g_suffixes) {
        if (host.size() >= suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0) {
            std::lock_guard<std::mutex> lock(g_logMutex);
            if (g_logged.insert(host).second) ReFixLog("[Photon-DNS] %s -> %s (local emulator)", host.c_str(), kLoopback);
            return true;
        }
    }
    return false;
}

std::string Narrow(PCWSTR w) {
    std::string s;
    for (; w && *w; w++) s += (char)(*w < 128 ? *w : '?');
    return s;
}

int WSAAPI Hooked_getaddrinfo(PCSTR node, PCSTR service, const ADDRINFOA* hints, PADDRINFOA* result) {
    if (node && Matches(node)) node = kLoopback;
    return g_origGetAddrInfo(node, service, hints, result);
}

int WSAAPI Hooked_GetAddrInfoW(PCWSTR node, PCWSTR service, const ADDRINFOW* hints, PADDRINFOW* result) {
    if (node && Matches(Narrow(node))) node = kLoopbackW;
    return g_origGetAddrInfoW(node, service, hints, result);
}

hostent* WSAAPI Hooked_gethostbyname(const char* name) {
    if (name && Matches(name)) name = kLoopback;
    return g_origGetHostByName(name);
}

bool Hook(HMODULE ws2, const char* name, void* detour, void** original) {
    void* target = (void*)GetProcAddress(ws2, name);
    if (!target) return false;
    MH_STATUS st = MH_CreateHook(target, detour, original);
    return (st == MH_OK || st == MH_ERROR_ALREADY_CREATED) && MH_EnableHook(target) == MH_OK;
}

} // namespace

bool Install(const std::vector<std::string>& hostSuffixes) {
    g_suffixes.clear();
    for (std::string s : hostSuffixes) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
        if (!s.empty()) g_suffixes.push_back(s);
    }
    HMODULE ws2 = LoadLibraryA("ws2_32.dll");
    if (!ws2) return false;
    MH_Initialize(); // no-op if another ReFix module already initialized MinHook
    bool a = Hook(ws2, "getaddrinfo", (void*)&Hooked_getaddrinfo, (void**)&g_origGetAddrInfo);
    bool w = Hook(ws2, "GetAddrInfoW", (void*)&Hooked_GetAddrInfoW, (void**)&g_origGetAddrInfoW);
    bool h = Hook(ws2, "gethostbyname", (void*)&Hooked_gethostbyname, (void**)&g_origGetHostByName);
    ReFixLog("[Photon-DNS] Hooks getaddrinfo=%d GetAddrInfoW=%d gethostbyname=%d for %zu suffix(es)", a, w, h, g_suffixes.size());
    return a || w || h;
}

} // namespace PnDns
