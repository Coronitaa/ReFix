# ReFix LAN / Re:Goldberg — Handoff

## Estado Actual
- **Fases Completadas:** Fase 0, Fase 1 (Auditoría Integral), Fase 2 (Provider Boundary).
- **Commit Actual:** `f5d598a docs: add phase 2 audit`
- **Resultados:** 
  1. Los 5 auditores determinaron cómo se componen los sistemas de red en ReFix v1.3.2. 
  2. El ruteo de tráfico P2P para Steamworks depende actualmente de un hook transparente en Winsock.
  3. Photon y EOS utilizan inyecciones o DLL proxies para desviar tráfico, pero EOS aún consulta su IP pública en WinINet (`api.ipify.org`).
  4. Se implementó la abstracción de `NetworkPolicyManager` (Online, LanOnly, Offline) y `ProviderFactory` para instanciar en tiempo de ejecución las interfaces de red emuladas (`LanSteamProvider` u `OnlineSteamProvider`).
  5. Se testeó con éxito la inyección y lectura de la política en `SteamAPI_Init` de `steam_proxy.cpp` a través de tests compilados. El funcionamiento "Online" quedó intocable.

## Próximo Paso (Phase 3)
El próximo agente debe **iniciar directamente la Fase 3 (Steamworks LAN)**.
- El objetivo inmediato de la Fase 3 es dotar al `LanSteamProvider` (y clases vinculadas) de una funcionalidad real de backend.
- Se debe reestructurar/adaptar el código de `unreal_steam_emu.cpp` (que actualmente ya tiene emulación P2P e ISteamNetworking stub) para que actúe explícitamente bajo la jerarquía del `LanSteamProvider`.
- Prestar especial atención a dotar de comportamiento semántico a `ISteamNetworkingSockets` si es posible o vincularlo correctamente al backend UDP de Multicast ya implementado. 

**Instrucción Inmediata para el Agente:** 
1. Leer `docs/refix-lan/REFIX_LAN_STATE.md`
2. Revisar este `HANDOFF.md` y `docs/refix-lan/call-graph.md`
3. Comenzar la **Fase 3**.
