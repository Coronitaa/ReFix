// tests/test_transport_ownership.cpp
// ReFix LAN 2.0 - Phase 1 H-01 Regression Test: Transport Ownership & Port Conflict Resolution
//
// Verifies:
// 1. Initialized transport selection (Canonical vs LegacyFallback vs Failed)
// 2. Effective ownership of discovery port 47584 by canonical transport
// 3. Absence of WSAEADDRINUSE conflict between the two transports in canonical mode
// 4. Proper reception of canonical datagrams on port 47584
// 5. Zero silent drops or interception by legacy socket in canonical mode
// 6. Clean fallback to legacy transport when canonical initialization fails and fallback is authorized
// 7. Rejection of unmanaged fallback when REFIX_DISABLE_LEGACY_FALLBACKS is set
// 8. Production DLL export table isolation (no test hooks leaked in production)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cassert>

#pragma comment(lib, "ws2_32.lib")

#include "lan_core/refix_lan_types.h"
#include "lan_core/refix_lan_core.h"
#include "lan_core/refix_lan_wire.h"

typedef bool (*fn_SteamAPI_Init)();
typedef void (*fn_SteamAPI_Shutdown)();
typedef void (*fn_SteamAPI_RunCallbacks)();
typedef void* (*fn_ReFix_GetLanCore)();
typedef int (*fn_ReFix_Test_GetTransportMode)();
typedef uint16_t (*fn_ReFix_Test_GetEffectiveDiscoveryPort)();
typedef bool (*fn_ReFix_Test_IsLegacySocketInitialized)();
typedef uint64_t (*fn_ReFix_Test_GetBroadcastNetPacketCount)();
typedef uint64_t (*fn_ReFix_Test_GetLegacySendToCount)();
typedef uint64_t (*fn_ReFix_Test_GetLegacyRecvFromCount)();
typedef void (*fn_ReFix_Test_ResetLegacyCounters)();

#pragma pack(push, 1)
struct WireHeaderTest {
    uint32_t magic;          // 0x52464958 'RFIX'
    uint8_t  version;        // 2
    uint8_t  msgType;        // 0x01 = DiscoveryBeacon
    uint16_t flags;          // 0
    uint64_t senderPeerHigh; // machineId
    uint64_t senderPeerLow;  // instanceId
    uint32_t generationId;   // 1
    uint32_t fragmentMsgId;  // 0
    uint8_t  channel;        // 0
    uint8_t  fragIndex;      // 0
    uint8_t  fragTotal;      // 1
    uint16_t payloadLen;     // byte count
};

struct NetPacketHeaderTest {
    uint32_t magic;          // 0x52464958 'RFIX'
    uint8_t  msgType;        // 1 = Ping
    uint64_t senderID;       // steamID
    uint32_t appID;          // appID
};
#pragma pack(pop)

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << (msg) << " (" #cond ") at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

static HMODULE LoadTestDll(const std::string& customPath) {
    std::vector<std::string> candidates;
    if (!customPath.empty()) candidates.push_back(customPath);
    candidates.push_back("build\\verify_bin\\steam_api64_test.dll");
    candidates.push_back("bin\\steam_api64_test.dll");
    candidates.push_back("build\\steam_api64_test.dll");
    candidates.push_back("steam_api64_test.dll");

    for (const auto& path : candidates) {
        HMODULE h = LoadLibraryA(path.c_str());
        if (h) {
            std::cout << "[*] Loaded test DLL from: " << path << std::endl;
            return h;
        }
    }
    return nullptr;
}

