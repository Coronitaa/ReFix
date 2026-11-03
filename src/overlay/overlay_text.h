// Small text helpers shared by the ReFix LAN overlay modules.
#pragma once
#include <windows.h>
#include <string>

namespace refix::overlay {

inline std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

inline std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

inline std::wstring Trim(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return {};
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Lower-case file name of the game executable with non-alphanumerics replaced (same key as the
// emulator's instance slots), used to scope per-game locks.
inline std::wstring ExeKey() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* base = wcsrchr(path, L'\\');
    std::wstring key = base ? base + 1 : path;
    for (wchar_t& c : key) {
        c = (wchar_t)towlower(c);
        if (!iswalnum(c)) c = L'_';
    }
    return key;
}

} // namespace refix::overlay
