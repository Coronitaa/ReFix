# Target Game: Phasmophobia (Kinetic Games)

## 1. Technical Profile

* **Game Engine:** Unity (IL2CPP x64)
* **Networking Framework:** Photon Unity Networking (PUN) / Photon Realtime
* **Voice Framework:** Photon Voice
* **Redirection Vector:** Interception of MasterServer/NameServer DNS endpoints or runtime hooking via proxy DLLs (as demonstrated by the UltimatePhobia clean-room research).

---

## 2. Multiplayer Architecture

* **Room Matching:** Room code-based private lobbies (6-digit alphanumeric codes) and public lobby lists.
* **Actor Assignment:** Up to 4 players per room. Master Client hosts ghost AI and environmental triggers.
* **Custom Properties:** Player readiness, selected equipment, ghost type hashes, and difficulty modifiers synchronized through Room Custom Properties.

---

## 3. Current Status

* **Status:** `Investigating`
* **Realtime Matchmaking:** Protocol compatible with Re:Photon Realtime.
* **Next Steps:** Verification of ghost AI state relay and custom VR/Desktop player synchronization.
