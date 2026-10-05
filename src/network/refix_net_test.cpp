#include <iostream>
#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <set>

#define STEAM_API_NODLL
#include "../include/steam/steam_api.h"
#include "../include/steam/isteamnetworkingsockets.h"
#include "../include/steam/steamnetworkingtypes.h"

// Function Pointers
typedef bool (*SteamAPI_Init_t)();
typedef void (*SteamAPI_Shutdown_t)();
typedef void (*SteamAPI_RunCallbacks_t)();

int main(int argc, char** argv) {
    HMODULE hSteamApi = LoadLibraryA("steam_api64.dll");
    if (!hSteamApi) return 1;

    auto SteamAPI_Init = (SteamAPI_Init_t)GetProcAddress(hSteamApi, "SteamAPI_Init");
    auto SteamAPI_Shutdown = (SteamAPI_Shutdown_t)GetProcAddress(hSteamApi, "SteamAPI_Shutdown");
    auto SteamAPI_RunCallbacks = (SteamAPI_RunCallbacks_t)GetProcAddress(hSteamApi, "SteamAPI_RunCallbacks");
    typedef void* (*SteamInternal_FindOrCreateUserInterface_t)(int hSteamUser, const char* pszVersion);
    auto SteamInternal_FindOrCreateUserInterface = (SteamInternal_FindOrCreateUserInterface_t)GetProcAddress(hSteamApi, "SteamInternal_FindOrCreateUserInterface");

    SetEnvironmentVariableA("SteamAppId", "480");
    if (!SteamAPI_Init()) return 1;
    
    std::cout << "[PASS] SteamAPI initialized." << std::endl;

    ISteamNetworkingSockets* sockets = (ISteamNetworkingSockets*)SteamInternal_FindOrCreateUserInterface(0, "SteamNetworkingSockets009");
    if (!sockets) return 1;

    bool isHost = (argc > 1 && std::string(argv[1]) == "host");
    std::cout << "Starting Test as " << (isHost ? "HOST" : "CLIENT") << std::endl;

    if (isHost) {
        sockets->CreateListenSocketP2P(0, 0, nullptr);
        HSteamNetConnection clientConn = 0;

        int expectedRel = 0;
        int unreliablesReceived = 0;
        bool reliablePhaseOk = false;

        for (int i = 0; i < 5000; i++) {
            SteamAPI_RunCallbacks();
            
            for (HSteamNetConnection h = 1000; h < 1020; h++) {
                SteamNetConnectionInfo_t info;
                memset(&info, 0, sizeof(info));
                if (sockets->GetConnectionInfo(h, &info)) {
                    if (info.m_eState == k_ESteamNetworkingConnectionState_Connecting) {
                        sockets->AcceptConnection(h);
                        clientConn = h;
                    } else if (info.m_eState == k_ESteamNetworkingConnectionState_Connected && clientConn == 0) {
                        clientConn = h;
                    }
                }
            }

            if (clientConn != 0) {
                SteamNetworkingMessage_t* msgs[16];
                int n = sockets->ReceiveMessagesOnConnection(clientConn, msgs, 16);
                for (int m = 0; m < n; m++) {
                    std::string str((const char*)msgs[m]->m_pData);
                    msgs[m]->Release();
                    
                    if (!reliablePhaseOk) {
                        if (str.find("REL_") == 0) {
                            int num = std::stoi(str.substr(4));
                            if (num != expectedRel) {
                                std::cerr << "[FAIL] Reliable out of order! Expected " << expectedRel << " got " << num << std::endl;
                                exit(1);
                            }
                            expectedRel++;
                            if (expectedRel == 1000) {
                                std::cout << "[PASS] Received 1000 reliable messages perfectly in order!" << std::endl;
                                const char* reply = "RELIABLE_OK";
                                sockets->SendMessageToConnection(clientConn, reply, strlen(reply) + 1, 8, nullptr);
                                reliablePhaseOk = true;
                            }
                        }
                    } else {
                        if (str.find("UNR_") == 0) {
                            unreliablesReceived++;
                            if (unreliablesReceived % 100 == 0) {
                                std::cout << "Received " << unreliablesReceived << " unreliable messages..." << std::endl;
                            }
                        }
                    }
                }
                
                if (unreliablesReceived > 0 && i % 10 == 0) {
                    // Host may eventually time out after client is done
                    if (unreliablesReceived >= 800) { // Expect some loss
                        std::cout << "[PASS] Test Complete." << std::endl;
                        SteamAPI_Shutdown();
                        return 0;
                    }
                }
            }
            Sleep(10);
        }
    } else {
        SteamNetworkingIdentity hostId;
        uint64_t targetId = 0x0110000100000000ULL;
        if (argc > 2) targetId = std::stoull(argv[2]);
        hostId.SetSteamID64(targetId);
        
        HSteamNetConnection conn = sockets->ConnectP2P(hostId, 0, 0, nullptr);
        bool connected = false;

        for (int i = 0; i < 500; i++) {
            SteamAPI_RunCallbacks();
            SteamNetConnectionInfo_t info;
            memset(&info, 0, sizeof(info));
            if (sockets->GetConnectionInfo(conn, &info) && info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
                connected = true;
                break;
            }
            Sleep(10);
        }

        if (!connected) return 1;
        std::cout << "[PASS] Client Connected! Sending 1000 reliable messages..." << std::endl;

        for (int m = 0; m < 1000; m++) {
            std::string str = "REL_" + std::to_string(m);
            sockets->SendMessageToConnection(conn, str.c_str(), str.length() + 1, 8 /* k_nSteamNetworkingSend_Reliable */, nullptr);
        }

        bool reliableOk = false;
        for (int i = 0; i < 3000; i++) {
            SteamAPI_RunCallbacks();
            SteamNetworkingMessage_t* msg = nullptr;
            if (sockets->ReceiveMessagesOnConnection(conn, &msg, 1) > 0) {
                std::string rep((const char*)msg->m_pData);
                msg->Release();
                if (rep == "RELIABLE_OK") {
                    reliableOk = true;
                    break;
                }
            }
            Sleep(10);
        }

        if (!reliableOk) {
            std::cerr << "[FAIL] Did not get RELIABLE_OK" << std::endl;
            return 1;
        }
        std::cout << "[PASS] Host acknowledged reliable messages. Sending 1000 unreliable..." << std::endl;

        for (int m = 0; m < 1000; m++) {
            std::string str = "UNR_" + std::to_string(m);
            sockets->SendMessageToConnection(conn, str.c_str(), str.length() + 1, 0 /* k_nSteamNetworkingSend_Unreliable */, nullptr);
            Sleep(1);
        }
        
        Sleep(500); // Allow time for packets to arrive
        sockets->CloseConnection(conn, 0, nullptr, false);
    }

    SteamAPI_Shutdown();
    return 0;
}
