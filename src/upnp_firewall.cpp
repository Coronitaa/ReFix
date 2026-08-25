// =============================================================================
// ReFix - UPnP & Windows Firewall Native Network Helper Implementation
// =============================================================================

#include "upnp_firewall.h"
#include <natupnp.h>
#include <wininet.h>
#include <netfw.h>
#include <comdef.h>
#include <cstdio>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace ReFixNet {

void AutoOpenPorts() {
    static bool s_opened = false;
    if (s_opened) return;
    s_opened = true;

    std::thread([]() {
        AddFirewallRule(7777, L"ReFix Game P2P (7777 UDP)");
        AddFirewallRule(7778, L"ReFix Game P2P (7778 UDP)");
        AddFirewallRule(7779, L"ReFix Game P2P (7779 UDP)");
        AddFirewallRule(27015, L"ReFix Steam Query (27015 UDP)");

        MapUPnPPort(7777, L"ReFix P2P Server 7777");
        MapUPnPPort(7778, L"ReFix P2P Server 7778");
        MapUPnPPort(7779, L"ReFix P2P Server 7779");
    }).detach();
}

bool AddFirewallRule(uint16_t port, const wchar_t* ruleName) {
    HRESULT hr = S_OK;
    INetFwPolicy2* pNetFwPolicy2 = nullptr;
    INetFwRules* pFwRules = nullptr;
    INetFwRule* pFwRule = nullptr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
    if (FAILED(hr)) return false;

    hr = pNetFwPolicy2->get_Rules(&pFwRules);
    if (FAILED(hr)) { pNetFwPolicy2->Release(); return false; }

    hr = CoCreateInstance(__uuidof(NetFwRule), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwRule), (void**)&pFwRule);
    if (FAILED(hr)) { pFwRules->Release(); pNetFwPolicy2->Release(); return false; }

    wchar_t portStr[16];
    swprintf_s(portStr, 16, L"%u", port);

    BSTR bstrName = SysAllocString(ruleName);
    BSTR bstrDesc = SysAllocString(L"ReFix Direct P2P Game Server Port");
    BSTR bstrPorts = SysAllocString(portStr);

    pFwRule->put_Name(bstrName);
    pFwRule->put_Description(bstrDesc);
    pFwRule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
    pFwRule->put_LocalPorts(bstrPorts);
    pFwRule->put_Direction(NET_FW_RULE_DIR_IN);
    pFwRule->put_Enabled(VARIANT_TRUE);
    pFwRule->put_Action(NET_FW_ACTION_ALLOW);

    hr = pFwRules->Add(pFwRule);

    SysFreeString(bstrName);
    SysFreeString(bstrDesc);
    SysFreeString(bstrPorts);

    pFwRule->Release();
    pFwRules->Release();
    pNetFwPolicy2->Release();

    return SUCCEEDED(hr);
}

bool MapUPnPPort(uint16_t port, const wchar_t* description) {
    HRESULT hr = S_OK;
    IUPnPNAT* pUNat = nullptr;
    IStaticPortMappingCollection* pPortMappings = nullptr;
    IStaticPortMapping* pPortMapping = nullptr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = CoCreateInstance(CLSID_UPnPNAT, NULL, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, (void**)&pUNat);
    if (FAILED(hr) || !pUNat) return false;

    hr = pUNat->get_StaticPortMappingCollection(&pPortMappings);
    if (FAILED(hr) || !pPortMappings) { pUNat->Release(); return false; }

    std::string localIP = GetLocalIP();
    if (localIP.empty()) { pPortMappings->Release(); pUNat->Release(); return false; }

    wchar_t wLocalIP[64];
    MultiByteToWideChar(CP_ACP, 0, localIP.c_str(), -1, wLocalIP, 64);

    hr = pPortMappings->Add((LONG)port, (BSTR)L"UDP", (LONG)port, (BSTR)wLocalIP, VARIANT_TRUE, (BSTR)description, &pPortMapping);

    if (pPortMapping) pPortMapping->Release();
    pPortMappings->Release();
    pUNat->Release();

    return SUCCEEDED(hr);
}

void UnmapUPnPPort(uint16_t port) {
    IUPnPNAT* pUNat = nullptr;
    IStaticPortMappingCollection* pPortMappings = nullptr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (SUCCEEDED(CoCreateInstance(CLSID_UPnPNAT, NULL, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, (void**)&pUNat)) && pUNat) {
        if (SUCCEEDED(pUNat->get_StaticPortMappingCollection(&pPortMappings)) && pPortMappings) {
            pPortMappings->Remove((LONG)port, (BSTR)L"UDP");
            pPortMappings->Release();
        }
        pUNat->Release();
    }
}

std::string GetLocalIP() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return "127.0.0.1";

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        WSACleanup();
        return "127.0.0.1";
    }

    struct addrinfo hints = {}, *info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string ip = "127.0.0.1";
    if (getaddrinfo(hostname, NULL, &hints, &info) == 0 && info) {
        struct sockaddr_in* sa = (struct sockaddr_in*)info->ai_addr;
        char ipBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sa->sin_addr), ipBuf, sizeof(ipBuf));
        ip = ipBuf;
        freeaddrinfo(info);
    }

    WSACleanup();
    return ip;
}

std::string GetPublicIP() {
    HINTERNET hSession = InternetOpenA("ReFixPublicIPCheck", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hSession) return GetLocalIP();

    HINTERNET hConnect = InternetOpenUrlA(hSession, "http://api.ipify.org", NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hSession);
        return GetLocalIP();
    }

    char buffer[64] = { 0 };
    DWORD bytesRead = 0;
    std::string publicIP = "";

    if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        publicIP = buffer;
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);

    return publicIP.empty() ? GetLocalIP() : publicIP;
}

} // namespace ReFixNet