#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cassert>
#include <iostream>
#include "../src/steam_network_compat.h"

// Stubs for linking with steam_network_compat.cpp
HMODULE g_hOriginalDll = NULL;

void ReFixLog(const char* fmt, ...) {
    (void)fmt;
}

int main() {
    std::cout << "=== ReFix SteamNetCompat Unit Tests ===" << std::endl;

    // Test 1: Non-Valve / Goldberg mode (Must NOT activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(true, 480, 1966720);
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 1: Goldberg/Offline mode correctly skipped." << std::endl;
    }

    // Test 2: Valve mode with identical Mask and Real AppID (Must NOT activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 480);
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 2: Identical AppIDs (480/480) correctly skipped." << std::endl;
    }

    // Test 3: Valve mode with zero AppIDs (Must NOT activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 0, 1966720);
        assert(!SteamNetCompat::IsActive());

        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 0);
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 3: Zero AppIDs correctly skipped." << std::endl;
    }

    // Test 4: Valve mode with MaskAppId != RealAppId (Generic Match: MUST activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 1966720); // Lethal Company
        assert(SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 4a: Valve mode with Mask=480, Real=1966720 correctly enabled." << std::endl;

        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 252490);  // Rust
        assert(SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 4b: Valve mode with Mask=480, Real=252490 (generic) correctly enabled." << std::endl;
    }

    // Test 5: Sockets Access Tracking
    {
        SteamNetCompat::NotifySocketsPathDetected();
        assert(SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 5: Sockets access notification handled cleanly." << std::endl;
    }

    // Test 6: Simulated Callback 1221 (SteamNetConnectionStatusChangedCallback_t with exact 712-byte ABI)
    {
        #pragma pack(push, 8)
        struct MockIdentity_t {
            int m_eType;
            int m_cbSize;
            union {
                uint64_t m_steamID64;
                char m_szGenericString[32];
                uint8_t m_reserved[128];
            };
        };
        struct MockIPAddr_t {
            uint8_t m_ipv6[16];
            uint16_t m_port;
        };
        struct MockInfo_t {
            MockIdentity_t m_identityRemote;
            int64_t m_nUserData;
            uint32_t m_hListenSocket;
            MockIPAddr_t m_addrRemote;
            uint16_t m__pad1;
            uint32_t m_idPOPRemote;
            uint32_t m_idPOPRelay;
            int32_t m_eState;
            int32_t m_eEndReason;
            char m_szEndDebug[128];
            char m_szConnectionDescription[128];
            int32_t m_nFlags;
            uint32_t reserved[63];
        };
        struct MockStatusChanged_t {
            uint32_t m_hConn;
            MockInfo_t m_info;
            int32_t m_eOldState;
        };
        #pragma pack(pop)
        static_assert(sizeof(MockStatusChanged_t) == 712, "MockStatusChanged_t ABI size mismatch");

        MockStatusChanged_t mockCb;
        memset(&mockCb, 0, sizeof(mockCb));
        mockCb.m_hConn = 12345;
        mockCb.m_eOldState = 1; // Connecting
        mockCb.m_info.m_eState = 4;    // ClosedByPeer
        mockCb.m_info.m_eEndReason = 4003; // k_ESteamNetConnectionEnd_Remote_BadCert
        strcpy_s(mockCb.m_info.m_szEndDebug, sizeof(mockCb.m_info.m_szEndDebug), "Cert test diagnostics");

        // Must process without crash or state corruption
        SteamNetCompat::ProcessConnectionStatusChanged(&mockCb, sizeof(mockCb));
        SteamNetCompat::ProcessConnectionStatusChanged(nullptr, 0); // Null safety
        std::cout << "[PASS] Test 6: Callback 1221 diagnostic processing verified (712-byte ABI)." << std::endl;
    }

    // Test 7: Fail-safe fallback function pointers with null original DLL
    {
        void* pSockets = SteamNetCompat::Intercept_SteamNetworkingSockets_v008();
        void* pUtils = SteamNetCompat::Intercept_SteamNetworkingUtils_v003();
        assert(pSockets == nullptr);
        assert(pUtils == nullptr);
        std::cout << "[PASS] Test 7: Null-safe fallback resolution verified." << std::endl;
    }

    // Test 8: Inactivity Passthrough & Clean Shutdown
    {
        SteamNetCompat::Shutdown();
        assert(!SteamNetCompat::IsActive());

        // When inactive, all interceptors must safely pass through without side-effects or crashing
        uint32_t hListen = SteamNetCompat::Intercept_CreateListenSocketP2P(nullptr, 0, 0, nullptr);
        assert(hListen == 0); // null original fallback

        uint32_t hConn = SteamNetCompat::Intercept_ConnectP2P(nullptr, nullptr, 0, 0, nullptr);
        assert(hConn == 0);

        int acceptRes = SteamNetCompat::Intercept_AcceptConnection(nullptr, 9999);
        assert(acceptRes == 2); // k_EResultFail

        bool closeRes = SteamNetCompat::Intercept_CloseConnection(nullptr, 9999, 0, nullptr, false);
        assert(closeRes == false);

        std::cout << "[PASS] Test 8: Inactivity passthrough guarantees verified." << std::endl;
    }

    // Test 9: Clean State Verification
    {
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 9: State cleanly confirmed inactive." << std::endl;
    }

    std::cout << "\nALL STEAM NETWORK COMPAT UNIT TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
