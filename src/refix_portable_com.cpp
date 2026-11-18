#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <atomic>
#include <string>
#include "refix_portable_com.h"
#include "minhook/MinHook.h"

extern void ReFixLog(const char* fmt, ...);

namespace {

// Classes served by XAudio2_7.dll (xaudio2.h / xaudio2fx.h of the June 2010 SDK).
const CLSID kXAudio2Classes[] = {
    { 0x5a508685, 0xa254, 0x4fba, { 0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf } },  // XAudio2
    { 0xdb05ea35, 0x0329, 0x4d4b, { 0xa5, 0x3a, 0x6d, 0xea, 0xd0, 0x3d, 0x38, 0x52 } },  // XAudio2_Debug
    { 0xcac1105f, 0x619b, 0x4d04, { 0x83, 0x1a, 0x44, 0xe1, 0xcb, 0xf1, 0x2d, 0x57 } },  // AudioVolumeMeter
    { 0x6a93130e, 0x1d53, 0x41d1, { 0xa9, 0xcf, 0xe7, 0x58, 0x80, 0x0b, 0xb1, 0x79 } },  // AudioReverb
};

using CoCreateInstanceFn = HRESULT(WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
CoCreateInstanceFn g_origCoCreateInstance = nullptr;

bool IsXAudio2Class(REFCLSID clsid) {
    for (const CLSID& c : kXAudio2Classes)
        if (IsEqualCLSID(clsid, c)) return true;
    return false;
}

// The game's copy: already loaded by the game, else next to its binaries (deploy extracts it there), else the exe folder.
HMODULE LocalXAudio2() {
    if (HMODULE h = GetModuleHandleW(L"XAudio2_7.dll")) return h;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    for (const wchar_t* sub : { L"bin\\", L"" }) {
        if (HMODULE h = LoadLibraryExW((dir + sub + L"XAudio2_7.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) return h;
    }
    return nullptr;
}

HRESULT WINAPI Hooked_CoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD ctx, REFIID iid, LPVOID* out) {
    HRESULT hr = g_origCoCreateInstance(clsid, outer, ctx, iid, out);
    if (hr != REGDB_E_CLASSNOTREG || !IsXAudio2Class(clsid)) return hr;
    HMODULE dll = LocalXAudio2();
    auto getClassObject = dll ? reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(dll, "DllGetClassObject")) : nullptr;
    if (!getClassObject) return hr;
    IClassFactory* factory = nullptr;
    if (FAILED(getClassObject(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory))) || !factory) return hr;
    const HRESULT created = factory->CreateInstance(outer, iid, out);
    factory->Release();
    static std::atomic<bool> logged{ false };
    if (!logged.exchange(true))
        ReFixLog("[PortableCOM] XAudio2 2.7 is not registered on this PC: created from the game's XAudio2_7.dll (hr=0x%08lx)",
                 (unsigned long)created);
    return SUCCEEDED(created) ? created : hr;
}

} // namespace

void ReFix_InstallPortableXAudio2() {
    static std::atomic<bool> installed{ false };
    if (installed.exchange(true)) return;
    HMODULE ole = LoadLibraryW(L"ole32.dll");
    void* target = ole ? reinterpret_cast<void*>(GetProcAddress(ole, "CoCreateInstance")) : nullptr;  // combase's, on Win10+
    if (!target) return;
    MH_Initialize();  // no-op if another ReFix module already initialized MinHook
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Hooked_CoCreateInstance), reinterpret_cast<void**>(&g_origCoCreateInstance)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        ReFixLog("[PortableCOM] Could not hook CoCreateInstance: XAudio2 stays registry-only");
    }
}
