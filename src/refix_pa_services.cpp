#include "refix_pa_services.h"
#include "refix_pa_gql.h"
#include "refix_pa_rooms.h"
#include <windows.h>
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
    // finance.timestamp 0 is older than the local one, so AccountFinanceManager keeps the current balance.
    r.body = ReFixPAGql::Success(operation, "{\"newName\":" + ReFixPAGql::JsonString(name) + ",\"finance\":{\"timestamp\":0}}");
    return r;
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

Result HandleRequest(const std::string& body) {
    if (ReFixPAGql::JsonStringMember(body, "operation") == "request-config-for-client-v3") return ServeClientConfig(body);
    std::string query = ReFixPAGql::JsonStringMember(body, "Query");
    std::string operation = ReFixPAGql::Operation(query);
    if (operation == "changeDisplayName") return ChangeDisplayName(operation, query);

    Result r;
    std::string data;
    if (ReFixPARooms::Handle(operation, query, data)) {
        r.handled = true;
        r.body = ReFixPAGql::Success(operation, data);
    }
    return r;
}

ClientConfig LocalClientConfig(const std::string& requestedRegion) {
    ClientConfig config;
    config.gatewayRegion = requestedRegion.empty() ? kDefaultGatewayRegion : requestedRegion;
    for (const char* field : kServiceUrlFields) config.urls.emplace_back(field, std::string(kLocalServiceBase) + field);
    return config;
}

std::string StoredDisplayName() {
    std::lock_guard<std::mutex> lock(g_profileMutex);
    char buf[256] = { 0 };
    GetPrivateProfileStringA("Profile", "DisplayName", "", buf, sizeof(buf), ProfilePath().c_str());
    return buf;
}

} // namespace ReFixPAServices
