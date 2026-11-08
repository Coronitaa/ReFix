#include "csgo_handlers.h"
#include "csgo_proto.h"
#include "../../csgo_schema.h"
#include "../../csgo_item_defs.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace refix::csgo {

static refix::IRefixServices* s_services = nullptr;
static std::atomic<int> s_overrideMaxPlayers{ 0 };
static std::atomic<bool> s_hasActiveHostConfig{ false };
static std::mutex s_hookMutex;

using GetMaxPlayers2Fn = int(__thiscall*)(void* thisPtr, int gameType, int gameMode);
using GetMaxPlayers3Fn = int(__thiscall*)(void* thisPtr, int gameType, int gameMode, int skirmishId);

struct GameTypesHookSlot {
    void** vtableEntry = nullptr;
    void* original = nullptr;
    void* hookFn = nullptr;
};

static GameTypesHookSlot s_gameTypesHooks[3] = {}; // 0: server.dll, 1: matchmaking.dll, 2: client.dll

template <int SlotIdx>
static int __fastcall Hook_GetMaxPlayersForTypeAndMode2(void* thisPtr, void* /*edx*/, int gameType, int gameMode) {
    const int overrideSlots = s_overrideMaxPlayers.load();
    if (overrideSlots >= 2 && overrideSlots <= 64) {
        return overrideSlots;
    }
    if (auto orig = reinterpret_cast<GetMaxPlayers2Fn>(s_gameTypesHooks[SlotIdx].original)) {
        return orig(thisPtr, gameType, gameMode);
    }
    return 10;
}

template <int SlotIdx>
static int __fastcall Hook_GetMaxPlayersForTypeAndMode3(void* thisPtr, void* /*edx*/, int gameType, int gameMode, int skirmishId) {
    const int overrideSlots = s_overrideMaxPlayers.load();
    if (overrideSlots >= 2 && overrideSlots <= 64) {
        return overrideSlots;
    }
    if (auto orig = reinterpret_cast<GetMaxPlayers3Fn>(s_gameTypesHooks[SlotIdx].original)) {
        return orig(thisPtr, gameType, gameMode, skirmishId);
    }
    return 10;
}

static bool IsTwoArgGetMaxPlayers(const void* fnAddr) {
    if (!fnAddr) return false;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(fnAddr);
    bool sawRet8 = false;
    for (int i = 0; i < 240; ++i) {
        if (p[i] == 0xCC && p[i + 1] == 0xCC) break;
        if (p[i] == 0xC2 && p[i + 2] == 0x00) {
            if (p[i + 1] == 0x0C) return false; // ret 0Ch (3 int params)
            if (p[i + 1] == 0x08) sawRet8 = true; // ret 08h (2 int params)
        }
    }
    return sawRet8;
}

template <int SlotIdx>
static void HookModuleGameTypes(const char* moduleName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return;
    using CreateInterfaceFn = void* (__cdecl*)(const char* name, int* returnCode);
    auto createIf = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hMod, "CreateInterface"));
    if (!createIf) return;
    void* gameTypes = createIf("VENGINE_GAMETYPES_VERSION002", nullptr);
    if (!gameTypes) return;

    void** vtable = *reinterpret_cast<void***>(gameTypes);
    if (!vtable || !vtable[31]) return;
    void** entry = &vtable[31]; // IGameTypes::GetMaxPlayersForTypeAndMode
    if (s_gameTypesHooks[SlotIdx].hookFn && *entry == s_gameTypesHooks[SlotIdx].hookFn) {
        return;
    }
    const bool twoArgs = IsTwoArgGetMaxPlayers(*entry);
    void* targetHook = twoArgs
        ? reinterpret_cast<void*>(&Hook_GetMaxPlayersForTypeAndMode2<SlotIdx>)
        : reinterpret_cast<void*>(&Hook_GetMaxPlayersForTypeAndMode3<SlotIdx>);
    if (*entry == targetHook) return;

    DWORD oldProt = 0;
    if (VirtualProtect(entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
        s_gameTypesHooks[SlotIdx].vtableEntry = entry;
        s_gameTypesHooks[SlotIdx].original = *entry;
        s_gameTypesHooks[SlotIdx].hookFn = targetHook;
        *entry = targetHook;
        VirtualProtect(entry, sizeof(void*), oldProt, &oldProt);
        if (s_services && s_services->Log) {
            s_services->Log("[CSGO Mod] Hooked IGameTypes::GetMaxPlayersForTypeAndMode (%d args) in %s",
                            twoArgs ? 2 : 3, moduleName);
        }
    }
}

