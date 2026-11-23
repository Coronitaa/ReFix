# ReFix v2.6.1

## Summary of Changes Since v2.6.0

ReFix v2.6.1 is a hotfix release addressing binary security signatures and generic multiplayer protocol stability:

---

### 1. Digital Signature & Antivirus Heuristics
- **Authenticode Signing for winhttp.dll:** Fixed missing digital signature and embedded standard Win32 `VERSIONINFO` metadata on `winhttp.dll` proxies across all distribution packages. This resolves false-positive detections by Windows Defender and endpoint security heuristics.
- **Full Binary Suite Validation:** All DLL proxies, host executables, and dependencies across `bin/`, `lan/`, and `online/` are signed with the project's Authenticode certificate and timestamped via DigiCert RFC 3161.

### 2. Generic Photon Realtime Protocol Fixes (Resolves #40)
- **Lobby Game List Handling (`OpGetGameList` / Opcode 217):** Implemented generic server handling for room list requests in custom and native lobbies, returning structured game parameter lists to prevent client connection timeouts when querying active rooms.
- **Clean Lobby Exit (`OpLeaveLobby` / Opcode 228):** Implemented clean disconnect transitions when leaving match lobbies to prevent lingering session states.

### 3. Deployment & UI Improvements
- **Stream-Based PE Architecture Detection:** Updated `deploy_helper.ps1` to inspect binary architecture using lightweight, seekable stream readers, preventing memory overhead or file locks on large game binaries.
- **Overlay Profile Grid Affordance:** Added subtle card boundary delineation in the profile management grid to improve visibility across varying background themes.
