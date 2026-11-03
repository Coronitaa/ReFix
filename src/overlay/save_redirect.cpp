#include "save_redirect.h"
#include "overlay_text.h"
#include "../minhook/MinHook.h"

#include <windows.h>
#include <winternl.h>
#include <shlobj.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

constexpr ULONG kFileRenameInformation = 10;
constexpr ULONG kFileLinkInformation = 11;
constexpr ULONG kFileRenameInformationEx = 65;
constexpr ULONG kFileLinkInformationEx = 72;
constexpr ULONG kFileOpen = 1; // FILE_OPEN disposition: never creates anything

// FILE_RENAME_INFORMATION / FILE_LINK_INFORMATION (same layout).
struct RenameInfo {
    union { BOOLEAN ReplaceIfExists; ULONG Flags; };
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
};

struct Mapping {
    std::wstring from;                    // NT path of the real folder, e.g. \??\C:\Users\me\AppData\Local
    std::wstring to;                      // NT path inside the profile
    std::vector<std::wstring> excluded;   // first-level sub-folders that stay on the real folder
};

std::vector<Mapping> g_maps;
std::atomic<bool> g_installed{false};
thread_local int t_depth = 0;

using NtCreateFile_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
                                        ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using NtOpenFile_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
using NtQueryAttr_t = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES, PVOID);
using NtSetInfo_t = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
using NtDeleteFile_t = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES);
using RegOpenW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
using RegCreateW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, const LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);

NtCreateFile_t o_NtCreateFile = nullptr;
NtOpenFile_t o_NtOpenFile = nullptr;
NtQueryAttr_t o_NtQueryAttributesFile = nullptr;
NtQueryAttr_t o_NtQueryFullAttributesFile = nullptr;
NtSetInfo_t o_NtSetInformationFile = nullptr;
NtDeleteFile_t o_NtDeleteFile = nullptr;
RegOpenW_t o_RegOpenKeyExW = nullptr;
RegCreateW_t o_RegCreateKeyExW = nullptr;

std::wstring g_unityKey;      // Software\<Company>\<Product>
std::wstring g_unityTarget;   // Software\ReFix LAN\Profiles\<id>\<Company>\<Product>

bool EqualNoCase(const wchar_t* a, const wchar_t* b, size_t n) {
    return CompareStringOrdinal(a, (int)n, b, (int)n, TRUE) == CSTR_EQUAL;
}

// Rewritten NT path for `name`, or false when it stays as is.
bool Rewrite(const wchar_t* name, size_t len, std::wstring& out) {
    for (const Mapping& m : g_maps) {
        const size_t n = m.from.size();
        if (len < n || !EqualNoCase(name, m.from.c_str(), n)) continue;
        if (len > n && name[n] != L'\\') continue;
        // First component after the root decides the exclusions.
        const wchar_t* rest = name + n;
        size_t restLen = len - n;
        if (restLen > 1) {
            const wchar_t* comp = rest + 1;
            size_t compLen = 0;
            while (compLen < restLen - 1 && comp[compLen] != L'\\') ++compLen;
            for (const std::wstring& ex : m.excluded) {
                if (ex.size() == compLen && EqualNoCase(comp, ex.c_str(), compLen)) return false;
                if (ex.back() == L'*' && compLen >= ex.size() - 1 && EqualNoCase(comp, ex.c_str(), ex.size() - 1))
                    return false;
            }
        }
        out.assign(m.to);
        out.append(rest, restLen);
        return true;
    }
    return false;
}

void EnsureParentDirs(const std::wstring& ntPath) {
    // \??\C:\...\file -> C:\...
    std::wstring win = ntPath.compare(0, 4, L"\\??\\") == 0 ? ntPath.substr(4) : ntPath;
    size_t slash = win.find_last_of(L'\\');
    if (slash == std::wstring::npos) return;
    win.resize(slash);
    for (size_t i = 3; i <= win.size(); ++i) {
        if (i == win.size() || win[i] == L'\\') CreateDirectoryW(win.substr(0, i).c_str(), nullptr);
    }
}

