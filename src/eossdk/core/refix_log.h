// ReFix EOS v3 - diagnostics.
//
// The emulator shares ReFix.log with the Steam proxy so a single file tells the
// whole story of a session. Every line is tagged with the subsystem that wrote
// it, which is what makes the log usable as a call trace when diagnosing a
// title's online flow.
#pragma once
#include "refix_common.h"

namespace refix {

enum class LogArea { Core, Abi, Platform, Auth, Lobby, Session, P2P, Net, Presence, User };

bool LogEnabled();
void LogWrite(LogArea area, const char* fmt, ...);

#define RFLOG(area, ...)  ::refix::LogWrite(::refix::LogArea::area, __VA_ARGS__)

} // namespace refix
