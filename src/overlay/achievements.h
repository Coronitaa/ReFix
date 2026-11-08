// Steam achievements and stats of the running game, per ReFix profile.
//
// State (unlocked achievements with their time, progress and stats) is saved in the stats.ini of
// ReFixOverlay::StatsFolder: one per profile (guests: a temporary folder; no profile: one for this PC).
// The schema (names and descriptions in English and Spanish, icons, hidden flag, progress limits and the share
// of Steam players who have each one) comes from Steam's public Web API, cached in
// %APPDATA%\ReFix\Achievements\<AppID>\ and refreshed in the background every few days. Offline and with no
// cache the game still records achievements by their API name; the overlay shows them once the schema arrives.
//
// The emulator's ISteamUserStats calls the game-side functions (any thread); the overlay reads Snapshot() and
// TakeEvents() on its host thread. Both see the same in-process store, so the overlay is always in step.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace refix::overlay::ach {

enum Lang { kEnglish = 0, kSpanish = 1, kLangCount = 2 };

struct Def {
    std::string api;                 // API name the game uses
    std::string name[kLangCount];    // UTF-8, empty when unknown
    std::string desc[kLangCount];
    std::string icon;                // file names on Steam's CDN
    std::string iconGray;
    bool hidden = false;
    float percent = -1.0f;           // share of Steam players who unlocked it, -1 unknown
    int minProgress = 0;
    int maxProgress = 0;             // 0: no progress bar
};

struct Entry {
    Def def;
    bool inSchema = false;           // false: unlocked by the game but unknown to the schema
    bool unlocked = false;
    uint32_t unlockTime = 0;         // Unix time
    uint32_t progress = 0;           // last IndicateAchievementProgress
    uint32_t progressMax = 0;
    uint32_t progressTime = 0;
    uint32_t ActivityTime() const { return unlocked ? unlockTime : progressTime; }
};

struct Event {
    std::string api;
    bool unlocked = false;           // false: progress update
    uint32_t progress = 0;
    uint32_t progressMax = 0;
};

// ---- game side (emulator, any thread) ----
bool GetAchievement(const char* name, bool* achieved, uint32_t* unlockTime);
bool SetAchievement(const char* name);          // true when it was newly unlocked
bool ClearAchievement(const char* name);
bool IndicateProgress(const char* name, uint32_t cur, uint32_t max);
bool GetStat(const char* name, int32_t* value);
bool GetStat(const char* name, float* value);
bool SetStat(const char* name, int32_t value);
bool SetStat(const char* name, float value);
bool Store();                                   // StoreStats: writes the profile's file
void Flush();                                   // shutdown: writes stats changed since the last Store
void ResetAll(bool achievementsToo);
uint32_t Count();
std::string NameAt(uint32_t index);
// GetAchievementDisplayAttribute: "name", "desc", "hidden" in the game's language.
std::string Attribute(const char* name, const char* key, const char* steamLanguage);
bool Percent(const char* name, float* percent);
bool ProgressLimits(const char* name, int32_t* minProgress, int32_t* maxProgress);

// ---- overlay side (host thread) ----
uint64_t Version();                             // changes whenever Snapshot() would
std::vector<Entry> Snapshot();                  // schema order; unknown unlocked ones at the end
std::vector<Event> TakeEvents();
bool SchemaReady();
// Local copy of an icon; empty while it downloads (or when it cannot).
std::wstring IconFile(const std::string& icon);

Lang LangFromSteam(const char* steamLanguage);

} // namespace refix::overlay::ach
