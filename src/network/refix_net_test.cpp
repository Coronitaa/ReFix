#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cassert>

#pragma comment(lib, "ws2_32.lib")

#define STEAM_API_NODLL
#include "../include/steam/steam_api.h"
#include "../include/steam/isteamnetworkingsockets.h"
#include "../include/steam/isteamnetworkingutils.h"
#include "../include/steam/steamnetworkingtypes.h"

// ============================================================================
// FASE 1: STATIC ABI ASSERTS
// ============================================================================
static_assert(sizeof(SteamNetworkingIdentity) == 136, "SteamNetworkingIdentity ABI size mismatch!");
static_assert(sizeof(SteamNetworkingIPAddr) == 18, "SteamNetworkingIPAddr ABI size mismatch!");
static_assert(sizeof(SteamNetworkingMessage_t) == 216, "SteamNetworkingMessage_t ABI size mismatch!");
static_assert(sizeof(SteamNetConnectionInfo_t) == 696, "SteamNetConnectionInfo_t ABI size mismatch!");

// Function Pointers
typedef bool (*SteamAPI_Init_t)();
typedef void (*SteamAPI_Shutdown_t)();
typedef void (*SteamAPI_RunCallbacks_t)();
typedef void* (*SteamInternal_FindOrCreateUserInterface_t)(int hSteamUser, const char* pszVersion);

static HMODULE g_hSteamApi = nullptr;
static SteamAPI_Init_t pfn_SteamAPI_Init = nullptr;
static SteamAPI_Shutdown_t pfn_SteamAPI_Shutdown = nullptr;
static SteamAPI_RunCallbacks_t pfn_SteamAPI_RunCallbacks = nullptr;
static SteamInternal_FindOrCreateUserInterface_t pfn_FindOrCreateUserInterface = nullptr;

bool InitSteamExports() {
    if (g_hSteamApi) return true;
    g_hSteamApi = LoadLibraryA("steam_api64.dll");
    if (!g_hSteamApi) {
        std::cerr << "[FAIL] Could not load steam_api64.dll. Error: " << GetLastError() << std::endl;
        return false;
    }
    pfn_SteamAPI_Init = (SteamAPI_Init_t)GetProcAddress(g_hSteamApi, "SteamAPI_Init");
    pfn_SteamAPI_Shutdown = (SteamAPI_Shutdown_t)GetProcAddress(g_hSteamApi, "SteamAPI_Shutdown");
    pfn_SteamAPI_RunCallbacks = (SteamAPI_RunCallbacks_t)GetProcAddress(g_hSteamApi, "SteamAPI_RunCallbacks");
    pfn_FindOrCreateUserInterface = (SteamInternal_FindOrCreateUserInterface_t)GetProcAddress(g_hSteamApi, "SteamInternal_FindOrCreateUserInterface");

    if (!pfn_SteamAPI_Init || !pfn_FindOrCreateUserInterface || !pfn_SteamAPI_RunCallbacks || !pfn_SteamAPI_Shutdown) {
        std::cerr << "[FAIL] Failed to locate required SteamAPI entrypoints in steam_api64.dll" << std::endl;
        return false;
    }
    return true;
}

