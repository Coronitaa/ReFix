# Target Game: R.E.P.O. (Semiwork)

## 1. Technical Profile

* **Game Engine:** Unity (Mono / IL2CPP x64)
* **Networking Framework:** Photon Unity Networking 2 (PUN 2) / Photon Realtime
* **Audio Framework:** Photon Voice 2 (Spatial/Positional Proximity Voice)
* **Default Transport:** UDP (Protocol 1.6 / 1.8)
* **Configuration Mechanism:** `PhotonServerSettings` ScriptableObject / `AppSettings` class

---

## 2. Configurable Network Parameters

Based on public research and modding investigations (`REPO-PhotonServerSettings` & `REPO-LocalMultiplayer`):
* **Realtime App ID:** Overridable at runtime.
* **Voice App ID:** Overridable at runtime.
* **Server Address & Port:** Supports redirection to `127.0.0.1:5055` or custom Re:Photon endpoints.
* **Protocol:** UDP (default) / TCP / WebSocket.
* **Send Rate & Serialization Rate:** Typically 20–30 packets/sec.

---

## 3. Test Harness Flow

```
[Client 1 - Host]                           [Client 2 - Joiner]
       |                                             |
       |--- OpAuthenticate(AppId, Version) --------->|
       |<-- OpAuthenticate Response (OK) ------------|
       |                                             |
       |--- OpCreateGame("REPO_Room_1", MaxPlayers=4)|
       |<-- OpCreateGame Response (ActorNr=1) -------|
       |                                             |
       |                                             |--- OpAuthenticate(AppId, Version) ---------->
       |                                             |<-- OpAuthenticate Response (OK) -------------
       |                                             |
       |                                             |--- OpJoinGame("REPO_Room_1") --------------->
       |                                             |<-- OpJoinGame Response (ActorNr=2) ----------
       |                                             |
       |<-- EventJoin (ActorNr=2) -------------------|
       |                                             |
       |<================== Gameplay Event Relay (RPCs) ================>|
       |                                             |
       |                                             |--- OpLeave() ------------------------------->
       |<-- EventLeave (ActorNr=2) ------------------|
```

---

## 4. Current Status

* **Status:** `RoomCompatible`
* **Realtime Multiplayer:** Fully compatible with Re:Photon Realtime/PUN adapter.
* **Voice:** Stubbed for Milestone 1 (positional voice adapter).