static void EnsureGameTypesHooksInstalled() {
    std::lock_guard<std::mutex> lock(s_hookMutex);
    HookModuleGameTypes<0>("server.dll");
    HookModuleGameTypes<1>("matchmaking.dll");
    HookModuleGameTypes<2>("client.dll");
}

static void SetTier0MaxPlayersOverride(int slots) {
    HMODULE hTier0 = GetModuleHandleA("tier0.dll");
    if (!hTier0) return;
    using CommandLineFn = void* (__cdecl*)();
    auto getCmdLine = reinterpret_cast<CommandLineFn>(GetProcAddress(hTier0, "CommandLine"));
    if (!getCmdLine) return;
    void* cmdLine = getCmdLine();
    if (!cmdLine) return;
    void** vtable = *reinterpret_cast<void***>(cmdLine);
    if (!vtable || !vtable[6]) return;
    // ICommandLine::AppendParm(const char* pszParm, const char* pszValues) is at vtable index 6.
    using AppendParmFn = void(__thiscall*)(void* thisPtr, const char* parm, const char* val);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", slots);
    reinterpret_cast<AppendParmFn>(vtable[6])(cmdLine, "-maxplayers_override", buf);
}

static std::string GetCsgoCfgDir() {
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    const size_t slash = dir.find_last_of("\\/");
    if (slash == std::string::npos) return "csgo\\cfg\\";
    return dir.substr(0, slash + 1) + "csgo\\cfg\\";
}

static void EnsureServerCfgHook(const std::string& cfgPath) {
    static const char* kHookLine = "exec refix_lan_host.cfg";
    std::string existing;
    {
        std::ifstream in(cfgPath, std::ios::binary);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            existing = ss.str();
        }
    }
    if (existing.find(kHookLine) != std::string::npos) return;
    std::ofstream out(cfgPath, std::ios::binary | std::ios::app);
    if (out) {
        if (!existing.empty() && existing.back() != '\n') out << "\r\n";
        out << kHookLine << "\r\n";
    }
}

