# Phase 2.2 Audit: Network Block Semantics

## 1. Concepto de "Private IP" / LAN Endpoint
El criterio para permitir tráfico en modo LAN/Offline ha sido expandido en `IsAllowedLanEndpoint()`.
* **IPv4 permitidas:** `127.0.0.0/8` (Loopback), `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `169.254.0.0/16` (Link-local), `224.0.0.0/4` (Multicast, incluye `239.255.71.84`), y `255.255.255.255` (Broadcast).
* **IPv6 permitidas:** `::1` (Loopback), `fe80::/10` (Link-local), `fc00::/7` (ULA), y `ff00::/8` (Multicast).
* **Bloqueadas:** Cualquier IP externa (ej. `8.8.8.8`).

## 2. Bloqueos de Transporte (Winsock)
El proxy P2P de ReFix (`steam_p2p_hook.cpp`) intercepta rutinas base de Winsock:
* **`connect`**: Si el socket intenta conectar a una IP bloqueada y `!IsExternalNetworkingAllowed()`, se intercepta y devuelve `SOCKET_ERROR` seteando `WSAEHOSTUNREACH`. *Justificación:* Bloqueo explícito de establecimiento TCP/UDP hacia WAN.
* **`sendto`**: Si el datagrama UDP se dirige a una IP bloqueada, el interceptor devuelve `len` falsificando éxito ("Synthetic success"). *Justificación:* Muchos motores como Unreal/Unity bombardean telemetría o STUN vía UDP *fire-and-forget* y crashean/spamean logs si sendto() falla bruscamente.
* **`WSAConnect` / `WSASendTo` / `send`**: No están hookeados directamente en el proxy actual. *Justificación:* `send` depende de un `connect` previo (ya bloqueado). `WSAConnect`/`WSASendTo` podrían ser un vector de fuga si el motor los usa en lugar de las rutinas POSIX base, pero los motores estándar invocados usan `connect`/`sendto`.

## 3. Bloqueos HTTP/DNS
* **WinInet (`GetPublicIP`)**: Modificado en `upnp_firewall.cpp`. La llamada a `api.ipify.org` ni siquiera abre el session HTTP; se aborta proactivamente (Block) retornando la IP local. *Justificación:* Prevención de leakage HTTP/DNS hacia infraestructura cloud.
* **DNS (`getaddrinfo` / `DnsQuery`)**: Actualmente no se hookean las APIs de resolución DNS. *Justificación:* En LAN, aunque un DNS resuelva una IP pública, el intento de conexión posterior será bloqueado en `connect` o `sendto`. Sin embargo, es un área posible de endurecimiento futuro si se detecta exfiltración de telemetría a través de las consultas DNS en sí.

## 4. Matriz de Transporte a Nivel de ReFix
| Channel | LAN           | Online  | Offline                 |
| ------- | ------------- | ------- | ----------------------- |
| UDP     | allowed local | allowed | blocked except loopback |
| TCP     | allowed local | allowed | blocked except loopback |
| DNS     | local/none    | allowed | blocked/none            |
| HTTP    | no external   | allowed | blocked                 |
| HTTPS   | no external   | allowed | blocked                 |
| IPv6    | local         | allowed | blocked external        |
