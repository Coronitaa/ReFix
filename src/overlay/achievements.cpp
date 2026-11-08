#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "achievements.h"
#include "mini_json.h"
#include "overlay_text.h"
#include "profile_store.h"
#include "refix_overlay.h"
#include "../unreal_steam_emu.h"

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::ach {
namespace {

constexpr ULONGLONG kSchemaMaxAgeMs = 3ull * 24 * 3600 * 1000;   // refresh the rarity every few days
constexpr DWORD kFirstFetchWaitMs = 6000;                        // game thread, first run without cache only
constexpr int kHttpTimeoutMs = 8000;
constexpr size_t kMaxSchemaBytes = 8u << 20;
constexpr size_t kMaxIconBytes = 1u << 20;
constexpr size_t kMaxEvents = 32;
const char* const kApiLang[kLangCount] = { "english", "spanish" };

struct StatState {
    std::map<std::string, int32_t> ints;
    std::map<std::string, float> floats;
};

struct AchState {
    bool unlocked = false;
    uint32_t unlockTime = 0;
    uint32_t progress = 0, progressMax = 0, progressTime = 0;
};

std::mutex g_mu;
bool g_loaded = false;
uint32_t g_appId = 0;
std::wstring g_stateFile;                    // stats.ini of the profile (empty: memory only)
bool g_dirty = false;                        // stats changed since the last save
std::wstring g_cacheDir;
std::vector<Def> g_defs;
std::map<std::string, size_t> g_index;       // api -> g_defs
std::map<std::string, AchState> g_state;
StatState g_stats;
uint64_t g_version = 1;
std::deque<Event> g_events;
HANDLE g_schemaEvent = nullptr;              // set when the first fetch is over

// Icon downloads.
std::mutex g_iconMu;
std::condition_variable g_iconCv;
std::deque<std::string> g_iconQueue;
std::set<std::string> g_iconRequested;
bool g_iconThread = false;

uint32_t Now() { return (uint32_t)::time(nullptr); }

void EnsureDirs(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == L'\\') CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
    }
}

bool ReadFileText(const std::wstring& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Write to a temporary file and swap it in, so a crash never leaves half a file.
bool WriteFileAtomic(const std::wstring& path, const std::string& data) {
    const std::wstring tmp = path + L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), (std::streamsize)data.size());
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

// One WinHTTP session for the process, thread-safe, keeps connections alive between requests.
HINTERNET Session() {
    static HINTERNET s = [] {
        HINTERNET h = WinHttpOpen(L"ReFix/2.2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (h) {
            WinHttpSetTimeouts(h, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs);
            // Networks with broken IPv6 made every new connection wait ~40 s before trying IPv4.
            BOOL fallback = TRUE;
            WinHttpSetOption(h, WINHTTP_OPTION_IPV6_FAST_FALLBACK, &fallback, sizeof(fallback));
        }
        return h;
    }();
    return s;
}

bool HttpGet(const std::wstring& host, const std::wstring& path, size_t maxBytes, std::string& out) {
    HINTERNET s = Session();
    if (!s) return false;
    bool ok = false;
    HINTERNET c = WinHttpConnect(s, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                    : nullptr;
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, len = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &len, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            ok = true;
            out.clear();
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(r, &avail)) { ok = false; break; }
                if (avail == 0) break;
                if (out.size() + avail > maxBytes) { ok = false; break; }
                const size_t at = out.size();
                out.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(r, &out[at], avail, &got)) { ok = false; break; }
                out.resize(at + got);
            }
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    return ok;
}

std::wstring SchemaFile(const std::wstring& dir, int lang) { return dir + L"\\" + FromUtf8(kApiLang[lang]) + L".json"; }

std::wstring ExeDir() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    const size_t slash = dir.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : dir.substr(0, slash + 1);
}

