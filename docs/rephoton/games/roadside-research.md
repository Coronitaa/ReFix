# Target Game: Roadside Research

## 1. Technical Profile

* **Game Engine:** Unity (Mono/IL2CPP x64)
* **Networking Framework:** Photon Realtime / PUN
* **Observed Network Behavior:** Standard room creation, lobby listings, custom properties, and reliable/unreliable state replication.

---

## 2. Research & Compatibility Verification

* **Authentication:** Standard `OpAuthenticate` with AppID and version validation.
* **Room Lifecycle:** Standard `OpCreateGame` with player slots and room visibility flags.
* **Event Dispatch:** In-game actor interactions transmitted via `OpRaiseEvent` with event codes in the 1–199 range.

---

## 3. Current Status

* **Status:** `Investigating`
* **Realtime Matchmaking:** Protocol compatible with Re:Photon Realtime.
