#include "../src/eos/eos_identity.h"
#include <iostream>
#include <cassert>
#include <cstring>

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Identity Interoperability Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();

    // -------------------------------------------------------------------------
    // TEST 1: Local Opaque Handles & Persistence
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] Local Opaque Handles & Persistence..." << std::endl;
    EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
    EOS_EpicAccountId localEaid = idMgr.GetLocalEpicAccountId();

    assert(localPuid != nullptr);
    assert(localEaid != nullptr);
    assert(EOS_ProductUserId_IsValid(localPuid) == 1);
    assert(EOS_EpicAccountId_IsValid(localEaid) == 1);

    char localPuidStr[64] = { 0 };
    int32_t len = sizeof(localPuidStr);
    assert(EOS_ProductUserId_ToString(localPuid, localPuidStr, &len) == EOS_Success);
    std::cout << "  Local PUID String: " << localPuidStr << std::endl;
    assert(strlen(localPuidStr) == 32);

    // -------------------------------------------------------------------------
    // TEST 2: Identity Interoperability (ToString -> FromString -> Equality)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] Identity Interoperability (Roundtrip & Equivalence)..." << std::endl;
    EOS_ProductUserId puidPrime = EOS_ProductUserId_FromString(localPuidStr);
    assert(puidPrime != nullptr);
    assert(puidPrime == localPuid); // Handle equivalence

    char puidPrimeStr[64] = { 0 };
    len = sizeof(puidPrimeStr);
    assert(EOS_ProductUserId_ToString(puidPrime, puidPrimeStr, &len) == EOS_Success);
    assert(strcmp(localPuidStr, puidPrimeStr) == 0); // Value equivalence
    std::cout << "  Roundtrip handle equality and value equivalence verified!" << std::endl;

    // Cross-Process Simulation (Export from Process A -> Import in Process B)
    std::string simulatedExportFromProcA = "a1b2c3d4e5f60718293a4b5c6d7e8f90";
    EOS_ProductUserId importedInProcB = EOS_ProductUserId_FromString(simulatedExportFromProcA.c_str());
    assert(importedInProcB != nullptr);
    assert(EOS_ProductUserId_IsValid(importedInProcB) == 1);

    char procBStr[64] = { 0 };
    len = sizeof(procBStr);
    assert(EOS_ProductUserId_ToString(importedInProcB, procBStr, &len) == EOS_Success);
    assert(simulatedExportFromProcA == procBStr);
    std::cout << "  Cross-process simulation string exchange verified!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 3: Foreign PUIDs (Multiple Independent Users)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] Foreign Independent ProductUserIds..." << std::endl;
    EOS_ProductUserId puid_A = idMgr.GetOrCreateProductUserId("11111111111111111111111111111111");
    EOS_ProductUserId puid_B = idMgr.GetOrCreateProductUserId("22222222222222222222222222222222");
    EOS_ProductUserId puid_C = idMgr.GetOrCreateProductUserId("33333333333333333333333333333333");

    assert(puid_A != nullptr && puid_B != nullptr && puid_C != nullptr);
    assert(EOS_ProductUserId_IsValid(puid_A) == 1);
    assert(EOS_ProductUserId_IsValid(puid_B) == 1);
    assert(EOS_ProductUserId_IsValid(puid_C) == 1);

    // Mutual Distinctness
    assert(puid_A != puid_B);
    assert(puid_B != puid_C);
    assert(puid_A != puid_C);
    assert(puid_A != localPuid);

    char bufA[64], bufB[64], bufC[64];
    len = sizeof(bufA); assert(EOS_ProductUserId_ToString(puid_A, bufA, &len) == EOS_Success);
    len = sizeof(bufB); assert(EOS_ProductUserId_ToString(puid_B, bufB, &len) == EOS_Success);
    len = sizeof(bufC); assert(EOS_ProductUserId_ToString(puid_C, bufC, &len) == EOS_Success);

    assert(strcmp(bufA, "11111111111111111111111111111111") == 0);
    assert(strcmp(bufB, "22222222222222222222222222222222") == 0);
    assert(strcmp(bufC, "33333333333333333333333333333333") == 0);
    std::cout << "  PUID_A, PUID_B, PUID_C generated, distinct, and verified!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 4: Malformed, Empty, and Error Handling Cases
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] Malformed, Empty & Error Cases..." << std::endl;
    assert(EOS_ProductUserId_FromString(nullptr) == nullptr);
    assert(EOS_ProductUserId_FromString("") == nullptr);
    assert(EOS_ProductUserId_FromString("short_id") == nullptr);
    assert(EOS_ProductUserId_FromString("1234567890123456789012345678901234567890") == nullptr); // Too long (40 chars)
    assert(EOS_ProductUserId_FromString("1234567890123456789012345678901z") == nullptr); // Non-hex char 'z'

    assert(EOS_EpicAccountId_FromString(nullptr) == nullptr);
    assert(EOS_EpicAccountId_FromString("") == nullptr);
    assert(EOS_EpicAccountId_FromString("malformed") == nullptr);

    // Invalid pointer ToString checks
    char errBuf[64] = { 0 };
    len = sizeof(errBuf);
    assert(EOS_ProductUserId_ToString(nullptr, errBuf, &len) == EOS_InvalidUser);
    assert(EOS_ProductUserId_ToString((void*)0xDEADBEEF, errBuf, &len) == EOS_InvalidUser);
    assert(EOS_EpicAccountId_ToString(nullptr, errBuf, &len) == EOS_InvalidUser);
    assert(EOS_EpicAccountId_ToString((void*)0xDEADBEEF, errBuf, &len) == EOS_InvalidUser);

    // Invalid pointer IsValid checks
    assert(EOS_ProductUserId_IsValid(nullptr) == 0);
    assert(EOS_ProductUserId_IsValid((void*)0xDEADBEEF) == 0);
    assert(EOS_EpicAccountId_IsValid(nullptr) == 0);
    assert(EOS_EpicAccountId_IsValid((void*)0xDEADBEEF) == 0);
    std::cout << "  All error and boundary conditions handled safely without crashes!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 5: Handle Lifetime Verification
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 5] Handle Lifetime Stability..." << std::endl;
    void* initialLocalPuidPtr = (void*)localPuid;
    for (int i = 0; i < 1000; ++i) {
        char tempStr[64];
        sprintf_s(tempStr, "%08x%08x%08x%08x", i, i + 1, i + 2, i + 3);
        EOS_ProductUserId tempPuid = EOS_ProductUserId_FromString(tempStr);
        assert(tempPuid != nullptr);
    }
    // Verify local PUID handle address did not move or corrupt
    assert((void*)idMgr.GetLocalProductUserId() == initialLocalPuidPtr);
    assert(EOS_ProductUserId_IsValid(localPuid) == 1);
    std::cout << "  Memory handle stability across 1000 allocations verified!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Identity Interoperability Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
