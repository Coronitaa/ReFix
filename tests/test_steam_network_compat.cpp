#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <isteamnetworkingsockets.h>
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
    std::cout << "=== ReFix SteamNetCompat Hardened Unit Tests ===" << std::endl;

    // Test 1: Non-Valve / Goldberg mode (Must NOT be eligible and NOT active)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(true, 480, 1966720);
        assert(!SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive());
        SteamNetCompat::NotifySocketsPathDetected();
        assert(!SteamNetCompat::IsActive()); // Still inactive even if sockets detected
        std::cout << "[PASS] Test 1: Goldberg/Offline mode correctly skipped (IsEligible=false, IsActive=false)." << std::endl;
    }

    // Test 2: Valve mode with identical Mask and Real AppID (Must NOT activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 480);
        assert(!SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive());
        SteamNetCompat::NotifySocketsPathDetected();
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 2: Identical AppIDs (480/480) correctly skipped." << std::endl;
    }

    // Test 3: Valve mode with zero AppIDs (Must NOT activate)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 0, 1966720);
        assert(!SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive());

        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 0);
        assert(!SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 3: Zero AppIDs correctly skipped." << std::endl;
    }

    // Test 4: Two-Phase Activation (Eligible on Init, Active ONLY when sockets accessed)
    {
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 1966720); // Lethal Company
        assert(SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive()); // Phase 1: Eligible, but NOT active yet!
        std::cout << "[PASS] Test 4a: Phase 1 verified - Eligible=true, Active=false (game has not touched sockets)." << std::endl;

        // Phase 2: Sockets accessed
        SteamNetCompat::NotifySocketsPathDetected();
        assert(SteamNetCompat::IsActive()); // Phase 2: Now active!
        std::cout << "[PASS] Test 4b: Phase 2 verified - Active=true after Sockets path detected." << std::endl;

        // Generic test with Rust (AppID 252490)
        SteamNetCompat::Shutdown();
        SteamNetCompat::Initialize();
        SteamNetCompat::OnSteamInitialized(false, 480, 252490);
        assert(SteamNetCompat::IsEligible());
        assert(!SteamNetCompat::IsActive());
        SteamNetCompat::NotifySocketsPathDetected();
        assert(SteamNetCompat::IsActive());
        std::cout << "[PASS] Test 4c: Generic masked AppID 252490 correctly transitioned to active." << std::endl;
    }

    // Test 5: Simulated Callback 1221 using official Steam SDK struct
    {
        static_assert(sizeof(SteamNetConnectionStatusChangedCallback_t) == 712, "Official struct size mismatch");

        SteamNetConnectionStatusChangedCallback_t cb;
        memset(&cb, 0, sizeof(cb));
        cb.m_hConn = 12345;
        cb.m_eOldState = k_ESteamNetworkingConnectionState_Connecting;
        cb.m_info.m_eState = k_ESteamNetworkingConnectionState_ClosedByPeer;
        cb.m_info.m_eEndReason = 4003; // k_ESteamNetConnectionEnd_Remote_BadCert
        strcpy_s(cb.m_info.m_szEndDebug, sizeof(cb.m_info.m_szEndDebug), "Cert test diagnostics");

        // Must process without crash or state corruption
        SteamNetCompat::ProcessConnectionStatusChanged(&cb, sizeof(cb));
        SteamNetCompat::ProcessConnectionStatusChanged(nullptr, 0); // Null safety
        std::cout << "[PASS] Test 5: Callback 1221 diagnostic processing verified using official SDK struct." << std::endl;
    }

    // Test 6: Fail-safe fallback function pointers with null original DLL
    {
        void* pSockets = SteamNetCompat::Intercept_SteamNetworkingSockets_v008();
        void* pUtils = SteamNetCompat::Intercept_SteamNetworkingUtils_v003();
        assert(pSockets == nullptr);
        assert(pUtils == nullptr);
        std::cout << "[PASS] Test 6: Null-safe fallback resolution verified." << std::endl;
    }

    // Test 7: Inactivity Passthrough & Clean Shutdown
    {
        SteamNetCompat::Shutdown();
        assert(!SteamNetCompat::IsEligible());
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

        std::cout << "[PASS] Test 7: Inactivity passthrough guarantees verified." << std::endl;
    }

    // Test 8: Clean State Verification
    {
        assert(!SteamNetCompat::IsActive());
        assert(!SteamNetCompat::IsEligible());
        std::cout << "[PASS] Test 8: State cleanly confirmed inactive." << std::endl;
    }

    std::cout << "\nALL STEAM NETWORK COMPAT HARDENED UNIT TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