// Holds a rewritten OBJECT_ATTRIBUTES for the duration of one call.
struct Redirected {
    std::wstring path;
    UNICODE_STRING us{};
    OBJECT_ATTRIBUTES oa{};
    bool active = false;

    explicit Redirected(POBJECT_ATTRIBUTES src) {
        if (t_depth > 0 || !src || src->RootDirectory || !src->ObjectName || !src->ObjectName->Buffer) return;
        const UNICODE_STRING* n = src->ObjectName;
        if (!Rewrite(n->Buffer, n->Length / sizeof(wchar_t), path)) return;
        us.Buffer = path.data();
        us.Length = (USHORT)(path.size() * sizeof(wchar_t));
        us.MaximumLength = us.Length;
        oa = *src;
        oa.ObjectName = &us;
        active = true;
    }
    POBJECT_ATTRIBUTES Get(POBJECT_ATTRIBUTES original) { return active ? &oa : original; }
};

struct DepthGuard {
    DepthGuard() { ++t_depth; }
    ~DepthGuard() { --t_depth; }
};

NTSTATUS NTAPI Hook_NtCreateFile(PHANDLE h, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PIO_STATUS_BLOCK iosb,
                                 PLARGE_INTEGER alloc, ULONG attrs, ULONG share, ULONG disposition, ULONG options,
                                 PVOID ea, ULONG eaLen) {
    Redirected r(oa);
    if (r.active && disposition != kFileOpen) {
        DepthGuard g;
        EnsureParentDirs(r.path);
    }
    return o_NtCreateFile(h, access, r.Get(oa), iosb, alloc, attrs, share, disposition, options, ea, eaLen);
}

NTSTATUS NTAPI Hook_NtOpenFile(PHANDLE h, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PIO_STATUS_BLOCK iosb,
                               ULONG share, ULONG options) {
    Redirected r(oa);
    return o_NtOpenFile(h, access, r.Get(oa), iosb, share, options);
}

NTSTATUS NTAPI Hook_NtQueryAttributesFile(POBJECT_ATTRIBUTES oa, PVOID info) {
    Redirected r(oa);
    return o_NtQueryAttributesFile(r.Get(oa), info);
}

NTSTATUS NTAPI Hook_NtQueryFullAttributesFile(POBJECT_ATTRIBUTES oa, PVOID info) {
    Redirected r(oa);
    return o_NtQueryFullAttributesFile(r.Get(oa), info);
}

NTSTATUS NTAPI Hook_NtDeleteFile(POBJECT_ATTRIBUTES oa) {
    Redirected r(oa);
    return o_NtDeleteFile(r.Get(oa));
}

// Renames carry the absolute target name inside the information buffer.
NTSTATUS NTAPI Hook_NtSetInformationFile(HANDLE h, PIO_STATUS_BLOCK iosb, PVOID info, ULONG len, ULONG cls) {
    const bool isRename = cls == kFileRenameInformation || cls == kFileLinkInformation ||
                          cls == kFileRenameInformationEx || cls == kFileLinkInformationEx;
    if (!isRename || !info || t_depth > 0 || len < sizeof(RenameInfo)) return o_NtSetInformationFile(h, iosb, info, len, cls);
    const RenameInfo* ri = static_cast<const RenameInfo*>(info);
    std::wstring target;
    if (ri->RootDirectory || !Rewrite(ri->FileName, ri->FileNameLength / sizeof(wchar_t), target))
        return o_NtSetInformationFile(h, iosb, info, len, cls);
    const size_t bytes = offsetof(RenameInfo, FileName) + target.size() * sizeof(wchar_t);
    std::vector<uint8_t> buf(bytes + sizeof(wchar_t), 0);
    RenameInfo* copy = reinterpret_cast<RenameInfo*>(buf.data());
    copy->Flags = ri->Flags;
    copy->RootDirectory = nullptr;
    copy->FileNameLength = (ULONG)(target.size() * sizeof(wchar_t));
    memcpy(copy->FileName, target.data(), copy->FileNameLength);
    return o_NtSetInformationFile(h, iosb, copy, (ULONG)bytes, cls);
}

