#include "overlay/refix_overlay.h"
#include <ctime>
#include "refix_pa_services.h"
#include "refix_pa_gql.h"
#include "refix_pa_rooms.h"
#include "refix_steam_flat.h"
#include "refix_pa_backend.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <string>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPAServices {

namespace {

constexpr size_t kMaxDisplayNameBytes = 64;
std::mutex g_profileMutex;

std::string ProfilePath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix_Profile.ini";
}

std::string StoredDisplayNameInternal() {
    std::lock_guard<std::mutex> lock(g_profileMutex);
    char buf[256] = { 0 };
    GetPrivateProfileStringA("Profile", "DisplayName", "", buf, sizeof(buf), ProfilePath().c_str());
    return buf;
}

// --- Handlers --------------------------------------------------------------------------------------

Result ChangeDisplayName(const std::string& operation, const std::string& query) {
    Result r;
    std::string name = ReFixPAGql::StringArg(query, "newName");
    if (name.empty() || name.size() > kMaxDisplayNameBytes) {
        ReFixLog("[PA-Services] changeDisplayName: missing/invalid newName in '%s'", query.c_str());
        return r;
    }
    {
        std::lock_guard<std::mutex> lock(g_profileMutex);
        WritePrivateProfileStringA("Profile", "DisplayName", name.c_str(), ProfilePath().c_str());
    }
    ReFixLog("[PA-Services] changeDisplayName -> '%s' (saved to ReFix_Profile.ini)", name.c_str());
    r.handled = true;
    r.body = ReFixPAGql::Success(operation, "{\"newName\":" + ReFixPAGql::JsonString(name) + ",\"finance\":{\"timestamp\":0}}");
    return r;
}

std::string StoreCatalogPath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix_StoreCatalog.json";
}

std::string LoadCachedStoreCatalog() {
    std::ifstream f(StoreCatalogPath(), std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    return s.empty() ? "" : s;
}

std::string StoreResponse(const std::string& operation) {
    std::string cached = LoadCachedStoreCatalog();
    if (!cached.empty() && cached.find("\"goods\"") != std::string::npos) {
        ReFixLog("[PA-Services] Serving cached store catalog (%zu bytes) for %s", cached.size(), operation.c_str());
        return ReFixPAGql::Success(operation.empty() ? "store" : operation, cached);
    }
    ReFixLog("[PA-Services] Serving valid fallback store catalog for %s", operation.c_str());
    return ReFixPAGql::Success(operation.empty() ? "store" : operation,
        "{\"code\":0,\"ok\":true,\"refreshTime\":2147483647,\"categories\":[{\"id\":\"featured\",\"name\":\"Featured\"}],\"goods\":[],\"items\":[],\"packages\":[],\"recommend\":[]}");
}

std::string TeamResponse(const std::string& operation, const std::string& query) {
    uint64_t steamId = ReFixSteamFlat::LocalSteamID();
    std::string name = StoredDisplayNameInternal();
    if (name.empty()) name = "Player";
    std::string fakeId = "fake_" + std::to_string(steamId);

    ReFixLog("[PA-Services] Serving team/party operation '%s'", operation.c_str());
    std::string teamData = "{\"teamId\":\"" + std::to_string(steamId) + "\""
                           ",\"partyId\":\"" + std::to_string(steamId) + "\""
                           ",\"leaderId\":\"" + fakeId + "\""
                           ",\"ownerId\":\"" + fakeId + "\""
                           ",\"status\":0"
                           ",\"isLeader\":true"
                           ",\"maxMembers\":4"
                           ",\"members\":[{"
                               "\"accountId\":\"" + fakeId + "\""
                               ",\"userId\":\"" + fakeId + "\""
                               ",\"displayName\":\"" + ReFixPAGql::JsonEscape(name) + "\""
                               ",\"isLeader\":true"
                               ",\"isReady\":true"
                               ",\"seatIndex\":0"
                           "}]"
                           ",\"code\":0"
                           ",\"ok\":true}";
    return ReFixPAGql::Success(operation.empty() ? "team" : operation, teamData);
}

// Service URLs handed to the game. Their host matches the backend host filter, so UriHelper.NewUri sends
// them to the local backend, which routes by GraphQL operation rather than by path. Asset/telemetry/
// websocket URLs stay unset so those features keep behaving as before (unreachable).
const char* const kServiceUrlFields[] = {
    "logicUrl", "friendUrl", "rewardUrl", "accountInfoUrl", "storeUrl", "mailUrl", "p2pUrl", "matchingUrl",
    "missionSystemUrl", "mysteryUrl", "homelandUrl", "gachaUrl", "miniGameUrl", "voteUrl", "weeklyRankUrl",
    "fishUrl", "mainCityUrl", "kartUrl", "ActivityUrl",
};
constexpr const char* kLocalServiceBase = "https://lambda.partyanimalsgame.com/";
constexpr const char* kDefaultGatewayRegion = "us-east-1";

// ConfigAsyncRetriever posts {"operation":"request-config-for-client-v3", "region": ..., "pingRegions": [...]}
// and deserializes the reply straight into ConfigRetrieverResult (code "0" == Status.Succeeded).
Result ServeClientConfig(const std::string& body) {
    ClientConfig config = LocalClientConfig(ReFixPAGql::JsonStringMember(body, "region"));
    std::string json = "{\"code\":\"0\",\"envName\":\"PartyAnimals\",\"enabled\":true,\"gatewayRegion\":" +
                       ReFixPAGql::JsonString(config.gatewayRegion) + ",\"message\":\"\"";
    for (const auto& [field, url] : config.urls) json += ",\"" + field + "\":" + ReFixPAGql::JsonString(url);
    ReFixLog("[PA-Services] Client config served (gatewayRegion=%s)", config.gatewayRegion.c_str());
    Result r;
    r.handled = true;
    r.body = json + "}";
    return r;
}

} // namespace

