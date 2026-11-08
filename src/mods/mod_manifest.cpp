#include "mod_manifest.h"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <map>
#include <vector>

namespace refix::mods {

namespace {

// =============================================================================
// Lightweight Zero-Dependency JSON Parser
// =============================================================================
enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;
};

class JsonParser {
    const std::string& src;
    size_t pos = 0;

    void SkipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
            pos++;
        }
    }

    char Peek() { SkipWhitespace(); return pos < src.size() ? src[pos] : '\0'; }
    char Get() { SkipWhitespace(); return pos < src.size() ? src[pos++] : '\0'; }

    bool ParseString(std::string& out) {
        if (Get() != '"') return false;
        out.clear();
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos >= src.size()) return false;
                char esc = src[pos++];
                if (esc == '"') out.push_back('"');
                else if (esc == '\\') out.push_back('\\');
                else if (esc == '/') out.push_back('/');
                else if (esc == 'b') out.push_back('\b');
                else if (esc == 'f') out.push_back('\f');
                else if (esc == 'n') out.push_back('\n');
                else if (esc == 'r') out.push_back('\r');
                else if (esc == 't') out.push_back('\t');
                else out.push_back(esc);
            } else {
                out.push_back(c);
            }
        }
        return false;
    }

    bool ParseNumber(double& out) {
        size_t start = pos;
        if (pos < src.size() && (src[pos] == '-' || src[pos] == '+')) pos++;
        while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '-' || src[pos] == '+')) {
            pos++;
        }
        try {
            out = std::stod(src.substr(start, pos - start));
            return true;
        } catch (...) {
            return false;
        }
    }

public:
    explicit JsonParser(const std::string& text) : src(text) {}

    bool Parse(JsonValue& out) {
        SkipWhitespace();
        char c = Peek();
        if (c == '{') {
            Get();
            out.type = JsonType::Object;
            out.obj_val.clear();
            if (Peek() == '}') { Get(); return true; }
            while (pos < src.size()) {
                std::string key;
                if (!ParseString(key)) return false;
                if (Get() != ':') return false;
                JsonValue val;
                if (!Parse(val)) return false;
                out.obj_val[key] = std::move(val);
                char next = Get();
                if (next == '}') return true;
                if (next != ',') return false;
            }
            return false;
        } else if (c == '[') {
            Get();
            out.type = JsonType::Array;
            out.arr_val.clear();
            if (Peek() == ']') { Get(); return true; }
            while (pos < src.size()) {
                JsonValue val;
                if (!Parse(val)) return false;
                out.arr_val.push_back(std::move(val));
                char next = Get();
                if (next == ']') return true;
                if (next != ',') return false;
            }
            return false;
        } else if (c == '"') {
            out.type = JsonType::String;
            return ParseString(out.str_val);
        } else if (c == 't' || c == 'f') {
            out.type = JsonType::Bool;
            if (src.compare(pos, 4, "true") == 0) { pos += 4; out.bool_val = true; return true; }
            if (src.compare(pos, 5, "false") == 0) { pos += 5; out.bool_val = false; return true; }
            return false;
        } else if (c == 'n') {
            out.type = JsonType::Null;
            if (src.compare(pos, 4, "null") == 0) { pos += 4; return true; }
            return false;
        } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
            out.type = JsonType::Number;
            return ParseNumber(out.num_val);
        }
        return false;
    }
};

std::string EscapeJsonString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    out += "\"";
    return out;
}

} // namespace

bool IsSafeRelativePath(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/' || path[0] == '\\') return false;
    if (path.find(':') != std::string::npos) return false;
    if (path.find("..") != std::string::npos) return false;
    if (path.find('\0') != std::string::npos) return false;
    return true;
}