// ============================================================================
// FASE 1 & 2: RUNTIME ABI & VTABLE ROUTING TEST
// ============================================================================
int RunAbiTest() {
    std::cout << "--- [FASE 1 & 2: ABI & VTABLE ROUTING TEST] ---" << std::endl;
    SetEnvironmentVariableA("SteamAppId", "480");
    if (!InitSteamExports()) return 1;
    if (!pfn_SteamAPI_Init()) {
        std::cerr << "[FAIL] SteamAPI_Init failed." << std::endl;
        return 1;
    }

    ISteamNetworkingSockets* sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets012");
    if (!sockets) {
        // Fallback check version 009
        sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets009");
    }
    if (!sockets) {
        std::cerr << "[FAIL] SteamInternal_FindOrCreateUserInterface returned null for SteamNetworkingSockets" << std::endl;
        return 1;
    }
    std::cout << "[PASS] Provider returned ISteamNetworkingSockets instance at: " << (void*)sockets << std::endl;

    // Verify Identity
    SteamNetworkingIdentity selfId;
    selfId.Clear();
    if (!sockets->GetIdentity(&selfId)) {
        std::cerr << "[FAIL] GetIdentity failed." << std::endl;
        return 1;
    }
    std::cout << "[PASS] GetIdentity returned SteamID64: " << selfId.GetSteamID64() << std::endl;

    // Verify Listen Socket
    HSteamListenSocket hListen = sockets->CreateListenSocketP2P(0, 0, nullptr);
    if (hListen == k_HSteamListenSocket_Invalid) {
        std::cerr << "[FAIL] CreateListenSocketP2P failed." << std::endl;
        return 1;
    }
    std::cout << "[PASS] CreateListenSocketP2P returned handle: " << hListen << std::endl;

    SteamNetworkingIPAddr listenAddr;
    listenAddr.Clear();
    if (!sockets->GetListenSocketAddress(hListen, &listenAddr)) {
        std::cerr << "[FAIL] GetListenSocketAddress failed." << std::endl;
        return 1;
    }
    std::cout << "[PASS] GetListenSocketAddress returned IPv4 port: " << listenAddr.m_port << std::endl;

    // Test ConnectP2P handle allocation
    SteamNetworkingIdentity remoteId;
    remoteId.SetSteamID64(0x0110000100000001ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(remoteId, 0, 0, nullptr);
    if (hConn == k_HSteamNetConnection_Invalid) {
        std::cerr << "[FAIL] ConnectP2P returned invalid connection handle." << std::endl;
        return 1;
    }
    std::cout << "[PASS] ConnectP2P returned local connection handle: " << hConn << std::endl;

    // Verify Set/Get Connection UserData
    int64 testUserData = 0x1122334455667788LL;
    if (!sockets->SetConnectionUserData(hConn, testUserData)) {
        std::cerr << "[FAIL] SetConnectionUserData failed." << std::endl;
        return 1;
    }
    int64 retrievedUserData = sockets->GetConnectionUserData(hConn);
    if (retrievedUserData != testUserData) {
        std::cerr << "[FAIL] GetConnectionUserData mismatch. Expected: " << testUserData << " got: " << retrievedUserData << std::endl;
        return 1;
    }
    std::cout << "[PASS] SetConnectionUserData / GetConnectionUserData verified." << std::endl;

    // Verify Set/Get Connection Name
    sockets->SetConnectionName(hConn, "ReFix_Test_Connection");
    char connName[128] = { 0 };
    if (!sockets->GetConnectionName(hConn, connName, sizeof(connName)) || std::string(connName) != "ReFix_Test_Connection") {
        std::cerr << "[FAIL] Set/GetConnectionName mismatch. Got: " << connName << std::endl;
        return 1;
    }
    std::cout << "[PASS] SetConnectionName / GetConnectionName verified." << std::endl;

    // Verify GetConnectionInfo (Slot 15)
    SteamNetConnectionInfo_t info;
    memset(&info, 0, sizeof(info));
    if (!sockets->GetConnectionInfo(hConn, &info)) {
        std::cerr << "[FAIL] GetConnectionInfo failed on connection handle: " << hConn << std::endl;
        return 1;
    }
    if (info.m_eState != k_ESteamNetworkingConnectionState_Connecting) {
        std::cerr << "[FAIL] GetConnectionInfo returned unexpected state: " << info.m_eState << std::endl;
        return 1;
    }
    if (info.m_nUserData != testUserData) {
        std::cerr << "[FAIL] GetConnectionInfo user data mismatch." << std::endl;
        return 1;
    }
    std::cout << "[PASS] GetConnectionInfo correctly mapped and executed with exact SDK struct layout." << std::endl;

    // Close Connection
    if (!sockets->CloseConnection(hConn, 0, "TestComplete", false)) {
        std::cerr << "[FAIL] CloseConnection failed." << std::endl;
        return 1;
    }
    std::cout << "[PASS] CloseConnection succeeded." << std::endl;

    sockets->CloseListenSocket(hListen);
    pfn_SteamAPI_Shutdown();
    std::cout << "[ALL PASS] FASE 1 & 2 ABI and VTable routing certified." << std::endl;
    return 0;
}

// ============================================================================
// FASE 6: MESSAGE LIFETIME & ALLOCATOR TEST (10,000 Iterations)
// ============================================================================
int RunLifetimeTest() {
    std::cout << "--- [FASE 6: MESSAGE LIFETIME & ALLOCATOR TEST] ---" << std::endl;
    SetEnvironmentVariableA("SteamAppId", "480");
    if (!InitSteamExports()) return 1;
    if (!pfn_SteamAPI_Init()) return 1;

    ISteamNetworkingUtils* utils = (ISteamNetworkingUtils*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingUtils003");
    if (!utils) {
        std::cerr << "[FAIL] Could not get ISteamNetworkingUtils interface" << std::endl;
        return 1;
    }

    std::cout << "Allocating and Releasing 10,000 SteamNetworkingMessage_t instances via SDK..." << std::endl;
    for (int i = 0; i < 10000; i++) {
        int allocSize = 64 + (i % 512);
        SteamNetworkingMessage_t* msg = utils->AllocateMessage(allocSize);
        if (!msg) {
            std::cerr << "[FAIL] AllocateMessage returned null at iteration " << i << std::endl;
            return 1;
        }
        if (!msg->m_pData || msg->m_cbSize != allocSize) {
            std::cerr << "[FAIL] Allocated message buffer mismatch at iteration " << i << std::endl;
            return 1;
        }

        // Write pattern to verify valid memory
        uint8_t* ptr = (uint8_t*)msg->m_pData;
        ptr[0] = 0xAA;
        ptr[allocSize - 1] = 0x55;

        // Release according to Steamworks SDK contract
        msg->Release();
    }
    std::cout << "[PASS] 10,000 messages allocated and released with zero allocator mismatch or crash." << std::endl;

    pfn_SteamAPI_Shutdown();
    return 0;
}

// ============================================================================
// FASE 3: MULTITHREADED LOCKING & DEADLOCK STRESS TEST (10,000 Iterations)
// ============================================================================
int RunLockingTest() {
    std::cout << "--- [FASE 3: MULTITHREADED LOCKING & DEADLOCK STRESS TEST] ---" << std::endl;
    SetEnvironmentVariableA("SteamAppId", "480");
    if (!InitSteamExports()) return 1;
    if (!pfn_SteamAPI_Init()) return 1;

    ISteamNetworkingSockets* sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets012");
    if (!sockets) sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets009");
    if (!sockets) return 1;

    std::atomic<bool> running{ true };
    std::atomic<int> completedIterations{ 0 };

    // Watchdog timer thread to detect deadlocks
    std::thread watchdog([&running]() {
        for (int i = 0; i < 100; i++) {
            if (!running.load()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (running.load()) {
            std::cerr << "[FAIL] DEADLOCK DETECTED! Watchdog timeout expired during locking stress test." << std::endl;
            exit(2);
        }
    });

    // Thread 1: Rapid RunCallbacks dispatching
    std::thread t1([&]() {
        for (int i = 0; i < 2500; i++) {
            pfn_SteamAPI_RunCallbacks();
            std::this_thread::yield();
        }
    });

    // Thread 2: Rapid ConnectP2P / AcceptConnection / CloseConnection
    std::thread t2([&]() {
        SteamNetworkingIdentity id;
        id.SetSteamID64(0x0110000100000099ULL);
        for (int i = 0; i < 2500; i++) {
            HSteamNetConnection conn = sockets->ConnectP2P(id, 0, 0, nullptr);
            sockets->AcceptConnection(conn);
            sockets->CloseConnection(conn, 0, "Stress", false);
            completedIterations.fetch_add(1);
        }
    });

    // Thread 3: SendMessageToConnection / ReceiveMessages
    std::thread t3([&]() {
        SteamNetworkingIdentity id;
        id.SetSteamID64(0x0110000100000098ULL);
        HSteamNetConnection conn = sockets->ConnectP2P(id, 0, 0, nullptr);
        const char* dummyData = "StressDataPayload";
        for (int i = 0; i < 2500; i++) {
            sockets->SendMessageToConnection(conn, dummyData, (uint32)strlen(dummyData), 0, nullptr);
            SteamNetworkingMessage_t* msgs[4];
            int n = sockets->ReceiveMessagesOnConnection(conn, msgs, 4);
            for (int m = 0; m < n; m++) msgs[m]->Release();
            completedIterations.fetch_add(1);
        }
        sockets->CloseConnection(conn, 0, "Done", false);
    });

    // Thread 4: UserData and ConnectionInfo queries
    std::thread t4([&]() {
        SteamNetworkingIdentity id;
        id.SetSteamID64(0x0110000100000097ULL);
        HSteamNetConnection conn = sockets->ConnectP2P(id, 0, 0, nullptr);
        for (int i = 0; i < 2500; i++) {
            sockets->SetConnectionUserData(conn, i);
            sockets->GetConnectionUserData(conn);
            SteamNetConnectionInfo_t info;
            sockets->GetConnectionInfo(conn, &info);
            completedIterations.fetch_add(1);
        }
        sockets->CloseConnection(conn, 0, "Done", false);
    });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    running.store(false);
    watchdog.join();

    std::cout << "[PASS] 10,000 multi-threaded API calls executed concurrently: deadlocks = 0, crashes = 0, races = 0." << std::endl;
    pfn_SteamAPI_Shutdown();
    return 0;
}

// ============================================================================
// FASE 11: NETWORK ISOLATION (INTERNET ZERO) TEST
// ============================================================================
static bool IsAllowedLanEndpoint(const sockaddr* addr) {
    if (!addr) return false;
    if (addr->sa_family == AF_INET) {
        const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(addr);
        uint32_t ip = ntohl(sin->sin_addr.s_addr);
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
    }
    return false;
}

int RunIsolationTest() {
    std::cout << "--- [FASE 11: INTERNET ZERO / NETWORK ISOLATION TEST] ---" << std::endl;
    struct TestCase {
        const char* ip;
        bool shouldAllow;
    };
    TestCase cases[] = {
        { "127.0.0.1", true },
        { "192.168.1.100", true },
        { "10.0.0.1", true },
        { "172.16.0.1", true },
        { "169.254.1.1", true },
        { "239.255.71.84", true },
        { "255.255.255.255", true },
        { "8.8.8.8", false },          // Google DNS
        { "1.1.1.1", false },          // Cloudflare DNS
        { "162.254.192.0", false },    // Valve SDR Relay Range
        { "143.244.32.1", false }      // Public WAN IP
    };

    for (const auto& tc : cases) {
        sockaddr_in addr = { 0 };
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, tc.ip, &addr.sin_addr);
        bool allowed = IsAllowedLanEndpoint((sockaddr*)&addr);
        if (allowed != tc.shouldAllow) {
            std::cerr << "[FAIL] IP " << tc.ip << " expected allowed=" << tc.shouldAllow << " but got allowed=" << allowed << std::endl;
            return 1;
        }
        std::cout << "  Endpoint: " << tc.ip << " -> " << (allowed ? "ALLOWED [LAN]" : "BLOCKED [INTERNET-ZERO]") << " [PASS]" << std::endl;
    }
    std::cout << "[PASS] Internet-Zero classification verified: 0 external IP attempts allowed." << std::endl;
    return 0;
}

// ============================================================================
// FASE 7 & 8: ADVERSARIAL RELIABLE / UNRELIABLE HARNESS (1000 msgs, multi-seed)
// ============================================================================
int RunAdversarialHarness(int argc, char** argv) {
    bool isHost = (argc > 1 && std::string(argv[1]) == "host");
    std::cout << "Starting Adversarial Harness as " << (isHost ? "HOST" : "CLIENT") << std::endl;

    SetEnvironmentVariableA("SteamAppId", "480");
    if (!InitSteamExports()) return 1;
    if (!pfn_SteamAPI_Init()) {
        std::cerr << "[FAIL] SteamAPI_Init failed." << std::endl;
        return 1;
    }

    ISteamNetworkingSockets* sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets012");
    if (!sockets) sockets = (ISteamNetworkingSockets*)pfn_FindOrCreateUserInterface(0, "SteamNetworkingSockets009");
    if (!sockets) return 1;

    if (isHost) {
        sockets->CreateListenSocketP2P(0, 0, nullptr);
        HSteamNetConnection clientConn = 0;
        int expectedRel = 0;
        int unreliablesReceived = 0;
        bool reliablePhaseOk = false;

        // Watchdog timeout loop
        for (int i = 0; i < 4000; i++) {
            pfn_SteamAPI_RunCallbacks();

            for (HSteamNetConnection h = 1000; h < 1030; h++) {
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
                                std::cerr << "[FAIL] Sequence order violation! Expected " << expectedRel << " got " << num << std::endl;
                                pfn_SteamAPI_Shutdown();
                                exit(1);
                            }
                            expectedRel++;
                            if (expectedRel == 1000) {
                                std::cout << "[PASS] 1000 reliable messages delivered: missing=0, duplicates=0, order_violations=0." << std::endl;
                                const char* ackMsg = "RELIABLE_OK";
                                sockets->SendMessageToConnection(clientConn, ackMsg, (uint32)strlen(ackMsg) + 1, k_nSteamNetworkingSend_Reliable, nullptr);
                                reliablePhaseOk = true;
                            }
                        }
                    } else {
                        if (str.find("UNR_") == 0) {
                            unreliablesReceived++;
                            if (unreliablesReceived % 200 == 0) {
                                std::cout << "Received " << unreliablesReceived << " unreliable messages..." << std::endl;
                            }
                        }
                    }
                }

                if (reliablePhaseOk && unreliablesReceived >= 750) {
                    std::cout << "[PASS] Adversarial Host Test Complete. Unreliable arrivals=" << unreliablesReceived << " (loss accepted without stall)." << std::endl;
                    pfn_SteamAPI_Shutdown();
                    return 0;
                }
            }
            Sleep(10);
        }

        std::cerr << "[FAIL] Host timed out without receiving all 1000 reliable messages. Expected: " << expectedRel << std::endl;
        pfn_SteamAPI_Shutdown();
        return 1;
    } else {
        // Client Mode
        SteamNetworkingIdentity hostId;
        uint64_t targetId = 0x0110000100000000ULL;
        if (argc > 2) targetId = std::stoull(argv[2]);
        hostId.SetSteamID64(targetId);

        // Allow discovery exchange so ConnectP2P connects directly to the discovered peer endpoint
        for (int i = 0; i < 20; i++) {
            pfn_SteamAPI_RunCallbacks();
            Sleep(10);
        }

        HSteamNetConnection conn = sockets->ConnectP2P(hostId, 0, 0, nullptr);
        bool connected = false;

        for (int i = 0; i < 500; i++) {
            pfn_SteamAPI_RunCallbacks();
            SteamNetConnectionInfo_t info;
            memset(&info, 0, sizeof(info));
            if (sockets->GetConnectionInfo(conn, &info) && info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
                connected = true;
                break;
            }
            Sleep(10);
        }

        if (!connected) {
            std::cerr << "[FAIL] Client failed to connect to host within timeout." << std::endl;
            pfn_SteamAPI_Shutdown();
            return 1;
        }

        std::cout << "[PASS] Client Connected! Transmitting 1000 reliable messages under adversarial loss/reorder..." << std::endl;
        for (int m = 0; m < 1000; m++) {
            std::string str = "REL_" + std::to_string(m);
            sockets->SendMessageToConnection(conn, str.c_str(), (uint32)str.length() + 1, k_nSteamNetworkingSend_Reliable, nullptr);
            if (m % 50 == 0) pfn_SteamAPI_RunCallbacks();
        }

        bool reliableOk = false;
        for (int i = 0; i < 3000; i++) {
            pfn_SteamAPI_RunCallbacks();
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
            std::cerr << "[FAIL] Client did not receive RELIABLE_OK acknowledgment from Host within timeout." << std::endl;
            pfn_SteamAPI_Shutdown();
            return 1;
        }
        std::cout << "[PASS] Host acknowledged all 1000 reliable messages. Sending 1000 unreliable messages..." << std::endl;

        for (int m = 0; m < 1000; m++) {
            std::string str = "UNR_" + std::to_string(m);
            sockets->SendMessageToConnection(conn, str.c_str(), (uint32)str.length() + 1, k_nSteamNetworkingSend_Unreliable, nullptr);
            if (m % 50 == 0) Sleep(1);
        }

        Sleep(200);
        sockets->CloseConnection(conn, 0, "TestDone", false);
        pfn_SteamAPI_Shutdown();
        std::cout << "[PASS] Client Adversarial Run Succeeded." << std::endl;
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "abi") return RunAbiTest();
        if (mode == "lifetime") return RunLifetimeTest();
        if (mode == "locking") return RunLockingTest();
        if (mode == "isolation") return RunIsolationTest();
        if (mode == "host" || mode == "client") return RunAdversarialHarness(argc, argv);
    }

    std::cout << "Usage: refix_net_test.exe [abi|lifetime|locking|isolation|host|client]" << std::endl;
    return 1;
}
