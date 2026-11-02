#include "refix_pa_gql.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ReFixPAGql {

namespace {

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

bool IsIdentChar(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

// Position of the value after a whole-word `key` followed by `separator`, or npos.
size_t ValueStart(const std::string& text, const std::string& key, char separator) {
    for (size_t pos = text.find(key); pos != std::string::npos; pos = text.find(key, pos + 1)) {
        bool wordStart = pos == 0 || !IsIdentChar(text[pos - 1]) || key[0] == '"';
        size_t sep = text.find_first_not_of(" \t\r\n", pos + key.size());
        if (!wordStart || sep == std::string::npos || text[sep] != separator) continue;
        return text.find_first_not_of(" \t\r\n", sep + 1);
    }
    return std::string::npos;
}

std::string ScalarAt(const std::string& text, size_t pos) {
    if (pos == std::string::npos) return "";
    size_t end = pos;
    while (end < text.size() && (isalnum((unsigned char)text[end]) || text[end] == '-' || text[end] == '.' || text[end] == '_')) end++;
    return text.substr(pos, end - pos);
}

} // namespace

bool ReadQuoted(const std::string& text, size_t pos, std::string& out, size_t* end) {
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

std::string JsonString(const std::string& s) {
    return "\"" + JsonEscape(s) + "\"";
}

std::string JsonStringMember(const std::string& json, const char* name) {
    size_t pos = ValueStart(json, std::string("\"") + name + "\"", ':');
    std::string value;
    return pos != std::string::npos && ReadQuoted(json, pos, value) ? value : "";
}

std::string JsonScalarMember(const std::string& json, const char* name) {
    return ScalarAt(json, ValueStart(json, std::string("\"") + name + "\"", ':'));
}

std::string Operation(const std::string& query) {
    size_t brace = query.find('{');
    if (brace == std::string::npos) return "";
    size_t i = brace + 1;
    while (i < query.size() && isspace((unsigned char)query[i])) i++;
    size_t start = i;
    while (i < query.size() && (isalnum((unsigned char)query[i]) || query[i] == '_')) i++;
    return query.substr(start, i - start);
}

std::string StringArg(const std::string& query, const char* name) {
    size_t pos = ValueStart(query, std::string(name), ':');
    std::string value;
    return pos != std::string::npos && ReadQuoted(query, pos, value) ? value : "";
}

std::string ScalarArg(const std::string& query, const char* name) {
    return ScalarAt(query, ValueStart(query, std::string(name), ':'));
}

std::string Success(const std::string& operation, const std::string& dataJson) {
    return "{\"code\":0,\"httpStatus\":200,\"result\":{\"" + operation + "\":" + dataJson + "},\"message\":\"\"}";
}

} // namespace ReFixPAGql