void ParseUtf16VdfTokens(const std::wstring& path,
                         std::map<std::string, std::string>& outTokens,
                         std::vector<std::string>* outDescOrder = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    file.seekg(0, std::ios::end);
    const size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 2) return;
    std::vector<char> raw(size);
    file.read(raw.data(), (std::streamsize)size);
    const wchar_t* p = reinterpret_cast<const wchar_t*>(raw.data());
    size_t wlen = size / 2;
    if (wlen > 0 && p[0] == 0xFEFF) { ++p; --wlen; }

    auto toUtf8 = [](const wchar_t* ws, int len) -> std::string {
        if (len <= 0) return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, ws, len, nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws, len, &s[0], n, nullptr, nullptr);
        return s;
    };

    size_t i = 0;
    while (i < wlen) {
        while (i < wlen && (p[i] == L' ' || p[i] == L'\t' || p[i] == L'\r' || p[i] == L'\n')) ++i;
        if (i >= wlen) break;
        if (p[i] == L'/' && i + 1 < wlen && p[i + 1] == L'/') {
            while (i < wlen && p[i] != L'\n') ++i;
            continue;
        }
        if (p[i] == L'"') {
            ++i;
            const size_t kStart = i;
            while (i < wlen && p[i] != L'"' && p[i] != L'\n') ++i;
            const size_t kLen = i - kStart;
            if (i < wlen && p[i] == L'"') ++i;
            while (i < wlen && (p[i] == L' ' || p[i] == L'\t')) ++i;
            if (i < wlen && p[i] == L'"') {
                ++i;
                const size_t vStart = i;
                while (i < wlen && p[i] != L'"' && p[i] != L'\n') {
                    if (p[i] == L'\\' && i + 1 < wlen) i += 2;
                    else ++i;
                }
                const size_t vLen = i - vStart;
                if (i < wlen && p[i] == L'"') ++i;
                std::string key = toUtf8(p + kStart, (int)kLen);
                for (char& c : key) c = (char)toupper((unsigned char)c);
                const bool isName = key.size() > 5 && key.compare(key.size() - 5, 5, "_NAME") == 0;
                const bool isDesc = key.size() > 5 && key.compare(key.size() - 5, 5, "_DESC") == 0;
                if (isName || isDesc) {
                    const bool validChars = std::all_of(key.begin(), key.end(), [](char c) {
                        return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
                    });
                    if (validChars) {
                        outTokens[key] = toUtf8(p + vStart, (int)vLen);
                        if (isDesc && outDescOrder) {
                            outDescOrder->push_back(key.substr(0, key.size() - 5));
                        }
                    }
                }
            }
        } else {
            ++i;
        }
    }
}

