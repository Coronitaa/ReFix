#include "refix_log.h"
#include "refix_config.h"
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace refix {

static const char* AreaName(LogArea a) {
    switch (a) {
        case LogArea::Core:     return "CORE";
        case LogArea::Abi:      return "ABI ";
        case LogArea::Platform: return "PLAT";
        case LogArea::Auth:     return "AUTH";
        case LogArea::Lobby:    return "LOBBY";
        case LogArea::Session:  return "SESS";
        case LogArea::P2P:      return "P2P ";
        case LogArea::Net:      return "NET ";
        case LogArea::Presence: return "PRES";
        case LogArea::User:     return "USER";
    }
    return "????";
}

bool LogEnabled() {
    static bool on = Config::Get().GetBool("Debug", "EnableLog", true) ||
                     Config::Get().GetBool("EOS", "DebugLogging", false);
    return on;
}

void LogWrite(LogArea area, const char* fmt, ...) {
    if (!LogEnabled()) return;

    static std::mutex mtx;
    // Two instances of the same game on one machine must not fight over one
    // log; [Debug] LogFile (or REFIX_DEBUG_LOGFILE) gives each its own.
    static std::string path = [] {
        std::string name = Config::Get().GetString("Debug", "LogFile", "ReFix.log");
        if (name.size() > 1 && (name[1] == ':' || name[0] == '\\\\')) return name;  // absolute
        return GameDirectory() + name;
    }();

    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::lock_guard<std::mutex> lock(mtx);
    FILE* f = fopen(path.c_str(), "a");
    if (!f) return;
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [T%04X] [EOSv3:%s] %s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            (unsigned)GetCurrentThreadId(), AreaName(area), msg);
    fclose(f);
}

} // namespace refix
