#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "network_mode.h"

#pragma comment(lib, "ws2_32.lib")

// Helper to determine if an IP is local/LAN
static bool IsAllowedLanEndpoint(const sockaddr* addr) {
    if (!addr) return false;

    if (addr->sa_family == AF_INET) {
        const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(addr);
        uint32_t ip = ntohl(sin->sin_addr.S_un.S_addr);
        if ((ip >= 0x0A000000 && ip <= 0x0AFFFFFF) || // 10.0.0.0/8
            (ip >= 0xAC100000 && ip <= 0xAC1FFFFF) || // 172.16.0.0/12
            (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF) || // 192.168.0.0/16
            (ip >= 0xA9FE0000 && ip <= 0xA9FEFFFF) || // 169.254.0.0/16
            (ip >= 0x7F000000 && ip <= 0x7FFFFFFF) || // 127.0.0.0/8
            (ip >= 0xE0000000 && ip <= 0xEFFFFFFF) || // Multicast 224.0.0.0/4
            (ip == 0xFFFFFFFF))                       // Broadcast 255.255.255.255
        {
            return true;
        }
        return false;
    } else if (addr->sa_family == AF_INET6) {
        const sockaddr_in6* sin6 = reinterpret_cast<const sockaddr_in6*>(addr);
        const uint8_t* b = sin6->sin6_addr.u.Byte;
        
        bool isLoopback = true;
        for (int i=0; i<15; i++) if (b[i] != 0) isLoopback = false;
        if (isLoopback && b[15] == 1) return true;
        
        if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) return true; // Link-local fe80::/10
        if ((b[0] & 0xFE) == 0xFC) return true;                 // ULA fc00::/7
        if (b[0] == 0xFF) return true;                          // Multicast ff00::/8
        
        return false;
    }
    return true;
}

void test_ip4(const char* ip, bool expected) {
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    bool res = IsAllowedLanEndpoint((sockaddr*)&addr);
    std::cout << "IPv4 " << ip << " -> " << (res ? "ALLOWED" : "BLOCKED") 
              << (res == expected ? " [PASS]" : " [FAIL]") << std::endl;
}

void test_ip6(const char* ip, bool expected) {
    sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, ip, &addr.sin6_addr);
    bool res = IsAllowedLanEndpoint((sockaddr*)&addr);
    std::cout << "IPv6 " << ip << " -> " << (res ? "ALLOWED" : "BLOCKED") 
              << (res == expected ? " [PASS]" : " [FAIL]") << std::endl;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    std::cout << "=== NETWORK ISOLATION TEST ===" << std::endl;
    test_ip4("127.0.0.1", true);
    test_ip4("192.168.1.55", true);
    test_ip4("10.0.0.5", true);
    test_ip4("172.16.0.5", true);
    test_ip4("169.254.1.2", true);
    test_ip4("239.255.71.84", true); // Multicast
    test_ip4("255.255.255.255", true);
    test_ip4("8.8.8.8", false); // External Google DNS
    test_ip4("1.1.1.1", false); // External Cloudflare DNS

    test_ip6("::1", true); // Loopback
    test_ip6("fe80::1", true); // Link-local
    test_ip6("fc00::1", true); // ULA
    test_ip6("ff02::1", true); // Multicast
    test_ip6("2001:4860:4860::8888", false); // External Google DNS IPv6

    return 0;
}
