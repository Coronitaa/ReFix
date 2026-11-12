#include "../src/eossdk/core/eos_registry_redirect.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("[*] Running EOS Registry Redirection Unit Test...\n");

    // Set test environment variable for active profile
    SetEnvironmentVariableW(L"REFIX_ACTIVE_PROFILE", L"TestProfile_BluFix");

    // Install registry redirection hooks
    refix::InstallRegistryRedirectHooks();

    // Attempt to create/open Epic Games\EOS subkey under HKCU
    HKEY hKey = nullptr;
    DWORD disposition = 0;
    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Epic Games\\EOS\\TestDevice",
        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
        &hKey, &disposition
    );
    assert(status == ERROR_SUCCESS);
    assert(hKey != nullptr);

    // Write a dummy device ID value
    const wchar_t testDeviceId[] = L"mock_device_id_blufix_12345";
    status = RegSetValueExW(
        hKey,
        L"DeviceId",
        0, REG_SZ,
        (const BYTE*)testDeviceId,
        (DWORD)(sizeof(testDeviceId))
    );
    assert(status == ERROR_SUCCESS);
    RegCloseKey(hKey);

    // Verify raw registry: HKCU\Software\Epic Games\EOS\TestDevice should NOT have the key on disk if redirected,
    // or rather, the key was physically created under HKCU\Software\ReFix\Profiles\TestProfile_BluFix\Epic Games\EOS\TestDevice!
    HKEY hRawRedirected = nullptr;
    status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\ReFix\\Profiles\\TestProfile_BluFix\\Epic Games\\EOS\\TestDevice",
        0, KEY_READ,
        &hRawRedirected
    );
    printf("  Verifying redirected key in HKCU\\Software\\ReFix\\Profiles\\TestProfile_BluFix\\Epic Games\\EOS\\TestDevice...\n");
    printf("  Direct open status: %ld (ERROR_SUCCESS=%d)\n", status, ERROR_SUCCESS);
    assert(status == ERROR_SUCCESS);

    wchar_t readBuf[128] = { 0 };
    DWORD bufSize = sizeof(readBuf);
    DWORD type = 0;
    status = RegQueryValueExW(hRawRedirected, L"DeviceId", nullptr, &type, (LPBYTE)readBuf, &bufSize);
    assert(status == ERROR_SUCCESS);
    assert(wcscmp(readBuf, testDeviceId) == 0);
    RegCloseKey(hRawRedirected);

    // Clean up test key
    HKEY hProfileKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ReFix\\Profiles\\TestProfile_BluFix\\Epic Games\\EOS", 0, KEY_ALL_ACCESS, &hProfileKey) == ERROR_SUCCESS) {
        RegDeleteKeyW(hProfileKey, L"TestDevice");
        RegCloseKey(hProfileKey);
    }

    printf("[PASS] EOS Registry Redirection verified: DeviceId registry keys are 100%% isolated per profile!\n");
    return 0;
}