static HMODULE LoadProdDll(const std::string& customPath) {
    std::vector<std::string> candidates;
    if (!customPath.empty()) {
        std::string p = customPath;
        size_t pos = p.find("_test.dll");
        if (pos != std::string::npos) {
            p.replace(pos, 9, ".dll");
        }
        candidates.push_back(p);
    }
    candidates.push_back("build\\verify_bin\\steam_api64.dll");
    candidates.push_back("bin\\steam_api64.dll");
    candidates.push_back("build\\steam_api64.dll");
    candidates.push_back("steam_api64.dll");

    for (const auto& path : candidates) {
        HMODULE h = LoadLibraryA(path.c_str());
        if (h) {
            std::cout << "[*] Loaded production DLL from: " << path << std::endl;
            return h;
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    std::string customDllPath = "";
    if (argc > 1) {
        customDllPath = argv[1];
    }

    // Set LAN environment variables required for autonomous Steamworks emulation
    SetEnvironmentVariableA("REFIX_NETWORK_MODE", "LAN");
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", "OwnershipTester");
    SetEnvironmentVariableA("REFIX_STEAM_ID", "76561198000000099");
    SetEnvironmentVariableA("REFIX_LISTEN_PORT", "47584");
    SetEnvironmentVariableA("SteamAppId", "480");

    std::cout << "==========================================================" << std::endl;
    std::cout << "   REFIX H-01 REGRESSION TEST: TRANSPORT OWNERSHIP        " << std::endl;
    std::cout << "==========================================================" << std::endl;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    HMODULE hTest = LoadTestDll(customDllPath);
    if (!hTest) {
        std::cerr << "[FAIL] Could not load steam_api64_test.dll" << std::endl;
        WSACleanup();
        return 1;
    }

    auto pfnInit = (fn_SteamAPI_Init)GetProcAddress(hTest, "SteamAPI_Init");
    auto pfnShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hTest, "SteamAPI_Shutdown");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hTest, "SteamAPI_RunCallbacks");
    auto pfnGetLanCore = (fn_ReFix_GetLanCore)GetProcAddress(hTest, "ReFix_GetLanCore");
    auto pfnGetMode = (fn_ReFix_Test_GetTransportMode)GetProcAddress(hTest, "ReFix_Test_GetTransportMode");
    auto pfnGetPort = (fn_ReFix_Test_GetEffectiveDiscoveryPort)GetProcAddress(hTest, "ReFix_Test_GetEffectiveDiscoveryPort");
    auto pfnIsLegacyInit = (fn_ReFix_Test_IsLegacySocketInitialized)GetProcAddress(hTest, "ReFix_Test_IsLegacySocketInitialized");
    auto pfnGetBcast = (fn_ReFix_Test_GetBroadcastNetPacketCount)GetProcAddress(hTest, "ReFix_Test_GetBroadcastNetPacketCount");
    auto pfnGetSendTo = (fn_ReFix_Test_GetLegacySendToCount)GetProcAddress(hTest, "ReFix_Test_GetLegacySendToCount");
    auto pfnGetRecvFrom = (fn_ReFix_Test_GetLegacyRecvFromCount)GetProcAddress(hTest, "ReFix_Test_GetLegacyRecvFromCount");
    auto pfnResetCounters = (fn_ReFix_Test_ResetLegacyCounters)GetProcAddress(hTest, "ReFix_Test_ResetLegacyCounters");

    if (!pfnInit || !pfnShutdown || !pfnRunCallbacks || !pfnGetLanCore || !pfnGetMode || !pfnGetPort ||
        !pfnIsLegacyInit || !pfnGetBcast || !pfnGetSendTo || !pfnGetRecvFrom || !pfnResetCounters) {
        std::cerr << "[FAIL] Missing required test exports in steam_api64_test.dll" << std::endl;
        FreeLibrary(hTest);
        WSACleanup();
        return 1;
    }

    int failures = 0;

    // =========================================================================
    // TEST 1: Canonical Mode Ownership & Zero Legacy Contention (Criteria 1-5)
    // =========================================================================
    std::cout << "\n[TEST 1] Canonical Mode Transport Ownership & Port Verification..." << std::endl;
    {
        SetEnvironmentVariableA("REFIX_FORCE_CANONICAL_FAILURE", nullptr);
        SetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", nullptr);

        pfnResetCounters();
        bool initOk = pfnInit();
        if (!initOk) {
            std::cerr << "  [FAIL] SteamAPI_Init failed in canonical mode!" << std::endl;
            failures++;
        } else {
            int mode = pfnGetMode();
            uint16_t discPort = pfnGetPort();
            bool legacyInit = pfnIsLegacyInit();

            std::cout << "  [*] Transport Mode: " << mode << " (1 = Canonical)" << std::endl;
            std::cout << "  [*] Discovery Port: " << discPort << std::endl;
            std::cout << "  [*] Legacy Socket Initialized: " << (legacyInit ? "TRUE" : "FALSE") << std::endl;

            // Criterion 1: Selected transport initialized
            if (mode != 1) {
                std::cerr << "  [FAIL] Expected TransportMode::Canonical (1), got: " << mode << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 1: Canonical transport successfully selected." << std::endl;
            }

            // Criterion 2 & 3: Port ownership & Absence of WSAEADDRINUSE conflict
            if (discPort != 47584) {
                std::cerr << "  [FAIL] Effective discovery port is " << discPort << ", expected canonical default 47584!" << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 2: Canonical transport owns primary discovery port 47584." << std::endl;
            }

            if (legacyInit) {
                std::cerr << "  [FAIL] Legacy UDP socket was initialized when canonical mode succeeded!" << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 3: Legacy UDP socket is NOT initialized (no WSAEADDRINUSE port collision)." << std::endl;
            }

            // Verify that port 47584 is bound exclusively by trying to bind without reuse
            SOCKET rawProbe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (rawProbe != INVALID_SOCKET) {
                sockaddr_in probeAddr{};
                probeAddr.sin_family = AF_INET;
                probeAddr.sin_addr.s_addr = INADDR_ANY;
                probeAddr.sin_port = htons(47584);
                int bindRes = bind(rawProbe, (sockaddr*)&probeAddr, sizeof(probeAddr));
                int bindErr = (bindRes == SOCKET_ERROR) ? WSAGetLastError() : 0;
                closesocket(rawProbe);

                if (bindErr == WSAEADDRINUSE) {
                    std::cout << "  [PASS] External socket confirmed port 47584 is held exclusively (bind error WSAEADDRINUSE as expected)." << std::endl;
                } else {
                    std::cerr << "  [FAIL] External bind to 47584 did not fail with WSAEADDRINUSE! Res=" << bindRes << " Err=" << bindErr << std::endl;
                    failures++;
                }
            }

            // Criterion 4 & 5: Datagram transmission & Absence of silent legacy drop
            // Observable Consumer Result: Send valid canonical DiscoveryBeacon to port 47584
            // and verify that LanTransport processes it and registers the peer in ILanCore::Peers().
            SOCKET senderSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (senderSock != INVALID_SOCKET) {
                const uint64_t remoteSteamId = 76561198000000888ULL;
                refix::lan::PeerId senderPid{0x99887766ULL, 0x55443322ULL};
                refix::lan::LanEndpoint senderEp{0x7F000001, 54321};
                const std::string senderDisplayName = "ObservableCanonicalPeer";

                refix::lan::ByteWriter w;
                w.WriteString("Steam_480");
                w.WritePeerId(senderPid);
                w.WriteString(senderDisplayName);
                w.WriteEndpoint(senderEp);
                w.WriteU64(remoteSteamId);

                refix::lan::WireHeader wire{};
                memset(&wire, 0, sizeof(wire));
                wire.magic = refix::lan::REFIX_WIRE_MAGIC;
                wire.version = refix::lan::REFIX_WIRE_VERSION;
                wire.msgType = static_cast<uint8_t>(refix::lan::MsgType::DiscoveryBeacon);
                wire.flags = 0;
                wire.senderPeerHigh = senderPid.high;
                wire.senderPeerLow = senderPid.low;
                wire.generationId = 1;
                wire.fragmentMsgId = 0;
                wire.channel = 0;
                wire.fragIndex = 0;
                wire.fragTotal = 1;
                wire.payloadLen = static_cast<uint16_t>(w.Size());

                std::vector<uint8_t> packet(sizeof(wire) + w.Size());
                memcpy(packet.data(), &wire, sizeof(wire));
                memcpy(packet.data() + sizeof(wire), w.Data(), w.Size());

                sockaddr_in dest{};
                dest.sin_family = AF_INET;
                dest.sin_addr.s_addr = htonl(0x7F000001); // 127.0.0.1
                dest.sin_port = htons(47584);

                pfnResetCounters();
                sendto(senderSock, (const char*)packet.data(), (int)packet.size(), 0, (sockaddr*)&dest, sizeof(dest));
                closesocket(senderSock);

                // Run callbacks to tick network and dispatch events
                pfnRunCallbacks();
                Sleep(100);
                pfnRunCallbacks();

                // Observable consumer assertion: Verify discovery registration in ILanCore
                auto* core = static_cast<refix::lan::ILanCore*>(pfnGetLanCore());
                if (!core) {
                    std::cerr << "  [FAIL] ReFix_GetLanCore returned nullptr!" << std::endl;
                    failures++;
                } else {
                    auto peerOpt = core->Peers().FindBySteamId(remoteSteamId);
                    if (peerOpt.has_value()) {
                        auto infoOpt = core->Peers().FindByPeerId(*peerOpt);
                        if (infoOpt.has_value() &&
                            infoOpt->displayName == senderDisplayName &&
                            infoOpt->endpoint.port == senderEp.port &&
                            infoOpt->endpoint.ipv4 == senderEp.ipv4) {
                            std::cout << "  [PASS] Criterion 4 (Observable Consumer Result): Datagram received on port 47584, "
                                      << "processed by LanTransport, and peer successfully registered in ILanCore::Peers() "
                                      << "(SteamID=" << remoteSteamId << ", Name=" << infoOpt->displayName
                                      << ", Endpoint=" << infoOpt->endpoint.ToString() << ")." << std::endl;
                        } else {
                            std::cerr << "  [FAIL] Peer found in ILanCore::Peers() but data mismatch! Name="
                                      << (infoOpt ? infoOpt->displayName : "none")
                                      << " Port=" << (infoOpt ? infoOpt->endpoint.port : 0) << std::endl;
                            failures++;
                        }
                    } else {
                        std::cerr << "  [FAIL] Canonical datagram was NOT processed into ILanCore::Peers()! FindBySteamId("
                                  << remoteSteamId << ") returned nullopt." << std::endl;
                        failures++;
                    }
                }

                uint64_t legacyRecv = pfnGetRecvFrom();
                uint64_t legacySend = pfnGetSendTo();
                uint64_t bcastCount = pfnGetBcast();

                if (legacyRecv == 0) {
                    std::cout << "  [PASS] Criterion 5: Zero legacy interception (legacyRecvFromCount=0). Canonical wire datagram bypassed legacy completely." << std::endl;
                } else {
                    std::cerr << "  [FAIL] Legacy socket intercepted " << legacyRecv << " packets! Silent drop regression detected." << std::endl;
                    failures++;
                }

                if (legacySend == 0 && bcastCount == 0) {
                    std::cout << "  [PASS] Zero spurious legacy transmissions in canonical mode (legacySendToCount=0, bcastCount=0)." << std::endl;
                } else {
                    std::cerr << "  [FAIL] Spurious legacy transmissions detected: sendto=" << legacySend << " bcast=" << bcastCount << std::endl;
                    failures++;
                }
            }

            pfnShutdown();
            int shutdownMode = pfnGetMode();
            if (shutdownMode == 0) {
                std::cout << "  [PASS] Transport shutdown verified (mode returned to Uninitialized=0)." << std::endl;
            } else {
                std::cerr << "  [FAIL] Shutdown did not reset transport mode! Current=" << shutdownMode << std::endl;
                failures++;
            }
        }
    }

    // =========================================================================
    // TEST 2: Fallback to Legacy when Canonical Fails (Criterion 6)
    // =========================================================================
    std::cout << "\n[TEST 2] Authorized Fallback to Legacy UDP Transport on Canonical Failure..." << std::endl;
    {
        SetEnvironmentVariableA("REFIX_FORCE_CANONICAL_FAILURE", "1");
        SetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", "0");

        pfnResetCounters();
        bool initOk = pfnInit();
        if (!initOk) {
            std::cerr << "  [FAIL] SteamAPI_Init failed when fallback was authorized!" << std::endl;
            failures++;
        } else {
            int mode = pfnGetMode();
            uint16_t discPort = pfnGetPort();
            bool legacyInit = pfnIsLegacyInit();

            std::cout << "  [*] Transport Mode: " << mode << " (2 = LegacyFallback)" << std::endl;
            std::cout << "  [*] Discovery Port: " << discPort << std::endl;
            std::cout << "  [*] Legacy Socket Initialized: " << (legacyInit ? "TRUE" : "FALSE") << std::endl;

            if (mode != 2) {
                std::cerr << "  [FAIL] Expected TransportMode::LegacyFallback (2), got: " << mode << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 6a: Fallback transport mode properly engaged." << std::endl;
            }

            if (!legacyInit) {
                std::cerr << "  [FAIL] Legacy socket was NOT initialized in fallback mode!" << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 6b: Legacy UDP socket successfully activated on fallback." << std::endl;
            }

            if (discPort != 47584) {
                std::cerr << "  [FAIL] Fallback socket did not bind expected port 47584! Port=" << discPort << std::endl;
                failures++;
            } else {
                std::cout << "  [PASS] Criterion 6c: Legacy socket claims port 47584 cleanly without conflict." << std::endl;
            }

            // Verify legacy packet processing works in fallback mode
            SOCKET rawSender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (rawSender != INVALID_SOCKET) {
                NetPacketHeaderTest legacyHdr{};
                legacyHdr.magic = 0x52464958;
                legacyHdr.msgType = 1; // Ping
                legacyHdr.senderID = 76561198000000099ULL;
                legacyHdr.appID = 480;

                std::string pingData = "FallbackTester";
                std::vector<uint8_t> pkt(sizeof(legacyHdr) + pingData.size());
                memcpy(pkt.data(), &legacyHdr, sizeof(legacyHdr));
                memcpy(pkt.data() + sizeof(legacyHdr), pingData.data(), pingData.size());

                sockaddr_in dest{};
                dest.sin_family = AF_INET;
                dest.sin_addr.s_addr = htonl(0x7F000001);
                dest.sin_port = htons(47584);

                pfnResetCounters();
                sendto(rawSender, (const char*)pkt.data(), (int)pkt.size(), 0, (sockaddr*)&dest, sizeof(dest));
                closesocket(rawSender);

                Sleep(50);
                pfnRunCallbacks();

                uint64_t recvCount = pfnGetRecvFrom();
                if (recvCount > 0) {
                    std::cout << "  [PASS] Criterion 6d: Legacy UDP transport processed packet in fallback mode (recvfrom=" << recvCount << ")." << std::endl;
                } else {
                    std::cerr << "  [FAIL] Legacy transport failed to receive packet in fallback mode!" << std::endl;
                    failures++;
                }
            }

            pfnShutdown();
            int shutdownMode = pfnGetMode();
            if (shutdownMode == 0 && !pfnIsLegacyInit()) {
                std::cout << "  [PASS] Fallback transport cleanly shut down and socket released." << std::endl;
            } else {
                std::cerr << "  [FAIL] Fallback shutdown failed to reset state or release legacy socket!" << std::endl;
                failures++;
            }
        }
    }

    // =========================================================================
    // TEST 3: Clean Rejection when Fallback is Disabled (Policy Compliance)
    // =========================================================================
    std::cout << "\n[TEST 3] Clean Failure when Canonical Fails and Fallback is Disabled..." << std::endl;
    {
        SetEnvironmentVariableA("REFIX_FORCE_CANONICAL_FAILURE", "1");
        SetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", "1");

        pfnResetCounters();
        bool initOk = pfnInit();
        int mode = pfnGetMode();
        bool legacyInit = pfnIsLegacyInit();

        std::cout << "  [*] SteamAPI_Init result: " << (initOk ? "TRUE" : "FALSE") << std::endl;
        std::cout << "  [*] Transport Mode: " << mode << " (3 = Failed)" << std::endl;
        std::cout << "  [*] Legacy Socket Initialized: " << (legacyInit ? "TRUE" : "FALSE") << std::endl;

        if (initOk) {
            std::cerr << "  [FAIL] SteamAPI_Init succeeded when canonical failed AND fallbacks were disabled!" << std::endl;
            failures++;
            pfnShutdown();
        } else {
            std::cout << "  [PASS] SteamAPI_Init cleanly rejected initialization." << std::endl;
        }

        if (mode != 3) {
            std::cerr << "  [FAIL] Expected TransportMode::Failed (3), got: " << mode << std::endl;
            failures++;
        } else {
            std::cout << "  [PASS] Explicit TransportMode::Failed state preserved." << std::endl;
        }

        if (legacyInit) {
            std::cerr << "  [FAIL] Legacy socket was initialized despite REFIX_DISABLE_LEGACY_FALLBACKS=1!" << std::endl;
            failures++;
        } else {
            std::cout << "  [PASS] No fallback socket opened when policy disallows fallback." << std::endl;
        }

        // Clean up environment variables
        SetEnvironmentVariableA("REFIX_FORCE_CANONICAL_FAILURE", nullptr);
        SetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", nullptr);
    }

    // =========================================================================
    // TEST 4: Production Export Table Isolation (No Test Hooks Leaked)
    // =========================================================================
    std::cout << "\n[TEST 4] Production DLL Export Table Isolation..." << std::endl;
    {
        HMODULE hProd = LoadProdDll(customDllPath);
        if (hProd) {
            bool leaked = false;
            if (GetProcAddress(hProd, "ReFix_Test_GetTransportMode") != nullptr) {
                std::cerr << "  [FAIL] Production DLL exports ReFix_Test_GetTransportMode!" << std::endl;
                leaked = true;
            }
            if (GetProcAddress(hProd, "ReFix_Test_GetEffectiveDiscoveryPort") != nullptr) {
                std::cerr << "  [FAIL] Production DLL exports ReFix_Test_GetEffectiveDiscoveryPort!" << std::endl;
                leaked = true;
            }
            if (GetProcAddress(hProd, "ReFix_Test_IsLegacySocketInitialized") != nullptr) {
                std::cerr << "  [FAIL] Production DLL exports ReFix_Test_IsLegacySocketInitialized!" << std::endl;
                leaked = true;
            }
            if (GetProcAddress(hProd, "ReFix_Test_ResetLegacyCounters") != nullptr) {
                std::cerr << "  [FAIL] Production DLL exports ReFix_Test_ResetLegacyCounters!" << std::endl;
                leaked = true;
            }

            if (!leaked) {
                std::cout << "  [PASS] Verified zero test hooks exported in production DLL." << std::endl;
            } else {
                failures++;
            }
            FreeLibrary(hProd);
        } else {
            std::cout << "  [SKIP] Production steam_api64.dll not found in candidate paths (build test only)." << std::endl;
        }
    }

    FreeLibrary(hTest);
    WSACleanup();

    std::cout << "\n==========================================================" << std::endl;
    if (failures == 0) {
        std::cout << "  [ALL PASS] H-01 TRANSPORT OWNERSHIP VERIFIED (0 FAILURES)" << std::endl;
        std::cout << "==========================================================" << std::endl;
        return 0;
    } else {
        std::cerr << "  [FAILED] " << failures << " CHECKS FAILED IN H-01 VERIFICATION" << std::endl;
        std::cout << "==========================================================" << std::endl;
        return 1;
    }
}