bool ParseManifest(const std::string& json_text, ModManifest& out, std::string& out_error) {
    JsonParser parser(json_text);
    JsonValue root;
    if (!parser.Parse(root) || root.type != JsonType::Object) {
        out_error = "Malformed JSON syntax or root is not an object";
        return false;
    }

    // schema_version
    auto it_schema = root.obj_val.find("schema_version");
    if (it_schema == root.obj_val.end() || it_schema->second.type != JsonType::Number) {
        out_error = "Missing or invalid 'schema_version'";
        return false;
    }
    out.schema_version = static_cast<uint32_t>(it_schema->second.num_val);
    if (out.schema_version != 1) {
        out_error = "Unsupported schema_version: only version 1 is supported";
        return false;
    }

    // id
    auto it_id = root.obj_val.find("id");
    if (it_id != root.obj_val.end() && it_id->second.type == JsonType::String) {
        out.id = it_id->second.str_val;
    } else {
        out_error = "Missing required string property 'id'";
        return false;
    }

    // name, version, author
    if (root.obj_val.count("name")) out.name = root.obj_val["name"].str_val;
    if (root.obj_val.count("version")) out.version = root.obj_val["version"].str_val;
    if (root.obj_val.count("author")) out.author = root.obj_val["author"].str_val;
    if (root.obj_val.count("author_pubkey")) out.author_pubkey = root.obj_val["author_pubkey"].str_val;
    if (root.obj_val.count("developer_cert")) out.developer_cert = root.obj_val["developer_cert"].str_val;
    if (root.obj_val.count("entrypoint")) out.entrypoint = root.obj_val["entrypoint"].str_val;
    if (root.obj_val.count("phase")) out.phase = root.obj_val["phase"].str_val;
    if (root.obj_val.count("priority")) out.priority = static_cast<int32_t>(root.obj_val["priority"].num_val);

    // execution_tier
    if (root.obj_val.count("execution_tier")) {
        const std::string& tier = root.obj_val["execution_tier"].str_val;
        if (tier == "native") out.execution_tier = ExecutionTier::Native;
        else out.execution_tier = ExecutionTier::Sandboxed;
    }

    // target_games
    out.target_games.clear();
    if (root.obj_val.count("target_games") && root.obj_val["target_games"].type == JsonType::Array) {
        for (const auto& g : root.obj_val["target_games"].arr_val) {
            if (g.type == JsonType::Object) {
                TargetGame tg;
                if (g.obj_val.count("app_id")) tg.app_id = static_cast<uint32_t>(g.obj_val.at("app_id").num_val);
                if (g.obj_val.count("exe_name")) tg.exe_name = g.obj_val.at("exe_name").str_val;
                if (g.obj_val.count("engine")) tg.engine = g.obj_val.at("engine").str_val;
                out.target_games.push_back(tg);
            }
        }
    }

    // dependencies
    out.dependencies = ModDependencies();
    if (root.obj_val.count("dependencies") && root.obj_val["dependencies"].type == JsonType::Object) {
        const auto& dep_obj = root.obj_val["dependencies"].obj_val;
        auto parse_str_list = [&](const std::string& key, std::vector<std::string>& dest) {
            if (dep_obj.count(key) && dep_obj.at(key).type == JsonType::Array) {
                for (const auto& item : dep_obj.at(key).arr_val) {
                    if (item.type == JsonType::String) dest.push_back(item.str_val);
                }
            }
        };
        parse_str_list("before", out.dependencies.before);
        parse_str_list("after", out.dependencies.after);
        parse_str_list("requires", out.dependencies.requires);
        parse_str_list("conflicts", out.dependencies.conflicts);
    }

    // capabilities
    out.capabilities.clear();
    if (root.obj_val.count("capabilities") && root.obj_val["capabilities"].type == JsonType::Array) {
        for (const auto& item : root.obj_val["capabilities"].arr_val) {
            if (item.type == JsonType::String) out.capabilities.insert(item.str_val);
        }
    }

    // resource_limits
    if (root.obj_val.count("resource_limits") && root.obj_val["resource_limits"].type == JsonType::Object) {
        const auto& lim = root.obj_val["resource_limits"].obj_val;
        if (lim.count("max_memory_mb")) out.resource_limits.max_memory_mb = static_cast<uint32_t>(lim.at("max_memory_mb").num_val);
        if (lim.count("watchdog_timeout_ms")) out.resource_limits.watchdog_timeout_ms = static_cast<uint32_t>(lim.at("watchdog_timeout_ms").num_val);
        if (lim.count("max_hooks")) out.resource_limits.max_hooks = static_cast<uint32_t>(lim.at("max_hooks").num_val);
        if (lim.count("storage_quota_kb")) out.resource_limits.storage_quota_kb = static_cast<uint32_t>(lim.at("storage_quota_kb").num_val);
    }

    // files
    out.files.clear();
    if (root.obj_val.count("files") && root.obj_val["files"].type == JsonType::Object) {
        for (const auto& kv : root.obj_val["files"].obj_val) {
            if (kv.second.type == JsonType::String) {
                out.files[kv.first] = kv.second.str_val;
            }
        }
    }

    return true;
}