bool LoadCsgoLocalSchema(std::vector<Def>& defs) {
    const std::wstring base = ExeDir();
    const std::wstring enPath = base + L"csgo\\resource\\csgo_english.txt";
    if (GetFileAttributesW(enPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    std::map<std::string, std::string> enTokens, esTokens;
    std::vector<std::string> order;
    ParseUtf16VdfTokens(enPath, enTokens, &order);
    ParseUtf16VdfTokens(base + L"csgo\\resource\\csgo_spanish.txt", esTokens, nullptr);

    const std::wstring panoIcons = base + L"csgo\\materials\\panorama\\images\\icons\\achievements\\";
    const std::wstring flashIcons = base + L"csgo\\resource\\flash\\images\\achievements\\";

    defs.clear();
    std::set<std::string> seen;
    for (const std::string& api : order) {
        if (!seen.insert(api).second) continue;
        // Cut beta achievements not in CS:GO's 167 achievement list.
        if (api == "BOMB_DEFUSE_NEEDED_KIT" || api == "BREAK_PROPS" || api == "GUN_GAME_SELECT_SUICIDE_WITH_KNIFE") continue;

        auto itName = enTokens.find(api + "_NAME");
        auto itDesc = enTokens.find(api + "_DESC");
        if (itName == enTokens.end() || itDesc == enTokens.end()) continue;

        std::string lower = api;
        for (char& c : lower) c = (char)tolower((unsigned char)c);
        const std::string iconFile = lower + ".jpg";
        const std::wstring wIcon = FromUtf8(iconFile);
        if (GetFileAttributesW((panoIcons + wIcon).c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW((flashIcons + wIcon).c_str()) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }

        Def d;
        d.api = api;
        d.icon = iconFile;
        d.iconGray = iconFile;
        d.hidden = false;
        d.percent = -1.0f;
        d.name[kEnglish] = itName->second;
        d.desc[kEnglish] = itDesc->second;
        auto itEsName = esTokens.find(api + "_NAME");
        auto itEsDesc = esTokens.find(api + "_DESC");
        d.name[kSpanish] = (itEsName != esTokens.end() && !itEsName->second.empty()) ? itEsName->second : d.name[kEnglish];
        d.desc[kSpanish] = (itEsDesc != esTokens.end() && !itEsDesc->second.empty()) ? itEsDesc->second : d.desc[kEnglish];
        defs.push_back(std::move(d));
    }
    return !defs.empty();
}

// Builds the definitions from the cached answers of IPlayerService/GetGameAchievements. False when there is
// no usable cache (an empty list is a valid cache: the game has no achievements).
bool ParseSchema(const std::wstring& dir, std::vector<Def>& defs) {
    defs.clear();
    std::map<std::string, size_t> at;
    for (int lang = 0; lang < kLangCount; ++lang) {
        std::string text;
        if (!ReadFileText(SchemaFile(dir, lang), text)) {
            if (lang == kEnglish) return false;
            continue;
        }
        const json::Value root = json::Parse(text);
        if (root["response"].type != json::Value::Type::Object) {
            if (lang == kEnglish) return false;
            continue;
        }
        const json::Value& list = root["response"]["achievements"];   // absent: the game has none
        for (const json::Value& a : list.items) {
            const std::string api = a.Str("internal_name");
            if (api.empty()) continue;
            auto it = at.find(api);
            if (it == at.end()) {
                if (lang != kEnglish) continue;
                Def d;
                d.api = api;
                d.icon = a.Str("icon");
                d.iconGray = a.Str("icon_gray");
                d.hidden = a.Bool("hidden");
                d.percent = (float)a.Num("player_percent_unlocked", -1.0);
                d.minProgress = (int)a.Num("min_progress_int", 0);
                d.maxProgress = (int)a.Num("max_progress_int", 0);
                it = at.emplace(api, defs.size()).first;
                defs.push_back(std::move(d));
            }
            defs[it->second].name[lang] = a.Str("localized_name");
            defs[it->second].desc[lang] = a.Str("localized_desc");
        }
    }
    return true;
}

// Under g_mu.
void InstallSchema(std::vector<Def> defs) {
    g_defs = std::move(defs);
    g_index.clear();
    for (size_t i = 0; i < g_defs.size(); ++i) g_index[g_defs[i].api] = i;
    ++g_version;
}

bool CacheFresh(const std::wstring& dir) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(SchemaFile(dir, kEnglish).c_str(), GetFileExInfoStandard, &a)) return false;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const ULONGLONG t = ((ULONGLONG)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime;
    const ULONGLONG n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
    return n > t && (n - t) / 10000 < kSchemaMaxAgeMs;
}

void FetchSchema(uint32_t appId, std::wstring dir) {
    bool any = false;
    for (int lang = 0; lang < kLangCount; ++lang) {
        const std::wstring path = L"/IPlayerService/GetGameAchievements/v1/?appid=" + std::to_wstring(appId) +
                                  L"&language=" + FromUtf8(kApiLang[lang]);
        std::string body;
        if (!HttpGet(L"api.steampowered.com", path, kMaxSchemaBytes, body)) {
            ReFixLog("[Achievements] Schema download failed (app %u, %s)", appId, kApiLang[lang]);
            break;
        }
        // Keep the cache only when the answer is a real list (an empty response means no achievements).
        if (json::Parse(body)["response"].type != json::Value::Type::Object) break;
        if (WriteFileAtomic(SchemaFile(dir, lang), body)) any = true;
    }
    if (any) {
        std::vector<Def> defs;
        if (ParseSchema(dir, defs)) {
            std::lock_guard<std::mutex> lock(g_mu);
            InstallSchema(std::move(defs));
            ReFixLog("[Achievements] Schema of app %u: %zu achievements", appId, g_defs.size());
        }
    }
    SetEvent(g_schemaEvent);
}

// stats.ini of the profile's StatsFolder, the format ReFix used before the overlay knew about achievements:
// [IntStats] [FloatStats] [Achievements] name=unlock time, plus [AchievementProgress] name=cur/max/time.
void SaveStateLocked() {
    g_dirty = false;
    if (g_stateFile.empty()) return;
    std::string out = "[IntStats]\n";
    char buf[96];
    for (const auto& [name, v] : g_stats.ints) {
        snprintf(buf, sizeof(buf), "=%d\n", v);
        out += name + buf;
    }
    out += "\n[FloatStats]\n";
    for (const auto& [name, v] : g_stats.floats) {
        snprintf(buf, sizeof(buf), "=%.9g\n", v);
        out += name + buf;
    }
    out += "\n[Achievements]\n";
    for (const auto& [name, st] : g_state) {
        if (!st.unlocked) continue;
        snprintf(buf, sizeof(buf), "=%u\n", st.unlockTime);
        out += name + buf;
    }
    out += "\n[AchievementProgress]\n";
    for (const auto& [name, st] : g_state) {
        if (!st.progressMax) continue;
        snprintf(buf, sizeof(buf), "=%u/%u/%u\n", st.progress, st.progressMax, st.progressTime);
        out += name + buf;
    }
    if (!WriteFileAtomic(g_stateFile, out)) ReFixLog("[Achievements] Could not save %s", ToUtf8(g_stateFile).c_str());
}

std::string TrimAscii(const std::string& v) {
    const size_t b = v.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    return v.substr(b, v.find_last_not_of(" \t\r") - b + 1);
}

void LoadStateLocked() {
    std::string text;
    if (g_stateFile.empty() || !ReadFileText(g_stateFile, text)) return;
    std::istringstream in(text);
    std::string line, section;
    while (std::getline(in, line)) {
        line = TrimAscii(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = TrimAscii(line.substr(0, eq));
        const std::string val = TrimAscii(line.substr(eq + 1));
        if (key.empty()) continue;
        if (section == "IntStats") {
            g_stats.ints[key] = (int32_t)strtol(val.c_str(), nullptr, 10);
        } else if (section == "FloatStats") {
            g_stats.floats[key] = strtof(val.c_str(), nullptr);
        } else if (section == "Achievements") {
            AchState& st = g_state[key];
            st.unlocked = true;
            st.unlockTime = (uint32_t)strtoul(val.c_str(), nullptr, 10);
        } else if (section == "AchievementProgress") {
            AchState& st = g_state[key];
            unsigned cur = 0, max = 0, t = 0;
            if (sscanf_s(val.c_str(), "%u/%u/%u", &cur, &max, &t) >= 2 && max) {
                st.progress = cur;
                st.progressMax = max;
                st.progressTime = t;
            }
        }
    }
}

// Loads the profile's state and the schema the first time anything asks. `waitForSchema`: the game thread may
// wait a few seconds for the very first download, so GetNumAchievements is right from the start.
void Ensure(bool waitForSchema) {
    HANDLE wait = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!g_loaded) {
            g_loaded = true;
            g_appId = UnrealSteamEmu::GetAppID();
            const std::wstring folder = g_appId ? ReFixOverlay::StatsFolder(g_appId) : std::wstring();
            if (!folder.empty()) g_stateFile = folder + L"\\stats.ini";
            LoadStateLocked();
            // Merge CS:GO stats across classic (730) and standalone legacy (4465480) AppIDs if present.
            if (g_appId == 730 || g_appId == 4465480) {
                const uint32_t altAppId = (g_appId == 730) ? 4465480u : 730u;
                const std::wstring altFolder = ReFixOverlay::StatsFolder(altAppId);
                if (!altFolder.empty()) {
                    const std::wstring savedFile = g_stateFile;
                    g_stateFile = altFolder + L"\\stats.ini";
                    LoadStateLocked();
                    g_stateFile = savedFile;
                }
            }
            g_schemaEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            std::vector<Def> csgoDefs;
            if (LoadCsgoLocalSchema(csgoDefs)) {
                InstallSchema(std::move(csgoDefs));
                SetEvent(g_schemaEvent);
                ReFixLog("[Achievements] Loaded %zu local CS:GO achievements", g_defs.size());
            } else if (g_appId != 0 && g_appId != 480) {
                g_cacheDir = profiles::RootDir() + L"\\Achievements\\" + std::to_wstring(g_appId);
                std::vector<Def> defs;
                const bool cached = ParseSchema(g_cacheDir, defs);
                if (cached) InstallSchema(std::move(defs));
                if (!cached || !CacheFresh(g_cacheDir)) {
                    EnsureDirs(g_cacheDir);
                    std::thread(FetchSchema, g_appId, g_cacheDir).detach();
                } else {
                    SetEvent(g_schemaEvent);
                }
                if (!cached && waitForSchema) wait = g_schemaEvent;
            } else {
                SetEvent(g_schemaEvent);
            }
            ReFixLog("[Achievements] App %u, state %s", g_appId,
                     g_stateFile.empty() ? "in memory" : ToUtf8(g_stateFile).c_str());
        }
    }
    if (wait) WaitForSingleObject(wait, kFirstFetchWaitMs);
}

void PushEvent(Event e) {
    g_events.push_back(std::move(e));
    while (g_events.size() > kMaxEvents) g_events.pop_front();
}

const Def* FindDef(const std::string& api) {
    auto it = g_index.find(api);
    return it == g_index.end() ? nullptr : &g_defs[it->second];
}

// File names from the Web API become paths: only plain names.
bool SafeIconName(const std::string& n) {
    if (n.empty() || n.size() > 128 || n.find("..") != std::string::npos) return false;
    return std::all_of(n.begin(), n.end(), [](char c) { return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.'; });
}

void IconWorker() {
    for (;;) {
        std::string icon;
        {
            std::unique_lock<std::mutex> lock(g_iconMu);
            g_iconCv.wait(lock, [] { return !g_iconQueue.empty(); });
            icon = g_iconQueue.front();
            g_iconQueue.pop_front();
        }
        uint32_t appId;
        std::wstring dir;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            appId = g_appId;
            dir = g_cacheDir + L"\\icons";
        }
        std::string body;
        // Steam serves every game's icons from community_assets; older games also from the legacy path.
        const std::wstring tail = std::to_wstring(appId) + L"/" + FromUtf8(icon);
        const bool got = HttpGet(L"shared.akamai.steamstatic.com", L"/community_assets/images/apps/" + tail, kMaxIconBytes, body) ||
                         HttpGet(L"cdn.akamai.steamstatic.com", L"/steamcommunity/public/images/apps/" + tail, kMaxIconBytes, body);
        if (got && !body.empty()) {
            EnsureDirs(dir);
            WriteFileAtomic(dir + L"\\" + FromUtf8(icon), body);
            std::lock_guard<std::mutex> lock(g_mu);
            ++g_version;
        }
    }
}

} // namespace

Lang LangFromSteam(const char* l) {
    if (!l) return kEnglish;
    return (_strnicmp(l, "spanish", 7) == 0 || _stricmp(l, "latam") == 0) ? kSpanish : kEnglish;
}

bool GetAchievement(const char* name, bool* achieved, uint32_t* unlockTime) {
    if (!name || !achieved) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_state.find(name);
    *achieved = it != g_state.end() && it->second.unlocked;
    if (unlockTime) *unlockTime = *achieved ? it->second.unlockTime : 0;
    return true;   // unknown names succeed too: without a schema every name is accepted
}

bool SetAchievement(const char* name) {
    if (!name || !name[0]) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    AchState& s = g_state[name];
    if (s.unlocked) return false;
    s.unlocked = true;
    s.unlockTime = Now();
    ++g_version;
    PushEvent({ name, true, 0, 0 });
    SaveStateLocked();
    return true;
}

bool ClearAchievement(const char* name) {
    if (!name) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_state.find(name);
    if (it == g_state.end() || !it->second.unlocked) return true;
    it->second.unlocked = false;
    it->second.unlockTime = 0;
    ++g_version;
    SaveStateLocked();
    return true;
}

bool IndicateProgress(const char* name, uint32_t cur, uint32_t max) {
    if (!name || !name[0] || max == 0) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    AchState& s = g_state[name];
    if (s.unlocked) return false;   // Steam ignores progress of an unlocked achievement
    s.progress = std::min(cur, max);
    s.progressMax = max;
    s.progressTime = Now();
    ++g_version;
    PushEvent({ name, false, s.progress, max });
    SaveStateLocked();
    return true;
}

bool GetStat(const char* name, int32_t* value) {
    if (!name || !value) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_stats.ints.find(name);
    *value = it == g_stats.ints.end() ? 0 : it->second;
    return true;
}

bool GetStat(const char* name, float* value) {
    if (!name || !value) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_stats.floats.find(name);
    *value = it == g_stats.floats.end() ? 0.0f : it->second;
    return true;
}

bool SetStat(const char* name, int32_t value) {
    if (!name) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_stats.ints.find(name);
    if (it == g_stats.ints.end() || it->second != value) {
        g_stats.ints[name] = value;
        g_dirty = true;
    }
    return true;
}

bool SetStat(const char* name, float value) {
    if (!name) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_stats.floats.find(name);
    if (it == g_stats.floats.end() || it->second != value) {
        g_stats.floats[name] = value;
        g_dirty = true;
    }
    return true;
}

bool Store() {
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    SaveStateLocked();
    return true;
}

void Flush() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_loaded && g_dirty) SaveStateLocked();
}

