// =============================================================================
// ReFix EOS Online v2 - Synthetic Callback 168 Unit Test Suite
// =============================================================================
#include "../src/identity/online_identity_provider.h"
#include "../src/eos/eos_connect.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <mutex>
#include <atomic>

#pragma pack(push, 8)
struct Steam_GetTicketForWebApiResponse_t {
    enum { k_iCallback = 168 };
    uint32_t m_hAuthTicket;
    int32_t  m_eResult;
    int32_t  m_cubTicket;
    uint8_t  m_rgubTicket[1024];
};
#pragma pack(pop)

// Simulated RedpointEOS CCallbackBase receiver
class MockRedpointEOSCallbackReceiver {
public:
    MockRedpointEOSCallbackReceiver() : m_called(false), m_receivedHandle(0), m_receivedResult(-1), m_receivedSize(0) {
        memset(m_receivedTicket, 0, sizeof(m_receivedTicket));
    }

    virtual void Run(void* pvParam) {
        if (!pvParam) return;
        auto* resp = (Steam_GetTicketForWebApiResponse_t*)pvParam;
        m_receivedHandle = resp->m_hAuthTicket;
        m_receivedResult = resp->m_eResult;
        m_receivedSize = resp->m_cubTicket;
        if (resp->m_cubTicket > 0 && resp->m_cubTicket <= 1024) {
            memcpy(m_receivedTicket, resp->m_rgubTicket, resp->m_cubTicket);
        }
        m_called = true;
    }

    virtual void Run(void* pvParam, bool bIOFailure, uint64_t hSteamAPICall) {
        (void)bIOFailure;
        (void)hSteamAPICall;
        Run(pvParam);
    }

    virtual int GetCallbackSizeBytes() {
        return sizeof(Steam_GetTicketForWebApiResponse_t);
    }

    bool m_called;
    uint32_t m_receivedHandle;
    int32_t m_receivedResult;
    int32_t m_receivedSize;
    uint8_t m_receivedTicket[1024];
};

static bool SafeCallRun(void* pCallback, void* pData) {
    if (!pCallback) return false;
    __try {
        void** vtable = *(void***)pCallback;
        if (!vtable) return false;

        typedef void (*fn_Run0_t)(void* self, void* pvParam);
        fn_Run0_t pRun0 = (fn_Run0_t)vtable[0];
        if (pRun0) {
            pRun0(pCallback, pData);
            return true;
        }
        return false;
    } __except (1) {
        return false;
    }
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix - Synthetic Callback 168 Unit Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // TEST 1: ABI Layout & Packing of Steam_GetTicketForWebApiResponse_t
    std::cout << "\n[TEST 1] Steam_GetTicketForWebApiResponse_t ABI Layout..." << std::endl;
    assert(Steam_GetTicketForWebApiResponse_t::k_iCallback == 168);
    assert(sizeof(Steam_GetTicketForWebApiResponse_t) == 1040 || sizeof(Steam_GetTicketForWebApiResponse_t) == 1036);
    std::cout << "  [PASS] Struct size: " << sizeof(Steam_GetTicketForWebApiResponse_t) << " bytes with 8-byte packing alignment." << std::endl;

    // TEST 2: Fallback Construction backed by a genuine Steam Session Ticket
    std::cout << "\n[TEST 2] Synthetic Callback 168 Construction with Session Ticket..." << std::endl;
    std::vector<uint8_t> genuineSessionTicket = {
        0x14, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x10, 0x01, 0x76, 0x56, 0x11, 0x97,
        0x96, 0x02, 0x65, 0x72, 0xAA, 0xBB, 0xCC, 0xDD,
        0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55
    };
    uint32_t genuineHandle = 4201;

    Steam_GetTicketForWebApiResponse_t cbResponse = {};
    cbResponse.m_hAuthTicket = genuineHandle;
    cbResponse.m_eResult = 1; // k_EResultOK
    cbResponse.m_cubTicket = (int32_t)genuineSessionTicket.size();
    memcpy(cbResponse.m_rgubTicket, genuineSessionTicket.data(), genuineSessionTicket.size());

    assert(cbResponse.m_hAuthTicket == genuineHandle);
    assert(cbResponse.m_eResult == 1);
    assert(cbResponse.m_cubTicket == 32);
    assert(memcmp(cbResponse.m_rgubTicket, genuineSessionTicket.data(), 32) == 0);
    std::cout << "  [PASS] Callback 168 successfully constructed with genuine ticket payload." << std::endl;

    // TEST 3: Virtual Dispatch to Simulated RedpointEOS Receiver
    std::cout << "\n[TEST 3] Virtual Dispatch to RedpointEOS Callback Receiver..." << std::endl;
    MockRedpointEOSCallbackReceiver receiver;
    bool dispatchOk = SafeCallRun(&receiver, &cbResponse);
    assert(dispatchOk);
    assert(receiver.m_called);
    assert(receiver.m_receivedHandle == genuineHandle);
    assert(receiver.m_receivedResult == 1);
    assert(receiver.m_receivedSize == 32);
    assert(memcmp(receiver.m_receivedTicket, genuineSessionTicket.data(), 32) == 0);
    std::cout << "  [PASS] Virtual SafeCallRun correctly delivered Callback 168 to receiver!" << std::endl;

    // TEST 4: Identity Provider Credential Validation with Session Ticket
    std::cout << "\n[TEST 4] Identity Provider Strict Validation of Session Ticket..." << std::endl;
    auto provider = ReFixIdentity::GetActiveIdentityProvider();
    provider->SetCapturedSteamTicket(genuineSessionTicket.data(), genuineSessionTicket.size(), genuineHandle);
    assert(provider->HasCapturedTicket());
    assert(provider->GetCapturedTicketHandle() == genuineHandle);

    std::string hexToken = ReFixIdentity::BytesToHex(receiver.m_receivedTicket, receiver.m_receivedSize);
    assert(!hexToken.empty());

    // Valid token matching captured ticket
    bool valid = provider->ValidateCredential(EOS_ECT_STEAM_SESSION_TICKET, hexToken.c_str());
    assert(valid);
    std::cout << "  [PASS] Hex ticket from Callback 168 successfully validated against Provider." << std::endl;

    // Mutated token rejection
    std::string badHex = hexToken;
    badHex[0] = (badHex[0] == '0' ? '1' : '0');
    bool badValid = provider->ValidateCredential(EOS_ECT_STEAM_SESSION_TICKET, badHex.c_str());
    assert(!badValid);
    std::cout << "  [PASS] Mutated ticket strictly rejected by Provider." << std::endl;

    // TEST 5: Ticket Invalidation (CancelAuthTicket)
    std::cout << "\n[TEST 5] Ticket Invalidation (CancelAuthTicket)..." << std::endl;
    provider->InvalidateCapturedTicket(genuineHandle);
    assert(!provider->HasCapturedTicket());
    assert(provider->GetCapturedTicketHandle() == 0);
    bool afterCancelValid = provider->ValidateCredential(EOS_ECT_STEAM_SESSION_TICKET, hexToken.c_str());
    assert(!afterCancelValid);
    std::cout << "  [PASS] Ticket successfully invalidated on CancelAuthTicket." << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Synthetic Callback 168 Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