bool UnitySubkey(HKEY root, LPCWSTR subKey, std::wstring& out) {
    if (root != HKEY_CURRENT_USER || !subKey || g_unityKey.empty()) return false;
    const size_t n = g_unityKey.size();
    const size_t len = wcslen(subKey);
    if (len < n || !EqualNoCase(subKey, g_unityKey.c_str(), n) || (len > n && subKey[n] != L'\\')) return false;
    out = g_unityTarget + (subKey + n);
    return true;
}

LSTATUS WINAPI Hook_RegOpenKeyExW(HKEY root, LPCWSTR subKey, DWORD opts, REGSAM sam, PHKEY result) {
    std::wstring alt;
    if (UnitySubkey(root, subKey, alt)) {
        // PlayerPrefs open the key for reading before it exists in a fresh profile.
        HKEY created = nullptr;
        if (o_RegCreateKeyExW(root, alt.c_str(), 0, nullptr, 0, sam, nullptr, &created, nullptr) == ERROR_SUCCESS) {
            *result = created;
            return ERROR_SUCCESS;
        }
    }
    return o_RegOpenKeyExW(root, subKey, opts, sam, result);
}

LSTATUS WINAPI Hook_RegCreateKeyExW(HKEY root, LPCWSTR subKey, DWORD reserved, LPWSTR cls, DWORD opts, REGSAM sam,
                                    const LPSECURITY_ATTRIBUTES sa, PHKEY result, LPDWORD disp) {
    std::wstring alt;
    const bool redirect = UnitySubkey(root, subKey, alt);
    return o_RegCreateKeyExW(root, redirect ? alt.c_str() : subKey, reserved, cls, opts, sam, sa, result, disp);
}

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &p)) && p) out = p;
    if (p) CoTaskMemFree(p);
    return out;
}

void AddMapping(REFKNOWNFOLDERID id, const std::wstring& dataRoot, const wchar_t* sub,
                std::vector<std::wstring> excluded) {
    std::wstring real = KnownFolder(id);
    if (real.empty()) return;
    if (!real.empty() && real.back() == L'\\') real.pop_back();
    g_maps.push_back({ L"\\??\\" + real, L"\\??\\" + dataRoot + L"\\" + sub, std::move(excluded) });
}

void BuildMappings(const std::wstring& dataRoot) {
    // Order: more specific folders first (Saved Games / Documents may live under the profile folder).
    AddMapping(FOLDERID_SavedGames, dataRoot, L"Saved Games", {});
    AddMapping(FOLDERID_Documents, dataRoot, L"Documents", {});
    AddMapping(FOLDERID_LocalAppDataLow, dataRoot, L"LocalLow",
               { L"Microsoft", L"Intel", L"NVIDIA*", L"AMD", L"Sun", L"Adobe", L"Temp" });
    AddMapping(FOLDERID_LocalAppData, dataRoot, L"Local",
               { L"Temp", L"Microsoft", L"Packages", L"D3DSCache", L"NVIDIA*", L"AMD", L"ATI", L"Intel",
                 L"CrashDumps", L"Steam", L"EpicGamesLauncher", L"Epic Games", L"UnrealEngine", L"Programs",
                 L"Comms", L"ConnectedDevicesPlatform", L"Publishers", L"VirtualStore", L"PlaceholderTileLogoFolder",
                 L"IconCache.db", L"Discord", L"CEF", L"DBG", L"PeerDistRepub", L"cache" });
    AddMapping(FOLDERID_RoamingAppData, dataRoot, L"Roaming",
               { L"ReFix LAN", L"Microsoft", L"NVIDIA*", L"AMD", L"Intel", L"Discord", L"Steam", L"Adobe",
                 L"Mozilla", L"Python", L"EasyAntiCheat" });

    // A game installed inside one of these folders (itch.io apps live in Roaming\itch) keeps reading its
    // own files from where they are.
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::wstring ntExe = std::wstring(L"\\??\\") + exe;
    for (Mapping& m : g_maps) {
        const size_t n = m.from.size();
        if (ntExe.size() <= n + 1 || !EqualNoCase(ntExe.c_str(), m.from.c_str(), n) || ntExe[n] != L'\\') continue;
        const size_t end = ntExe.find(L'\\', n + 1);
        m.excluded.push_back(ntExe.substr(n + 1, end == std::wstring::npos ? std::wstring::npos : end - n - 1));
    }
}

