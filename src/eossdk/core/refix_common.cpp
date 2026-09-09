#include "refix_common.h"
#include <chrono>
#include <cstdio>
#include <winver.h>

namespace refix {

const char* CopyString(const char* src) {
    if (!src) return nullptr;
    size_t n = std::strlen(src) + 1;
    char* p = (char*)std::malloc(n);
    if (!p) return nullptr;
    std::memcpy(p, src, n);
    return p;
}

const char* CopyString(const std::string& src) { return CopyString(src.c_str()); }

void FreeString(const char* s) { if (s) std::free((void*)s); }

const std::string& GameDirectory() {
    static std::string dir = [] {
        char buf[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string p(buf);
        size_t pos = p.find_last_of("\\/");
        return pos == std::string::npos ? std::string(".\\") : p.substr(0, pos + 1);
    }();
    return dir;
}

const std::string& ExecutableProductName() {
    static std::string name = [] {
        char exe[MAX_PATH] = {0};
        if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return std::string();
        DWORD ignored = 0;
        DWORD size = GetFileVersionInfoSizeA(exe, &ignored);
        if (!size) return std::string();
        std::vector<uint8_t> block(size);
        if (!GetFileVersionInfoA(exe, 0, size, block.data())) return std::string();

        // The version resource is per-language; ask for whichever translation
        // the executable actually shipped rather than assuming US English.
        struct LangCp { WORD Lang; WORD CodePage; };
        LangCp* langs = nullptr;
        UINT langBytes = 0;
        if (!VerQueryValueA(block.data(), "\\VarFileInfo\\Translation",
                            (LPVOID*)&langs, &langBytes) || langBytes < sizeof(LangCp))
            return std::string();

        for (UINT i = 0; i < langBytes / sizeof(LangCp); ++i) {
            char key[64];
            snprintf(key, sizeof(key), "\\StringFileInfo\\%04x%04x\\ProductName",
                     langs[i].Lang, langs[i].CodePage);
            char* value = nullptr;
            UINT valueLen = 0;
            if (VerQueryValueA(block.data(), key, (LPVOID*)&value, &valueLen) && value && *value)
                return Trim(std::string(value, valueLen ? valueLen - 1 : 0));
        }
        return std::string();
    }();
    return name;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = (char)::tolower((unsigned char)c);
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

uint64_t NowMs() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}

int64_t UnixSeconds() {
    using namespace std::chrono;
    return (int64_t)duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace refix
