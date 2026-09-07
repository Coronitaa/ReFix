// ReFix EOS v3 - shared primitives.
//
// Everything in the emulator is built on top of this header: it pulls in the
// official EOS SDK C headers (vendored under src/eossdk/sdk) so that every
// struct we touch has the exact layout the game's Redpoint/EOS plugin expects,
// and provides the few small utilities the rest of the code relies on.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

// The SDK headers are C; EOS_BUILD_DLL must stay undefined so EOS_DECLARE_FUNC
// resolves to an import/export-free declaration and we can define the symbols
// ourselves.
#include "../sdk/eos_sdk.h"
#include "../sdk/eos_logging.h"
#include "../sdk/eos_lobby.h"
#include "../sdk/eos_sessions.h"
#include "../sdk/eos_p2p.h"
#include "../sdk/eos_presence.h"
#include "../sdk/eos_friends.h"
#include "../sdk/eos_userinfo.h"
#include "../sdk/eos_achievements.h"
#include "../sdk/eos_stats.h"
#include "../sdk/eos_leaderboards.h"
#include "../sdk/eos_ecom.h"
#include "../sdk/eos_ui.h"
#include "../sdk/eos_metrics.h"
#include "../sdk/eos_sanctions.h"
#include "../sdk/eos_reports.h"
#include "../sdk/eos_titlestorage.h"
#include "../sdk/eos_playerdatastorage.h"
#include "../sdk/eos_custominvites.h"
#include "../sdk/eos_rtc.h"
#include "../sdk/eos_rtc_audio.h"
#include "../sdk/eos_anticheatclient.h"
#include "../sdk/eos_anticheatserver.h"
#include "../sdk/eos_integratedplatform.h"
#include "../sdk/eos_mods.h"
#include "../sdk/eos_kws.h"
#include "../sdk/eos_progressionsnapshot.h"
#include "../sdk/eos_version.h"

// The SDK compiles its enums as `enum class` under C++ (matching what the game's
// own EOS plugin sees), so values must be qualified. These short aliases keep
// call sites readable without weakening the typing.
using ER   = EOS_EResult;
using EAT  = EOS_EExternalAccountType;
using ECT  = EOS_EExternalCredentialType;
using ELS  = EOS_ELoginStatus;

namespace refix {

// Duplicates a string into memory owned by the emulator. EOS hands `const char*`
// back to the game inside structs that outlive the call, so the storage has to
// be stable until the matching _Release.
const char* CopyString(const char* src);
const char* CopyString(const std::string& src);
void        FreeString(const char* s);

// Directory of the running game executable; ReFix.ini and ReFix.log live there.
const std::string& GameDirectory();

std::string ToLower(std::string s);
std::string Trim(const std::string& s);

// Monotonic milliseconds since process start.
uint64_t NowMs();

// Wall-clock seconds since the Unix epoch (EOS reports login times this way).
int64_t UnixSeconds();

} // namespace refix