bool ValidateManifest(const ModManifest& m, std::string& out_error) {
    if (m.schema_version != 1) {
        out_error = "Invalid schema_version";
        return false;
    }
    if (m.id.empty()) {
        out_error = "Manifest ID cannot be empty";
        return false;
    }
    for (char c : m.id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-') {
            out_error = "Manifest ID contains illegal character: '" + std::string(1, c) + "'";
            return false;
        }
    }
    if (m.id.find("..") != std::string::npos) {
        out_error = "Manifest ID cannot contain '..'";
        return false;
    }

    // Validate capabilities
    for (const auto& cap : m.capabilities) {
        if (!Capability::IsValidCapability(cap)) {
            out_error = "Unknown or prohibited capability: '" + cap + "'";
            return false;
        }
    }

    // Validate files paths
    for (const auto& kv : m.files) {
        if (!IsSafeRelativePath(kv.first)) {
            out_error = "Unsafe file path in manifest: '" + kv.first + "'";
            return false;
        }
        if (kv.second.size() != 64) {
            out_error = "Invalid SHA-256 hash length for file '" + kv.first + "'";
            return false;
        }
    }

    return true;
}

std::string ToCanonicalJson(const ModManifest& m) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"author\":" << EscapeJsonString(m.author) << ",";
    ss << "\"author_pubkey\":" << EscapeJsonString(m.author_pubkey) << ",";

    // capabilities sorted
    ss << "\"capabilities\":[";
    bool first = true;
    for (const auto& cap : m.capabilities) {
        if (!first) ss << ",";
        first = false;
        ss << EscapeJsonString(cap);
    }
    ss << "],";

    // dependencies
    ss << "\"dependencies\":{";
    auto dump_str_vec = [&](const char* name, const std::vector<std::string>& vec, bool is_last) {
        ss << "\"" << name << "\":[";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) ss << ",";
            ss << EscapeJsonString(vec[i]);
        }
        ss << "]" << (is_last ? "" : ",");
    };
    dump_str_vec("after", m.dependencies.after, false);
    dump_str_vec("before", m.dependencies.before, false);
    dump_str_vec("conflicts", m.dependencies.conflicts, false);
    dump_str_vec("requires", m.dependencies.requires, true);
    ss << "},";

    if (!m.developer_cert.empty()) {
        ss << "\"developer_cert\":" << EscapeJsonString(m.developer_cert) << ",";
    }
    ss << "\"entrypoint\":" << EscapeJsonString(m.entrypoint) << ",";
    ss << "\"execution_tier\":" << (m.execution_tier == ExecutionTier::Native ? "\"native\"" : "\"sandboxed\"") << ",";

    // files sorted
    ss << "\"files\":{";
    first = true;
    for (const auto& kv : m.files) {
        if (!first) ss << ",";
        first = false;
        ss << EscapeJsonString(kv.first) << ":" << EscapeJsonString(kv.second);
    }
    ss << "},";

    ss << "\"id\":" << EscapeJsonString(m.id) << ",";
    ss << "\"name\":" << EscapeJsonString(m.name) << ",";
    ss << "\"phase\":" << EscapeJsonString(m.phase) << ",";
    ss << "\"priority\":" << m.priority << ",";

    // resource_limits
    ss << "\"resource_limits\":{";
    ss << "\"max_hooks\":" << m.resource_limits.max_hooks << ",";
    ss << "\"max_memory_mb\":" << m.resource_limits.max_memory_mb << ",";
    ss << "\"storage_quota_kb\":" << m.resource_limits.storage_quota_kb << ",";
    ss << "\"watchdog_timeout_ms\":" << m.resource_limits.watchdog_timeout_ms;
    ss << "},";

    ss << "\"schema_version\":" << m.schema_version << ",";

    // target_games
    ss << "\"target_games\":[";
    for (size_t i = 0; i < m.target_games.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{";
        ss << "\"app_id\":" << m.target_games[i].app_id << ",";
        ss << "\"engine\":" << EscapeJsonString(m.target_games[i].engine) << ",";
        ss << "\"exe_name\":" << EscapeJsonString(m.target_games[i].exe_name);
        ss << "}";
    }
    ss << "],";

    ss << "\"version\":" << EscapeJsonString(m.version);
    ss << "}";
    return ss.str();
}

} // namespace refix::mods
