# REGRESSION TESTS: Transport Layer & Socket Routing Verification

## 1. Objetivo del Harness
Proveer un conjunto de pruebas unitarias y de integración en C++ (`tests/test_p2p_network_harness.cpp`) capaz de simular de forma determinista la topología de red, el comportamiento de sockets UDP, puertos efímeros, buffers de retención, y la capa de redirección Steam P2P sin requerir un juego completo ni una sesión viva de Steamworks.

## 2. Casos de Prueba Implementados

| # | Identificador del Test | Escenario Simulado | Comportamiento en v1.3.1 (Baseline) |
|---|------------------------|--------------------|-------------------------------------|
| 1 | `test_known_peer_send` | Envío a una IP mapeada legítima de un par conocido. | **PASSED** |
| 2 | `test_unknown_peer_send` | Envío a una IP no mapeada antes de la resolución del lobby. | **FAILED** (`[REGRESSION DETECTED] Packet leaked to raw Winsock before peer resolution!`) |
| 3 | `test_late_peer_resolution` | Retención en buffer y posterior despacho al registrarse el par vía `RegisterPeer`. | **FAILED** (Fuga previa y desincronización de cola) |
| 4 | `test_single_peer_fallback` | Redirección heurística a loopback / subnet cuando hay exactamente 1 par. | **PASSED** |
| 5 | `test_multiple_peer_no_ambiguous_route` | Destino no mapeado con >1 par no debe enrutarse arbitrariamente al par erróneo. | **PASSED** |
| 6 | `test_incoming_exact_socket_route` | Paquete P2P con puerto destino coincidente se entrega al socket local exacto. | **PASSED** |
| 7 | `test_incoming_ephemeral_socket` | Socket cliente UDP ligado a puerto efímero (`bind(0)`); verificación de `select()` y entrega. | **FAILED** (`[REGRESSION DETECTED] select() failed to signal readability for ephemeral client socket!`) |
| 8 | `test_nonstandard_game_port` | Juego en puerto no estándar (ej. 9999 o 14000); clasificación y notificación en `select()`. | **FAILED** (`[REGRESSION DETECTED] select() failed on nonstandard port 9999!`) |
| 9 | `test_game_and_voice_isolation` | Aislamiento entre tráfico de juego (canal 0) y voz (canal 1 / puerto 5058). | **PASSED** |
| 10 | `test_legacy_unframed_packet` | Recepción de paquetes sin cabecera `REFX` (compatibilidad con nodos legacy o sin framing). | **PASSED** |
| 11 | `test_hold_buffer_flush` | Verificación de que `FlushHoldBuffer()` vacía datagramas al resolver pares. | **PASSED** |
| 12 | `test_hold_buffer_expiry` | Expiración segura de datagramas tras TTL de 3000 ms sin crash. | **PASSED** |
| 13 | `test_socket_close_cleanup` | Cierre de socket con `Hook_closesocket` limpia adecuadamente contextos y colas. | **PASSED** |

---

## 3. Resultado de la Ejecución en el Baseline v1.3.1 (Commit 577f1bc)
```
====================================================================
[*] Running ReFix P2P Network Regression Test Harness...
====================================================================
[RUN ] test_known_peer_send... PASSED
[RUN ] test_unknown_peer_send... [REGRESSION DETECTED] Packet leaked to raw Winsock before peer resolution!
FAILED
[RUN ] test_late_peer_resolution... FAILED
[RUN ] test_single_peer_fallback... PASSED
[RUN ] test_multiple_peer_no_ambiguous_route... PASSED
[RUN ] test_incoming_exact_socket_route... PASSED
[RUN ] test_incoming_ephemeral_socket... [REGRESSION DETECTED] select() failed to signal readability for ephemeral client socket!
FAILED
[RUN ] test_nonstandard_game_port... [REGRESSION DETECTED] select() failed on nonstandard port 9999!
FAILED
[RUN ] test_game_and_voice_isolation... PASSED
[RUN ] test_legacy_unframed_packet... PASSED
[RUN ] test_hold_buffer_flush... PASSED
[RUN ] test_hold_buffer_expiry... PASSED
[RUN ] test_socket_close_cleanup... PASSED
====================================================================
[SUMMARY] Total: 13 | Passed: 9 | Failed: 4
====================================================================
```

El harness demuestra formalmente los puntos de falla identificados en el análisis forense.
