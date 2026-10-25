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

} // namespace

Result HandleRequest(const std::string& body) {
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

std::string StoredDisplayName() {
    std::lock_guard<std::mutex> lock(g_profileMutex);
    char buf[256] = { 0 };
    GetPrivateProfileStringA("Profile", "DisplayName", "", buf, sizeof(buf), ProfilePath().c_str());
    return buf;
}

} // namespace ReFixPAServices