void ResetAll(bool achievementsToo) {
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    g_stats = StatState();
    if (achievementsToo) g_state.clear();
    ++g_version;
    SaveStateLocked();
}

uint32_t Count() {
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    return (uint32_t)g_defs.size();
}

std::string NameAt(uint32_t index) {
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    return index < g_defs.size() ? g_defs[index].api : std::string();
}

std::string Attribute(const char* name, const char* key, const char* steamLanguage) {
    if (!name || !key) return {};
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    const Def* d = FindDef(name);
    if (!d) return {};
    const int lang = LangFromSteam(steamLanguage);
    auto pick = [&](const std::string* v) { return v[lang].empty() ? v[kEnglish] : v[lang]; };
    if (_stricmp(key, "name") == 0) return pick(d->name);
    if (_stricmp(key, "desc") == 0) return pick(d->desc);
    if (_stricmp(key, "hidden") == 0) return d->hidden ? "1" : "0";
    return {};
}

bool Percent(const char* name, float* percent) {
    if (!name || !percent) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    const Def* d = FindDef(name);
    if (!d || d->percent < 0) return false;
    *percent = d->percent;
    return true;
}

bool ProgressLimits(const char* name, int32_t* minProgress, int32_t* maxProgress) {
    if (!name) return false;
    Ensure(true);
    std::lock_guard<std::mutex> lock(g_mu);
    const Def* d = FindDef(name);
    if (!d || d->maxProgress <= 0) return false;
    if (minProgress) *minProgress = d->minProgress;
    if (maxProgress) *maxProgress = d->maxProgress;
    return true;
}

