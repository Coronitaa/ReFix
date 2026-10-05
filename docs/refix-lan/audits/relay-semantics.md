# Phase 2.2 Audit: Relay and SDR Semantics

## API: `InitRelayNetworkAccess`
* **Comportamiento Esperado por el Juego:** El juego llama a esta API para iniciar la resolución de los POPs (Points of Presence) y obtener latencias globales con los datacenters de Valve.
* **Online (Valve DLL):** La solicitud se despacha a la DLL original, la cual consulta la infraestructura WAN.
* **LAN/Offline:** Interceptado en `steam_proxy.cpp`. Se retorna inmediatamente sin generar tráfico y se dispara de forma sintética el callback `SteamRelayNetworkStatusChanged_t` indicando disponibilidad de red (`k_eRelayAvail_Current`).
* **Conclusión LAN:** No genera tráfico externo.

## API: `GetRelayNetworkStatus`
* **Comportamiento Esperado por el Juego:** Consultar si los datacenters de Valve están disponibles (status code 100 o similar). Muchos motores de juego detienen el matchmaking si el estado es de error o timeout.
* **Online (Valve DLL):** Forward a la implementación original.
* **LAN/Offline:** Se devuelve una estructura simulada (`SteamRelayNetworkStatus_t`) en donde el campo principal de estado está fijado a `k_eRelayAvail_Current`.
* **Conclusión LAN:** Satisface la aserción interna del juego (Ej. Unreal Engine `FSteamSocketsSubsystem`) permitiendo que proceda la lógica de red local sin interrupciones por timeout de WAN.
