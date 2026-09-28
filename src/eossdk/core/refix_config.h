// ReFix EOS v3 - configuration.
//
// Every tunable the emulator has comes from here. There are no magic constants
// buried in the implementation: values are resolved in this order
//
//   1. process environment  (REFIX_<SECTION>_<KEY>)  - per-instance overrides
//   2. ReFix.ini next to the game executable
//   3. the default supplied at the call site
//
// The environment layer is what makes it possible to run two independent
// instances of the same game on one machine (different identity, different
// ports) without editing any file.
#pragma once
#include "refix_common.h"

namespace refix {

class Config {
public:
    static Config& Get();

    std::string GetString(const char* section, const char* key, const std::string& def = "") const;
    int         GetInt   (const char* section, const char* key, int def = 0) const;
    bool        GetBool  (const char* section, const char* key, bool def = false) const;

    const std::string& IniPath() const { return m_iniPath; }
    bool               IniFound() const { return m_found; }

private:
    Config();
    void Load();

    std::string m_iniPath;
    bool        m_found = false;
    // section -> key -> value, both lowercased for lookup.
    std::map<std::string, std::map<std::string, std::string>> m_values;
};

} // namespace refix