uint64_t Version() {
    Ensure(false);
    std::lock_guard<std::mutex> lock(g_mu);
    return g_version;
}

std::vector<Entry> Snapshot() {
    Ensure(false);
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<Entry> out;
    out.reserve(g_defs.size());
    auto fill = [](Entry& e, const AchState& s) {
        e.unlocked = s.unlocked;
        e.unlockTime = s.unlockTime;
        e.progress = s.progress;
        e.progressMax = s.progressMax;
        e.progressTime = s.progressTime;
    };
    for (const Def& d : g_defs) {
        Entry e;
        e.def = d;
        e.inSchema = true;
        auto it = g_state.find(d.api);
        if (it != g_state.end()) fill(e, it->second);
        out.push_back(std::move(e));
    }
    // Names the schema does not know are shown only when there is a schema: a game with no Steam achievements
    // (CS:GO under its own AppID) keeps its internal names out of the overlay.
    for (const auto& [name, s] : g_state) {
        if (g_defs.empty()) break;
        if (g_index.count(name) || (!s.unlocked && !s.progressMax)) continue;
        Entry e;
        e.def.api = name;
        fill(e, s);
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<Event> TakeEvents() {
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<Event> out(g_events.begin(), g_events.end());
    g_events.clear();
    return out;
}

bool SchemaReady() {
    Ensure(false);
    std::lock_guard<std::mutex> lock(g_mu);
    return !g_defs.empty();
}

std::wstring IconFile(const std::string& icon) {
    if (!SafeIconName(icon)) return {};
    const std::wstring base = ExeDir();
    const std::wstring wIcon = FromUtf8(icon);
    const std::wstring panoPath = base + L"csgo\\materials\\panorama\\images\\icons\\achievements\\" + wIcon;
    if (GetFileAttributesW(panoPath.c_str()) != INVALID_FILE_ATTRIBUTES) return panoPath;
    const std::wstring flashPath = base + L"csgo\\resource\\flash\\images\\achievements\\" + wIcon;
    if (GetFileAttributesW(flashPath.c_str()) != INVALID_FILE_ATTRIBUTES) return flashPath;

    std::wstring dir;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_cacheDir.empty()) return {};
        dir = g_cacheDir + L"\\icons\\";
    }
    const std::wstring path = dir + wIcon;
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
    std::lock_guard<std::mutex> lock(g_iconMu);
    if (g_iconRequested.insert(icon).second) {
        g_iconQueue.push_back(icon);
        if (!g_iconThread) {
            g_iconThread = true;
            std::thread(IconWorker).detach();
        }
        g_iconCv.notify_one();
    }
    return {};
}

} // namespace refix::overlay::ach
