#include "refix_pa_services.h"
#include <windows.h>
#include <cctype>
#include <cstdint>
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

// --- Minimal JSON / GraphQL text helpers ---------------------------------------------------------

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

// Parses a JSON/GraphQL string literal starting at text[pos] == '"'. Returns false on malformed input.
bool ReadQuoted(const std::string& text, size_t pos, std::string& out, size_t* end = nullptr) {
    if (pos >= text.size() || text[pos] != '"') return false;
    out.clear();
    for (size_t i = pos + 1; i < text.size(); i++) {
        char c = text[i];
        if (c == '"') { if (end) *end = i + 1; return true; }
        if (c != '\\') { out += c; continue; }
        if (++i >= text.size()) return false;
        switch (text[i]) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u':
                if (i + 4 >= text.size()) return false;
                AppendUtf8(out, (uint32_t)strtoul(text.substr(i + 1, 4).c_str(), nullptr, 16));
                i += 4;
                break;
            default: out += text[i]; break;
        }
    }
    return false;
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
        else out += (char)c;
    }
    return out;
}

// Value of a top-level string member of a JSON object (e.g. "Query").
std::string JsonStringMember(const std::string& json, const char* name) {
    std::string key = std::string("\"") + name + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    std::string value;
    return pos != std::string::npos && ReadQuoted(json, pos, value) ? value : "";
}

// First field name of a GraphQL document: "mutation { changeDisplayName(...) {...} }" -> changeDisplayName.
std::string GraphQLOperation(const std::string& query) {
    size_t brace = query.find('{');
    if (brace == std::string::npos) return "";
    size_t i = brace + 1;
    while (i < query.size() && isspace((unsigned char)query[i])) i++;
    size_t start = i;
    while (i < query.size() && (isalnum((unsigned char)query[i]) || query[i] == '_')) i++;
    return query.substr(start, i - start);
}

// String argument value: `name: "value"` inside the operation's argument list.
std::string GraphQLStringArg(const std::string& query, const char* name) {
    std::string key = std::string(name) + ":";
    size_t pos = query.find(key);
    if (pos == std::string::npos) return "";
    pos = query.find_first_not_of(" \t", pos + key.size());
    std::string value;
    return pos != std::string::npos && ReadQuoted(query, pos, value) ? value : "";
}

std::string Success(const std::string& operation, const std::string& dataJson) {
    return "{\"code\":0,\"httpStatus\":200,\"result\":{\"" + operation + "\":" + dataJson + "},\"message\":\"\"}";
}

// --- Handlers --------------------------------------------------------------------------------------

Result ChangeDisplayName(const std::string& operation, const std::string& query) {
    Result r;
    std::string name = GraphQLStringArg(query, "newName");
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
    r.body = Success(operation, "{\"newName\":\"" + JsonEscape(name) + "\",\"finance\":{\"timestamp\":0}}");
    return r;
}

} // namespace

Result HandleRequest(const std::string& body) {
    std::string query = JsonStringMember(body, "Query");
    std::string operation = GraphQLOperation(query);
    if (operation == "changeDisplayName") return ChangeDisplayName(operation, query);
    return Result{};
}

std::string StoredDisplayName() {
    std::lock_guard<std::mutex> lock(g_profileMutex);
    char buf[256] = { 0 };
    GetPrivateProfileStringA("Profile", "DisplayName", "", buf, sizeof(buf), ProfilePath().c_str());
    return buf;
}

} // namespace ReFixPAServices