void ApplyLanHostConfig(const char* jsonStr) {
    if (!jsonStr || !*jsonStr) return;
    nlohmann::json j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (!j.is_object()) return;

    auto num = [&](const char* key, double def) {
        double v = j.contains(key) && j[key].is_number() ? j[key].get<double>() : def;
        return (std::max)(-1.0e6, (std::min)(1.0e6, v));
    };
    auto str = [&](const char* key, const char* def) {
        return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(def);
    };

    const int slots = std::clamp((int)num("maxplayers", 10), 2, 64);
    const int maxRounds = std::clamp((int)num("maxrounds", 30), 1, 100);
    const double roundTime = std::clamp(num("roundtime", 1.92), 0.5, 60.0);
    const int warmupTime = std::clamp((int)num("warmuptime", 60), 0, 3600);
    const int startMoney = std::clamp((int)num("startmoney", 800), 0, 65535);
    const int maxMoney = std::clamp((int)num("maxmoney", 16000), 0, 65535);
    const int friendlyFire = (int)num("friendlyfire", 1) ? 1 : 0;
    const int bots = std::clamp((int)num("bot_quota", 9), 0, 63);
    const int botDifficulty = std::clamp((int)num("bot_difficulty", 2), 0, 3);
    const int cheats = (int)num("sv_cheats", 0) ? 1 : 0;
    const int overtime = (int)num("overtime", 0) ? 1 : 0;
    std::string password = str("password", "");
    password.erase(std::remove_if(password.begin(), password.end(), [](char c) { return c == '"' || c == ';' || c == '\n' || c == '\r'; }),
                   password.end());

    s_overrideMaxPlayers.store(slots);
    s_hasActiveHostConfig.store(true);
    SetTier0MaxPlayersOverride(slots);
    EnsureGameTypesHooksInstalled();

    const std::string cfgDir = GetCsgoCfgDir();
    CreateDirectoryA(cfgDir.c_str(), nullptr);

    std::ostringstream cfg;
    cfg << "// Generated by ReFix CS:GO LAN Mod\r\n"
        << "sv_lan 1\r\n"
        << "sv_visiblemaxplayers " << slots << "\r\n"
        << "mp_maxrounds " << maxRounds << "\r\n"
        << "mp_roundtime " << roundTime << "\r\n"
        << "mp_roundtime_defuse " << roundTime << "\r\n"
        << "mp_roundtime_hostage " << roundTime << "\r\n"
        << "mp_startmoney " << startMoney << "\r\n"
        << "mp_maxmoney " << maxMoney << "\r\n"
        << "mp_friendlyfire " << friendlyFire << "\r\n"
        << "mp_overtime_enable " << overtime << "\r\n"
        << "sv_cheats " << cheats << "\r\n"
        << "sv_password \"" << password << "\"\r\n"
        << "mp_limitteams 0\r\n"
        << "mp_autoteambalance 0\r\n"
        << "mp_humanteam any\r\n"
        << "bot_difficulty " << botDifficulty << "\r\n"
        << "bot_chatter normal\r\n"
        << "bot_join_after_player 1\r\n";
    if (warmupTime <= 0) {
        cfg << "mp_do_warmup_period 0\r\n"
            << "mp_do_warmup_offine 0\r\n"
            << "mp_warmuptime 0\r\n"
            << "mp_warmuptime_all_players_connected 0\r\n"
            << "mp_warmup_end\r\n";
    } else {
        cfg << "mp_do_warmup_period 1\r\n"
            << "mp_do_warmup_offine 1\r\n"
            << "mp_warmuptime " << warmupTime << "\r\n"
            << "mp_warmuptime_all_players_connected " << warmupTime << "\r\n"
            << "mp_warmup_pausetimer 0\r\n";
    }
    if (bots <= 0) {
        cfg << "bot_quota 0\r\n"
            << "bot_kick\r\n";
    } else {
        cfg << "bot_quota_mode normal\r\n"
            << "bot_quota " << bots << "\r\n";
    }

    {
        std::ofstream out(cfgDir + "refix_lan_host.cfg", std::ios::binary | std::ios::trunc);
        if (out) {
            const std::string text = cfg.str();
            out.write(text.data(), (std::streamsize)text.size());
        }
    }

    static const char* const kServerModeCfgs[] = {
        "gamemode_competitive_server.cfg",
        "gamemode_casual_server.cfg",
        "gamemode_competitive2v2_server.cfg",
        "gamemode_weapons_expert_server.cfg",
        "gamemode_deathmatch_server.cfg",
        "gamemode_armsrace_server.cfg",
        "gamemode_demolition_server.cfg",
        "gamemode_flyingscoutsman_server.cfg",
        "gamemode_cooperative_server.cfg",
        "gamemode_coopmission_server.cfg",
        "gamemode_survival_server.cfg",
        "op08_flying_scoutsman_server.cfg",
        "op08_stab_stab_zap_server.cfg",
        "op08_trigger_discipline_server.cfg",
        "op08_headshots_server.cfg",
        "op08_hunter_gatherers_server.cfg",
        "op08_heavy_assault_suit_server.cfg",
    };
    for (const char* name : kServerModeCfgs) {
        EnsureServerCfgHook(cfgDir + name);
    }

    if (s_services && s_services->Log) {
        s_services->Log("[CSGO Mod] Applied LAN Host config: slots=%d rounds=%d roundTime=%.2f warmup=%d startMoney=%d maxMoney=%d bots=%d ff=%d ot=%d cheats=%d",
                        slots, maxRounds, roundTime, warmupTime, startMoney, maxMoney, bots, friendlyFire, overtime, cheats);
    }
}

void EnforceActiveLanHostConfig() {
    EnsureGameTypesHooksInstalled();
    if (!s_hasActiveHostConfig.load()) return;
    if (s_services && s_services->engine) {
        s_services->engine->EnqueueCommand("exec refix_lan_host.cfg");
    }
}

void OnModPhaseEvent(int event_id) {
    EnsureGameTypesHooksInstalled();
    // ModPhase::GameServer_Init == 3
    if (event_id == 3) {
        EnforceActiveLanHostConfig();
    }
}

void ShutdownCSGOHandlers() {
    std::lock_guard<std::mutex> lock(s_hookMutex);
    for (auto& slot : s_gameTypesHooks) {
        if (slot.vtableEntry && slot.original) {
            DWORD oldProt = 0;
            if (VirtualProtect(slot.vtableEntry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
                *slot.vtableEntry = reinterpret_cast<void*>(slot.original);
                VirtualProtect(slot.vtableEntry, sizeof(void*), oldProt, &oldProt);
            }
            slot.vtableEntry = nullptr;
            slot.original = nullptr;
            slot.hookFn = nullptr;
        }
    }
}

void RegisterCSGOHandlers(refix::IRefixServices* services) {
    s_services = services;
    EnsureGameTypesHooksInstalled();

    if (services && services->Log) {
        services->Log("[CSGO Mod] LAN Host Enforcer & GC bridge active");
    }
}

} // namespace refix::csgo
