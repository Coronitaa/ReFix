#include "refix_config.h"
#include <fstream>

namespace refix {

Config& Config::Get() { static Config c; return c; }

Config::Config() { Load(); }

void Config::Load() {
    m_iniPath = GameDirectory() + "ReFix.ini";
    std::ifstream f(m_iniPath);
    if (!f.is_open()) return;
    m_found = true;

    std::string line, section;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = ToLower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = ToLower(Trim(line.substr(0, eq)));
        std::string val = Trim(line.substr(eq + 1));
        size_t comment = val.find(" ;");
        if (comment != std::string::npos) val = Trim(val.substr(0, comment));
        m_values[section][key] = val;
    }
}

static std::string EnvKey(const char* section, const char* key) {
    std::string s = std::string("REFIX_") + section + "_" + key;
    for (auto& c : s) c = (char)::toupper((unsigned char)c);
    return s;
}

std::string Config::GetString(const char* section, const char* key, const std::string& def) const {
    char buf[1024] = {0};
    DWORD n = GetEnvironmentVariableA(EnvKey(section, key).c_str(), buf, (DWORD)sizeof(buf));
    if (n > 0 && n < sizeof(buf)) return std::string(buf, n);

    auto s = m_values.find(ToLower(section));
    if (s != m_values.end()) {
        auto k = s->second.find(ToLower(key));
        if (k != s->second.end() && !k->second.empty()) return k->second;
    }
    return def;
}

int Config::GetInt(const char* section, const char* key, int def) const {
    std::string v = GetString(section, key, "");
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

bool Config::GetBool(const char* section, const char* key, bool def) const {
    std::string v = ToLower(GetString(section, key, ""));
    if (v.empty()) return def;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

} // namespace refix
