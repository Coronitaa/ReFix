# Re:Photon — Local Development & LAN Guide

## 1. Zero-Cost Local Testing Architecture

Re:Photon allows complete end-to-end multi-client testing on a single development workstation or across a Local Area Network (LAN) without cloud dependencies or subscription costs:

```
+-------------------+           +-------------------+
|  Client 1 (Host)  |           | Client 2 (Joiner) |
|   (Game + ReFix)  |           |   (Game + ReFix)  |
+-------------------+           +-------------------+
          \                               /
           \                             /
            v                           v
     +-----------------------------------------+
     |         Local Re:Photon Server          |
     |          (127.0.0.1 / Local LAN)        |
     |                                         |
     |   NameServer:   5058 (UDP/TCP)          |
     |   MasterServer: 5055 (UDP/TCP)          |
     |   GameServer:   5056 (UDP/TCP)          |
     +-----------------------------------------+
```

---

## 2. Setting Up Local Development

1. **Step 1: Configuration**
   In `ReFix.ini`, set:
   ```ini
   [RePhoton]
   Enabled=true
   Backend=ReFixCloud

   [RePhoton.Cloud]
   MasterServer=127.0.0.1:5055
   GameServer=127.0.0.1:5056
   NameServer=127.0.0.1:5058
   ```

2. **Step 2: Starting the Server**
   Start the standalone local test server or use the built-in Re:Photon test harness:
   ```cmd
   bin\rephoton_test.exe --server
   ```

3. **Step 3: Launching Game Instances**
   Launch two instances of the target game (e.g. R.E.P.O.). ReFix proxies intercept the Photon initialization and route requests to `127.0.0.1`.

---

## 3. LAN Multiplayer Configuration

To host for friends over a LAN or VPN (ZeroTier / Radmin / Tailscale):
1. In `ReFix.ini`, set `MasterServer=<HOST_LOCAL_IP>:5055` and `GameServer=<HOST_LOCAL_IP>:5056`.
2. Ensure Windows Defender Firewall allows incoming UDP/TCP packets on ports `5055`, `5056`, and `5058`.