void LoadUnityKey(const std::wstring& profileId) {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dataDir = exe;
    size_t dot = dataDir.find_last_of(L'.');
    if (dot == std::wstring::npos) return;
    dataDir.resize(dot);
    std::ifstream f(dataDir + L"_Data\\app.info", std::ios::binary);
    if (!f) return;
    std::string company, product;
    std::getline(f, company);
    std::getline(f, product);
    auto strip = [](std::string& s) { while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back(); };
    strip(company);
    strip(product);
    if (company.empty() || product.empty()) return;
    const std::wstring cp = FromUtf8(company) + L"\\" + FromUtf8(product);
    g_unityKey = L"Software\\" + cp;
    g_unityTarget = L"Software\\ReFix LAN\\Profiles\\" + profileId + L"\\" + cp;
}

template <typename T>
bool Hook(void* target, void* detour, T* original, const char* name) {
    if (!target) return false;
    MH_STATUS st = MH_CreateHook(target, detour, reinterpret_cast<void**>(original));
    if (st == MH_OK) st = MH_EnableHook(target);
    if (st != MH_OK) ReFixLog("[SaveRedirect] Hook %s failed (%d)", name, (int)st);
    return st == MH_OK;
}

} // namespace

bool InstallSaveRedirect(const std::wstring& profileId, const std::wstring& dataRoot) {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) return true;
    CreateDirectoryW(dataRoot.c_str(), nullptr);
    BuildMappings(dataRoot);
    LoadUnityKey(profileId);
    if (g_maps.empty()) return false;

    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    bool ok = Hook(GetProcAddress(nt, "NtCreateFile"), &Hook_NtCreateFile, &o_NtCreateFile, "NtCreateFile");
    ok &= Hook(GetProcAddress(nt, "NtOpenFile"), &Hook_NtOpenFile, &o_NtOpenFile, "NtOpenFile");
    ok &= Hook(GetProcAddress(nt, "NtQueryAttributesFile"), &Hook_NtQueryAttributesFile, &o_NtQueryAttributesFile, "NtQueryAttributesFile");
    ok &= Hook(GetProcAddress(nt, "NtQueryFullAttributesFile"), &Hook_NtQueryFullAttributesFile, &o_NtQueryFullAttributesFile, "NtQueryFullAttributesFile");
    ok &= Hook(GetProcAddress(nt, "NtSetInformationFile"), &Hook_NtSetInformationFile, &o_NtSetInformationFile, "NtSetInformationFile");
    Hook(GetProcAddress(nt, "NtDeleteFile"), &Hook_NtDeleteFile, &o_NtDeleteFile, "NtDeleteFile");
    if (!g_unityKey.empty()) {
        HMODULE adv = LoadLibraryW(L"advapi32.dll");
        o_RegCreateKeyExW = reinterpret_cast<RegCreateW_t>(GetProcAddress(adv, "RegCreateKeyExW"));
        Hook(reinterpret_cast<void*>(o_RegCreateKeyExW), &Hook_RegCreateKeyExW, &o_RegCreateKeyExW, "RegCreateKeyExW");
        Hook(GetProcAddress(adv, "RegOpenKeyExW"), &Hook_RegOpenKeyExW, &o_RegOpenKeyExW, "RegOpenKeyExW");
        ReFixLog("[SaveRedirect] Unity PlayerPrefs: HKCU\\%s -> HKCU\\%s", ToUtf8(g_unityKey).c_str(), ToUtf8(g_unityTarget).c_str());
    }
    for (const Mapping& m : g_maps) ReFixLog("[SaveRedirect] %s -> %s", ToUtf8(m.from).c_str(), ToUtf8(m.to).c_str());
    ReFixLog("[SaveRedirect] %s for profile %s", ok ? "Active" : "Partially active", ToUtf8(profileId).c_str());
    return ok;
}

} // namespace refix::overlay
