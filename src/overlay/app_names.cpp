#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "app_names.h"
#include "achievements.h"
#include "mini_json.h"
#include "overlay_text.h"
#include "profile_store.h"

#include <windows.h>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace refix::overlay::apps {
namespace {

constexpr size_t kMaxAnswer = 256 * 1024;

std::mutex g_mu;
std::map<uint32_t, std::string> g_names;   // "" = known to have no name
std::set<uint32_t> g_pending;

std::wstring CacheFile() { return profiles::RootDir() + L"\\AppNames.ini"; }

bool ReadCache(uint32_t appId, std::string& out) {
    wchar_t buf[256] = {};
    GetPrivateProfileStringW(L"Names", std::to_wstring(appId).c_str(), L"\x01", buf, 256, CacheFile().c_str());
    if (buf[0] == L'\x01') return false;
    out = ToUtf8(buf);
    return true;
}

void Lookup(uint32_t appId) {
    std::string body, name;
    const std::wstring path = L"/api/appdetails?filters=basic&appids=" + std::to_wstring(appId);
    if (ach::WebGet(L"store.steampowered.com", path, kMaxAnswer, body)) {
        const json::Value root = json::Parse(body);
        const json::Value& app = root[std::to_string(appId).c_str()];
        if (app.Bool("success")) name = app["data"].Str("name");
        WritePrivateProfileStringW(L"Names", std::to_wstring(appId).c_str(), FromUtf8(name).c_str(), CacheFile().c_str());
    }
    std::lock_guard<std::mutex> lock(g_mu);
    g_names[appId] = name;
    g_pending.erase(appId);
}

} // namespace

std::string Name(uint32_t appId) {
    if (appId == 0) return {};
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_names.find(appId);
        if (it != g_names.end()) return it->second;
        if (g_pending.count(appId)) return {};
    }
    std::string cached;
    if (ReadCache(appId, cached)) {
        std::lock_guard<std::mutex> lock(g_mu);
        g_names[appId] = cached;
        return cached;
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_pending.insert(appId);
    }
    std::thread(Lookup, appId).detach();
    return {};
}

} // namespace refix::overlay::apps
