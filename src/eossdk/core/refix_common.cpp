#include "refix_common.h"
#include <chrono>

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
