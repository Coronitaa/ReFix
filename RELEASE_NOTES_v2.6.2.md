# ReFix v2.6.2

## Summary of Changes

ReFix v2.6.2 resolves Photon Realtime room initialization timeouts and improves P2P lobby discovery for friends.

---

### 1. Generic Photon Realtime Event Routing (Resolves #40)
- **Strict Protocol Discrimination**: Differentiated Photon Fusion protocol messages from generic Photon Realtime / PUN 2 custom room events (Event Code 100). Validates protocol versions and modes so non-Fusion room setup packets are never swallowed.
- **Actor 0 Multiplexing**: Implemented generic routing for events targeted to Actor 0. Non-master client events route directly to the authoritative room master, while room master broadcasts deliver to all peers in the room.

### 2. Friend Room Discovery in Public Lobbies
- **Rich Presence Synthesis**: Integrated Steam Rich Presence (`refix_photon_room` and `connect`) into the public lobby list (`OpGetGameList` / opcode 217). Friends hosting games are now immediately visible and joinable in game browser lists even prior to active P2P broadcast discovery.

### 3. Binary Security & Code Signing
- **Authenticode Signatures**: All proxy DLLs (`winhttp.dll`, `winmm.dll`, `steam_api64.dll`, `steam_api.dll`, `EOSSDK-Win64-Shipping.dll`, `RedboneEOS.dll`) and executables across all configurations are signed with Authenticode certificates and RFC 3161 timestamps to prevent antivirus heuristic false positives.