Result HandleRequest(const std::string& body, const std::string& path) {
    if (ReFixPAGql::JsonStringMember(body, "operation") == "request-config-for-client-v3") return ServeClientConfig(body);
    std::string query = ReFixPAGql::JsonStringMember(body, "Query");
    std::string operation = ReFixPAGql::Operation(query);
    if (operation.empty()) operation = ReFixPAGql::JsonStringMember(body, "operation");
    if (operation == "changeDisplayName") return ChangeDisplayName(operation, query);

    Result r;
    std::string data;
    if (ReFixPARooms::Handle(operation, query, data)) {
        r.handled = true;
        r.body = ReFixPAGql::Success(operation, data);
        return r;
    }

    std::string lowerOp = operation;
    for (char& c : lowerOp) c = (char)tolower((unsigned char)c);
    std::string lowerPath = path;
    for (char& c : lowerPath) c = (char)tolower((unsigned char)c);

    if (operation == "createNemoBuckTxn" || query.find("createNemoBuckTxn") != std::string::npos) {
        ReFixOverlay::PromptPurchase(1260320, 1, "Nemo Bucks", "Nemo Bucks Pack", true);
        uint64_t nowSec = (uint64_t)time(nullptr);
        std::string txnData = "{\"orderId\":\"refix_order_1260320_" + std::to_string(nowSec) + "\""
                              ",\"finance\":{\"currency\":\"USD\",\"nemoBuck\":999999,\"kartCoin\":999999,\"miniGameCoin\":999999,\"timestamp\":" + std::to_string(nowSec) + "}}";
        r.handled = true;
        r.body = ReFixPAGql::Success(operation.empty() ? "createNemoBuckTxn" : operation, txnData);
        ReFixLog("[PA-Services] Served createNemoBuckTxn order and credited Nemo Bucks");
        return r;
    }

    if (operation == "getAvoidList" || query.find("getAvoidList") != std::string::npos) {
        r.handled = true;
        r.body = ReFixPAGql::Success("getAvoidList", "{\"avoidList\":[],\"nextRefreshTime\":2147483647}");
        ReFixLog("[PA-Services] Served getAvoidList (nextRefreshTime=max)");
        return r;
    }

    if (operation == "getAnniv3Info" || query.find("getAnniv3Info") != std::string::npos) {
        uint64_t steamId = ReFixSteamFlat::LocalSteamID();
        uint64_t nowSec = (uint64_t)time(nullptr);
        std::string annivData = "{\"accountId\":\"fake_" + std::to_string(steamId) + "\""
                                ",\"anniv3ClaimRewardRecord\":[]"
                                ",\"anniv3PersonalContribution\":100"
                                ",\"anniv3PersonalContributionDelta\":0"
                                ",\"anniv3GlobalContribution\":1000000"
                                ",\"timestamp\":" + std::to_string(nowSec) + "}";
        r.handled = true;
        r.body = ReFixPAGql::Success("getAnniv3Info", annivData);
        ReFixLog("[PA-Services] Served getAnniv3Info");
        return r;
    }

    if (operation == "claimAnniv3Reward" || query.find("claimAnniv3Reward") != std::string::npos) {
        uint64_t steamId = ReFixSteamFlat::LocalSteamID();
        uint64_t nowSec = (uint64_t)time(nullptr);
        std::string claimData = "{\"claimRes\":{\"getReward\":true,\"newItems\":[],\"newItemQty\":[]"
                                ",\"finance\":{\"currency\":\"USD\",\"nemoBuck\":999999,\"kartCoin\":999999,\"miniGameCoin\":999999,\"timestamp\":" + std::to_string(nowSec) + "}"
                                ",\"actInfo\":{\"accountId\":\"fake_" + std::to_string(steamId) + "\""
                                ",\"anniv3ClaimRewardRecord\":[]"
                                ",\"anniv3PersonalContribution\":100"
                                ",\"anniv3PersonalContributionDelta\":0"
                                ",\"anniv3GlobalContribution\":1000000"
                                ",\"timestamp\":" + std::to_string(nowSec) + "}}}";
        r.handled = true;
        r.body = ReFixPAGql::Success("claimAnniv3Reward", claimData);
        ReFixLog("[PA-Services] Served claimAnniv3Reward");
        return r;
    }

    if (operation == "accountKartInventory" || query.find("accountKartInventory") != std::string::npos) {
        r.handled = true;
        r.body = ReFixPAGql::Success("accountKartInventory", "[]");
        ReFixLog("[PA-Services] Served accountKartInventory empty array");
        return r;
    }

    if (operation == "accountKartLevelInfo" || query.find("accountKartLevelInfo") != std::string::npos) {
        r.handled = true;
        r.body = ReFixPAGql::Success("accountKartLevelInfo", "[]");
        ReFixLog("[PA-Services] Served accountKartLevelInfo empty array");
        return r;
    }

    if (operation == "recordShutdown" || query.find("recordShutdown") != std::string::npos) {
        r.handled = true;
        r.body = ReFixPAGql::Success("recordShutdown", "{\"totalTime\":3600}");
        ReFixLog("[PA-Services] Served recordShutdown");
        return r;
    }

    if (operation == "getMessage" || query.find("getMessage") != std::string::npos) {
        r.handled = true;
        r.body = ReFixPAGql::Success("getMessage", "{\"messages\":[]}");
        ReFixLog("[PA-Services] Served getMessage empty list");
        return r;
    }

    if (lowerPath.find("store") != std::string::npos || lowerOp.find("store") != std::string::npos ||
        lowerOp.find("shop") != std::string::npos || lowerOp.find("goods") != std::string::npos || lowerOp.find("mall") != std::string::npos) {
        r.handled = true;
        r.body = StoreResponse(operation);
        return r;
    }

    if (lowerPath.find("matching") != std::string::npos || lowerOp.find("match") != std::string::npos || lowerOp.find("team") != std::string::npos || lowerOp.find("party") != std::string::npos ||
        lowerOp.find("group") != std::string::npos) {
        r.handled = true;
        r.body = TeamResponse(operation, query);
        return r;
    }

    // Generic fallback: never drop connections, always respond with clean success
    r.handled = true;
    r.body = ReFixPAGql::Success(operation.empty() ? "result" : operation, "{\"ok\":true,\"code\":0}");
    ReFixLog("[PA-Services] Generic fallback served for operation '%s' path '%s'", operation.c_str(), path.c_str());
    return r;
}

