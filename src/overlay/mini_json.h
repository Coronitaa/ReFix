// Minimal JSON reader for the Steam Web API answers the overlay uses (achievement schema). Parses the whole
// document into a tree; malformed input gives a null root. Not a general-purpose library.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace refix::overlay::json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double n = 0;
    std::string s;                                   // UTF-8
    std::vector<Value> items;
    std::map<std::string, Value> fields;

    const Value& operator[](const char* key) const {
        static const Value kNull;
        auto it = fields.find(key);
        return it == fields.end() ? kNull : it->second;
    }
    std::string Str(const char* key) const {
        const Value& v = (*this)[key];
        return v.type == Type::String ? v.s : std::string();
    }
    // Numbers may come as strings ("player_percent_unlocked":"74.0").
    double Num(const char* key, double def = 0) const {
        const Value& v = (*this)[key];
        if (v.type == Type::Number) return v.n;
        if (v.type == Type::String && !v.s.empty()) return std::strtod(v.s.c_str(), nullptr);
        return def;
    }
    bool Bool(const char* key) const {
        const Value& v = (*this)[key];
        return v.type == Type::Bool ? v.b : v.type == Type::Number && v.n != 0;
    }
};

namespace detail {

constexpr int kMaxDepth = 64;

struct Parser {
    const char* p;
    const char* end;
    bool ok = true;

    void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
    bool Eat(char c) { Ws(); if (p < end && *p == c) { ++p; return true; } return false; }
    bool Lit(const char* w) {
        size_t n = 0;
        while (w[n]) ++n;
        if ((size_t)(end - p) < n) return false;
        for (size_t i = 0; i < n; ++i) if (p[i] != w[i]) return false;
        p += n;
        return true;
    }
    static void Utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool Hex4(unsigned& v) {
        if (end - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool String(std::string& out) {
        if (!Eat('"')) return false;
        while (p < end && *p != '"') {
            if (*p != '\\') { out += *p++; continue; }
            if (++p >= end) return false;
            const char e = *p++;
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp;
                if (!Hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                    p += 2;
                    unsigned lo;
                    if (!Hex4(lo)) return false;
                    if (lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                Utf8(out, cp);
                break;
            }
            default: out += e; break;   // \" \\ \/
            }
        }
        if (p >= end) return false;
        ++p;
        return true;
    }
    void Parse(Value& v, int depth) {
        if (!ok || depth > kMaxDepth) { ok = false; return; }
        Ws();
        if (p >= end) { ok = false; return; }
        const char c = *p;
        if (c == '{') {
            ++p;
            v.type = Value::Type::Object;
            if (Eat('}')) return;
            do {
                std::string key;
                if (!String(key) || !Eat(':')) { ok = false; return; }
                Parse(v.fields[key], depth + 1);
            } while (ok && Eat(','));
            if (!Eat('}')) ok = false;
        } else if (c == '[') {
            ++p;
            v.type = Value::Type::Array;
            if (Eat(']')) return;
            do {
                v.items.emplace_back();
                Parse(v.items.back(), depth + 1);
            } while (ok && Eat(','));
            if (!Eat(']')) ok = false;
        } else if (c == '"') {
            v.type = Value::Type::String;
            if (!String(v.s)) ok = false;
        } else if (Lit("true")) { v.type = Value::Type::Bool; v.b = true; }
        else if (Lit("false")) { v.type = Value::Type::Bool; v.b = false; }
        else if (Lit("null")) { v.type = Value::Type::Null; }
        else {
            const std::string num(p, (size_t)std::min<ptrdiff_t>(end - p, 64));
            char* stop = nullptr;
            v.n = std::strtod(num.c_str(), &stop);
            if (stop == num.c_str()) { ok = false; return; }
            v.type = Value::Type::Number;
            p += stop - num.c_str();
        }
    }
};

} // namespace detail

inline Value Parse(const std::string& text) {
    detail::Parser ps{ text.data(), text.data() + text.size() };
    Value root;
    ps.Parse(root, 0);
    return ps.ok ? root : Value();
}

} // namespace refix::overlay::json
