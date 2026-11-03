#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_i18n.h"
#include "profile_store.h"

#include <windows.h>
#include <atomic>

namespace refix::overlay::i18n {
namespace {

std::atomic<int> g_lang{-1};   // -1 = not read yet

std::wstring SettingsIni() { return profiles::RootDir() + L"\\profiles.ini"; }

} // namespace

Lang Current() {
    int v = g_lang.load();
    if (v < 0) {
        wchar_t buf[8] = {};
        GetPrivateProfileStringW(L"General", L"Language", L"en", buf, 8, SettingsIni().c_str());
        v = _wcsicmp(buf, L"es") == 0 ? (int)Lang::Es : (int)Lang::En;
        g_lang = v;
    }
    return (Lang)v;
}

void Set(Lang lang) {
    g_lang = (int)lang;
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    WritePrivateProfileStringW(L"General", L"Language", lang == Lang::Es ? L"es" : L"en", SettingsIni().c_str());
}

} // namespace refix::overlay::i18n
