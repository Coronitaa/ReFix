# ReFix LAN / Re:Goldberg — Handoff

## Estado Actual
- **Fases Completadas:** Fase 0, Fase 1, Fase 2, y Fase 2.1 (Hardening e Internet-Zero).
- **Commit Actual:** `563a7d9 refix-lan: phase 2.1 provider hardening and internet-zero enforcement`
- **Resultados Phase 2.1:** 
  1. Se bloqueó proactivamente el tráfico de WinInet (`api.ipify.org`) y Winsock (`sendto`/`connect`) hacia redes externas si `NetworkModeManager::IsOnline()` es falso.
  2. Todas las 7 rutas de inicialización de SteamAPI están unificadas bajo `ReFixInitializePre()` y `ReFixInitializePost()`, asegurando que el provider se cree y el ruteo de hooks sea inequívoco.
  3. Los subagentes auditaron Callbacks y SDR comprobando que Relay aborta limpiamente sin red externa en LAN, y que `ISteamNetworkingSockets` actual carece de transmisión de datos real.

## Próximo Paso (Phase 3)
El próximo agente debe **iniciar la Fase 3 (Steamworks LAN)**.
- El objetivo inmediato de la Fase 3 es dotar al `LanSteamProvider` (y clases vinculadas) de una funcionalidad real de backend.
- Se debe reestructurar/adaptar el código de `unreal_steam_emu.cpp` (que actualmente ya tiene emulación P2P e ISteamNetworking stub) para que actúe explícitamente bajo la jerarquía del `LanSteamProvider`.
- Hay un requerimiento urgente: `ISteamNetworkingSockets` debe ser implementado y/o el transporte UDP de Winsock modificado para que el LAN matchmaking y transmisión funcione apropiadamente usando Unicast en lugar del Broadcast destructivo actual.

**Instrucción Inmediata para el Agente:** 
1. Leer `docs/refix-lan/REFIX_LAN_STATE.md` y `docs/refix-lan/audits/steamnetworkingsockets-diag.md`
2. Comenzar la **Fase 3**.
