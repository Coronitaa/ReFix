# Re:Photon — Backend Architecture & Configuration Guide

## 1. Supported Backend Providers

Re:Photon provides first-class support for two primary backend providers:

### Provider A: `ReFixCloud`
* **Default Mode:** Designed for direct connectivity to our managed infrastructure or zero-cost local development servers.
* **Architecture:**
  * **Control Plane:** Cloudflare Workers + Durable Objects for Authentication, Matchmaking metadata, Region Directory, and Health Checks.
  * **Data Plane:** Lightweight regional C++ Game Server instances for low-latency UDP event relay and room synchronization.
* **Local Development:** When set to `MasterServer=127.0.0.1:5055`, runs completely locally without external cloud dependencies.

### Provider B: `CustomPhoton`
* **User-Driven:** Allows players and testers to connect using their own official Photon Cloud accounts or private self-hosted Photon/Luxon servers.
* **Privacy by Design:** App IDs, authentication tokens, and custom server addresses remain exclusively local on the user's machine and are never transmitted to ReFix Cloud.
* **Fallback & Verification:** Serves as a golden benchmark to diagnose whether gameplay synchronization issues originate from the game engine or server emulation.

---

## 2. Configuration Reference (`ReFix.ini`)

```ini
; =============================================================================
; Re:Photon Subsystem Configuration
; =============================================================================
[RePhoton]
; Enable/Disable the Re:Photon compatibility subsystem
Enabled=true

; Selected Backend: ReFixCloud | CustomPhoton | OfficialPhoton
Backend=ReFixCloud

; Target Game Profile: Auto | REPO | Phasmophobia | RoadsideResearch | Custom
Profile=Auto

; -----------------------------------------------------------------------------
; Provider A: ReFixCloud Settings
; -----------------------------------------------------------------------------
[RePhoton.Cloud]
; Master Server endpoint for matchmaking and room allocation
MasterServer=127.0.0.1:5055

; Game Server endpoint for active gameplay relay
GameServer=127.0.0.1:5056

; Name Server endpoint for initial region discovery
NameServer=127.0.0.1:5058

; Preferred region identifier: sa | us | eu | asia
PreferredRegion=sa

; Automatically ping and route to the lowest latency region
AutoSelectLowestPing=true

; -----------------------------------------------------------------------------
; Provider B: CustomPhoton Settings
; -----------------------------------------------------------------------------
[RePhoton.Custom]
; User's custom Photon Realtime / PUN Application ID (e.g. GUID)
RealtimeAppId=

; User's custom Photon Voice Application ID
VoiceAppId=

; User's custom Photon Chat Application ID
ChatAppId=

; Target Photon Region (e.g. us, eu, sa, asia, ru)
Region=sa

; Custom server hostname or IP (if self-hosting)
ServerAddress=127.0.0.1

; Custom server port
ServerPort=5055

; Protocol: UDP | TCP | WebSocket | SecureWebSocket
Protocol=UDP

; Client version string expected by the game
ServerVersion=1.0

; -----------------------------------------------------------------------------
; Diagnostics & Logging
; -----------------------------------------------------------------------------
[RePhoton.Diagnostics]
LogLevel=Info
LogAuth=true
LogTransport=false
LogRealtime=true
LogRoom=true
LogVoice=false
LogChat=false

; Automatically redact sensitive Application IDs from logs and console
RedactAppIds=true
```
