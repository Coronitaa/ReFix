# ReFix-LAN Network Policy Enforcement Audit

## 1. Overview
The `NetworkPolicyManager` tracks the desired networking mode (`Online`, `LanOnly`, `Offline`) based on the `Unreal.Networking->Mode` INI setting. It exposes `IsInternetAllowed()` and `IsLanAllowed()`. This audit evaluates how well this policy is enforced across network access points like Winsock (`connect`, `sendto`, `recvfrom`), `WinInet`, HTTP, and specific subsystems (EOS, Photon, Steam SDR/Relay).

## 2. Policy Adherence by Subsystem

### 2.1 ISteamHTTP
*   **Status**: **Fully Compliant**
*   **Analysis**: `ISteamHTTP` is completely stubbed and mocked locally in `unreal_steam_emu.cpp` (`CSteamHTTPEmu`). Functions like `SendHTTPRequest` instantly complete successfully with a 200 OK and an empty body via `PostCallResult`.
*   **Enforcement**: Requires no enforcement because it never initiates physical network requests.

### 2.2 Steam SDR / Relay
*   **Status**: **Compliant (By Nature of Hook)**
*   **Analysis**: The `GetRelayNetworkStatus` and `InitRelayNetworkAccess` functions are intercepted at the export and VTable level in `steam_proxy.cpp`. The hooks bypass Steam Datagram Relay initialization by mocking network availability.
*   **Enforcement**: Because the hook intercepts logic before SDR traffic can occur, SDR does not contact the internet.

### 2.3 Public IP Lookup (api.ipify.org)
*   **Status**: **Vulnerable / Unenforced**
*   **Analysis**: `upnp_firewall.cpp` implements `GetPublicIP()`, which uses `WinInet` (`InternetOpenUrlA`) to make an HTTP request to `http://api.ipify.org`.
*   **Trigger Points**: This is called asynchronously by `eos_proxy.cpp` to resolve external account information and bindings.
*   **Enforcement Gap**: It does not query `NetworkPolicyManager::IsInternetAllowed()`. Thus, even if the policy is set to `LanOnly` or `Offline`, the EOS proxy leaks an outbound internet HTTP request to `ipify.org`.

### 2.4 P2P Winsock Hook (`steam_p2p_hook.cpp`) & Photon
*   **Status**: **Vulnerable / Unenforced**
*   **Analysis**: The Winsock hooks (`Hook_sendto`, `Hook_connect`) in `steam_p2p_hook.cpp` attempt to intercept traffic targeted at known Steam peers and route it via P2P.
*   **Trigger Points**: When third-party networking solutions like Photon Fusion or Nanosockets attempt to reach their public relay servers, their destination IPs do not match known SteamIDs in the `g_ipToSteamID` table.
*   **Enforcement Gap**: When a peer is unmatched, the hooks silently fall through to the original Winsock functions (`g_orig_sendto` and `g_orig_connect`), passing the packets through directly to the internet. `NetworkPolicyManager::IsInternetAllowed()` is never checked before doing this. This allows Photon (and any other direct UDP traffic) to freely bypass the `LanOnly` or `Offline` setting and communicate over the internet.

## 3. Summary of Currently Blocked/Permitted Traffic

*   **Permitted by Design (LAN)**: 
    *   UDP broadcasts to `255.255.255.255` on port 47584 (LAN peer discovery).
    *   Unicast UDP to local IP addresses for Steam P2P proxying.
*   **Blocked by Design**: 
    *   Steam Web API / HTTP requests (mocked).
    *   Steam Datagram Relay initialization (hooked/mocked).
*   **Permitted by Flaw (Leaks to Internet)**:
    *   **WinInet**: `http://api.ipify.org` public IP lookup triggered by EOS (`GetPublicIP()`).
    *   **Winsock**: Outbound UDP/TCP connections to unknown external IPs (Photon relays, telemetry, etc.) falling through `steam_p2p_hook.cpp` due to lack of policy checks.

## 4. Remediation Requirements
1.  **Enforce Policy in `GetPublicIP()`**: Wrap the `WinInet` request in `upnp_firewall.cpp` with `if (!ReFix::NetworkPolicyManager::IsInternetAllowed()) return GetLocalIP();`.
2.  **Enforce Policy in Winsock Hooks**: In `steam_p2p_hook.cpp`, when falling back to `g_orig_sendto` or `g_orig_connect`, classify the destination IP. If it is a public internet address (not localhost, and not in the `192.168.x.x`, `10.x.x.x`, `172.16.x.x` ranges), check `NetworkPolicyManager::IsInternetAllowed()`. If disallowed, block the packet and return a simulation of success or `WSAEADDRNOTAVAIL` to gracefully prevent outbound traffic while maintaining game stability.
