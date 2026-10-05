# Handle Wire Semantics Audit

## Regla Estructural
El handle `HSteamNetConnection` generado por la API de `ISteamNetworkingSockets` es **estrictamente de uso local** en memoria del cliente. Bajo ninguna circunstancia un cliente debe enviar su handle al Peer remoto esperando que este lo utilice como referencia directa, ya que cada máquina mantiene su propio pool de handles independientes (ej. PC A puede asignar el handle 1000 a la sesión, mientras que PC B asigna el handle 1014).

## Implementación en ReFix LAN (Phase 3.5)
Para garantizar esta separación:
1. **Abstracción de Sesión**: Se introdujo un `sessionId` (generado criptográficamente o pseudoaleatoriamente mediante `GetTickCount64() ^ remoteID`) que identifica unívocamente la intención de conexión P2P entre dos peers.
2. **SocketsHandshake**: El paquete de `msgType = 7` envía la estructura de Handshake conteniendo el `sessionId`, la identidad del invocador, y la versión del protocolo `RFIX`.
3. **Mapeo Independiente**: 
   - Cuando PC A inicia conexión, genera su handle (ej. 1000) y lo asocia al `sessionId X`.
   - Cuando PC B recibe el handshake con `sessionId X`, busca si ya lo conoce. Como no es así, genera un **nuevo handle local** (ej. 1001), e inicializa su propia instancia local de `ReFixConnection` asociada al mismo `sessionId X`.
4. **Data Payload (SocketsPayloadHeader)**: Los mensajes posteriores (`msgType = 6`) transmiten únicamente el `sessionId` en su cabecera junto con los contadores de secuencia y ACK. Al recibirse en la máquina destino, el enrutador UDP busca qué `HSteamNetConnection` local le corresponde a ese `sessionId`, garantizando que jamás se intercambie un Handle a través de la red física.

Esta arquitectura blinda el sistema contra ataques de colisión de handles y respeta enteramente la directiva técnica original de Valve y la restricción del usuario.
