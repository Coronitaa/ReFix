// ReFix EOS v3 - bringing the online layer up.
//
// The transport is started lazily, the first time a title actually asks for
// something online. A single-player session therefore never opens a socket.
#pragma once
#include "refix_common.h"

namespace refix {

// Starts the lobby directory for this title if it is not running yet.
// Returns false when networking is unavailable, in which case online entry
// points report failure instead of pretending to succeed.
bool EnsureOnline();

// Brings the online layer up *and* wires the lobby directory's events into the
// EOS notification system. Every online entry point goes through this rather
// than EnsureOnline directly, so notifications are always connected before the
// first datagram can arrive.
bool StartOnlineSubsystem();

// Routes P2P datagrams into the EOS_P2P receive queue. Installed when the online layer starts:
// a listen server may not touch the P2P API before its first client connects, and packets that
// arrived before then were dropped, so the host never saw the connection request.
void InstallP2PPacketHook();

// Scope string isolating this title's traffic on a shared network. Derived from
// the EOS product id the game passed to EOS_Platform_Create, so two different
// games never see each other's lobbies.
const std::string& OnlineScope();

} // namespace refix