ClientConfig LocalClientConfig(const std::string& requestedRegion) {
    ClientConfig config;
    config.gatewayRegion = requestedRegion.empty() ? kDefaultGatewayRegion : requestedRegion;
    uint16_t port = ReFixPABackend::Port(); std::string base = port ? ("http://127.0.0.1:" + std::to_string(port) + "/") : kLocalServiceBase; for (const char* field : kServiceUrlFields) config.urls.emplace_back(field, base + field);
    return config;
}

std::string StoredDisplayName() {
    return StoredDisplayNameInternal();
}

void InitStoreDownloader(uint32_t appId) {
    static std::atomic<bool> s_started{ false };
    if (s_started.exchange(true)) return;
    std::thread([appId]() {
        Sleep(2000);
        typedef void* HINTERNET;
        typedef HINTERNET (WINAPI *fn_InternetOpenA)(LPCSTR, DWORD, LPCSTR, LPCSTR, DWORD);
        typedef HINTERNET (WINAPI *fn_InternetOpenUrlA)(HINTERNET, LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
        typedef BOOL (WINAPI *fn_InternetReadFile)(HINTERNET, LPVOID, DWORD, LPDWORD);
        typedef BOOL (WINAPI *fn_InternetCloseHandle)(HINTERNET);

        HMODULE hWinINet = LoadLibraryA("wininet.dll");
        if (!hWinINet) return;
        auto pfnOpen = (fn_InternetOpenA)GetProcAddress(hWinINet, "InternetOpenA");
        auto pfnOpenUrl = (fn_InternetOpenUrlA)GetProcAddress(hWinINet, "InternetOpenUrlA");
        auto pfnRead = (fn_InternetReadFile)GetProcAddress(hWinINet, "InternetReadFile");
        auto pfnClose = (fn_InternetCloseHandle)GetProcAddress(hWinINet, "InternetCloseHandle");
        if (!pfnOpen || !pfnOpenUrl || !pfnRead || !pfnClose) {
            FreeLibrary(hWinINet);
            return;
        }
        HINTERNET hSession = pfnOpen("ReFix/1.0", 1, nullptr, nullptr, 0);
        if (hSession) {
            std::string url = "https://store.steampowered.com/api/appdetails?appids=" + std::to_string(appId);
            HINTERNET hFile = pfnOpenUrl(hSession, url.c_str(), nullptr, 0, 0x80000000 | 0x04000000, 0);
            if (hFile) {
                std::string data;
                char buf[4096];
                DWORD readBytes = 0;
                while (pfnRead(hFile, buf, sizeof(buf), &readBytes) && readBytes > 0) {
                    data.append(buf, readBytes);
                    if (data.size() > 5 * 1024 * 1024) break;
                }
                pfnClose(hFile);
                if (!data.empty() && data.find("\"success\":true") != std::string::npos) {
                    std::string path = StoreCatalogPath();
                    std::ofstream out(path, std::ios::binary);
                    if (out) {
                        out.write(data.data(), data.size());
                        ReFixLog("[PA-Services] Downloaded store catalog for app %u (%zu bytes) -> %s", appId, data.size(), path.c_str());
                    }
                }
            }
            pfnClose(hSession);
        }
        FreeLibrary(hWinINet);
    }).detach();
}

} // namespace ReFixPAServices
