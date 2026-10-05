# ReFix LAN / Re:Goldberg — Handoff

## Estado Actual
- **CURRENT PHASE:** Phase 3.6.1 VERIFIED
- **CURRENT COMMIT:** pending commit
- **WORKTREE STATUS:** modifications in `src/unreal_steam_emu.cpp`, `src/steam_proxy.cpp`, `src/network/refix_net_test.cpp`, configs, and docs.
- **TEST RESULTS:** 
  - ABI static assertions verified: `sizeof(SteamNetConnectionInfo_t) == 696`, `sizeof(SteamNetworkingIdentity) == 136`, `sizeof(SteamNetworkingIPAddr) == 18`, `sizeof(SteamNetworkingMessage_t) == 216`.
  - In-process ABI and vtable routing: PASS (all slots strictly matching SDK).
  - Message lifetime allocator: PASS (10,000 cycles, 0 leaks, 0 crashes).
  - Multithreaded locking: PASS (10,000 iterations across 4 threads, 0 deadlocks, 0 crashes, 0 races).
  - Internet-Zero isolation: PASS (public WAN IPs blocked, LAN/loopback allowed).
  - Adversarial Reliable Protocol: PASS across multiple seeds (10% drop / 5% dup / 50-200ms jitter and 20% drop / 10% dup / 50-250ms jitter): 1000/1000 reliable messages delivered, 0 missing, 0 duplicates, 0 sequence order violations.
  - Unreliable protocol: PASS (750+ messages received without stall or retransmission storm).
- **ONLINE REGRESSION STATUS:** UNVERIFIED (Proxy boundary routes straight to Valve DLL in online mode, but runtime online verification is impossible in this offline testbed).
- **LAN STATUS:** Fully Verified.
- **RESOLVED ROOT CAUSES:**
  1. `SteamNetConnectionInfo_t` ABI size: Corrected from erroneous 416 bytes to true SDK size of 696 bytes.
  2. `SocketsHandshakeAck` parsing bug: Receiver was reading `protocolVersion` (1) instead of `ack->sessionId`, causing client connection ACK lookup to fail.
  3. Bidirectional discovery: Implemented unicast discovery ping replies so peers learn each other's endpoints before connecting.
  4. Interface routing: `steam_proxy.cpp` properly routes `SteamNetworkingSockets` and `SteamNetworkingUtils` to `UnrealSteamEmu` in LAN mode, while strictly preserving native Valve DLL passthrough in Online mode.
  5. Localhost multi-process port collision: Configured distinct listen ports (47584 Host, 47585 Client) on localhost so Winsock can direct unicast UDP packets correctly.
- **NEXT EXACT ACTION:** Do NOT advance to Phase 4 until user explicitly directs.
