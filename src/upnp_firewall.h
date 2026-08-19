// =============================================================================
// ReFix - UPnP & Windows Firewall Native Network Helper
// =============================================================================
// Automatically configures Windows Firewall rules & UPnP router port forwarding.
// Resolves Host Public IP for seamless 1-click Steam Overlay invitations.
// =============================================================================

#ifndef REFIX_UPNP_FIREWALL_H
#define REFIX_UPNP_FIREWALL_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

namespace ReFixNet {
    // Automatically opens Windows Firewall & UPnP ports (7777-7779 UDP) on game launch
    void AutoOpenPorts();

    // Adds inbound UDP rule to Windows Firewall
    bool AddFirewallRule(uint16_t port, const wchar_t* ruleName);

    // Maps external UDP port to local machine via native UPnP COM interface (IUPnPNAT)
    bool MapUPnPPort(uint16_t port, const wchar_t* description);

    // Removes UPnP port mapping upon shutdown
    void UnmapUPnPPort(uint16_t port);

    // Obtains host local IPv4 address
    std::string GetLocalIP();

    // Obtains host public IPv4 address (via lightweight HTTP lookup)
    std::string GetPublicIP();
}

#endif // REFIX_UPNP_FIREWALL_H
