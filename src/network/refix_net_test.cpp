#include <iostream>
#include <windows.h>
#include <thread>
#include "steam_api.h"
#include "isteamnetworkingsockets.h"

int main(int argc, char** argv) {
    bool isHost = (argc > 1 && std::string(argv[1]) == "host");
    std::cout << "Starting SteamNetworkingSockets Test as " << (isHost ? "HOST" : "CLIENT") << std::endl;

    if (!SteamAPI_Init()) {
        std::cerr << "SteamAPI_Init failed!" << std::endl;
        return 1;
    }
    std::cout << "SteamAPI initialized." << std::endl;

    ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
    if (!sockets) {
        std::cerr << "Failed to get ISteamNetworkingSockets!" << std::endl;
        return 1;
    }

    HSteamNetConnection conn = 0;
    if (isHost) {
        std::cout << "Waiting for connections... (Mocked for testing, host creates listen socket)" << std::endl;
        sockets->CreateListenSocketP2P(0, 0, nullptr);
    } else {
        std::cout << "Client attempting to connect to Host..." << std::endl;
        SteamNetworkingIdentity hostId;
        hostId.SetSteamID64(0x0110000100000000ULL); // Replace with real host ID if known
        conn = sockets->ConnectP2P(hostId, 0, 0, nullptr);
        std::cout << "ConnectP2P Handle: " << conn << std::endl;
    }

    // Run callbacks loop
    for (int i=0; i<50; i++) {
        SteamAPI_RunCallbacks();
        
        if (isHost) {
            // Assume we got a connection from some magic callback or we check all conns
            // Not doing full callback dispatch here since it requires complex CCallback implementation
        } else {
            if (conn) {
                SteamNetConnectionInfo_t info;
                if (sockets->GetConnectionInfo(conn, &info)) {
                    if (info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
                        std::cout << "Client Connected!" << std::endl;
                        const char* msg = "Hello from Client!";
                        sockets->SendMessageToConnection(conn, msg, strlen(msg)+1, k_nSteamNetworkingSend_Reliable, nullptr);
                        break;
                    }
                }
            }
        }
        Sleep(100);
    }

    SteamAPI_Shutdown();
    return 0;
}
