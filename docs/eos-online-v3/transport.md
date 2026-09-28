# Transporte y directorio de salas

## Un solo socket

Cada proceso abre un socket UDP en un puerto efímero. Por ahí va todo: anuncios,
peticiones de join, réplicas de estado y tráfico P2P del juego.

El descubrimiento usa un **grupo multicast IPv4** (`239.255.71.84:47584` por
defecto, configurable en `[Network]`), más un broadcast dirigido como respaldo
porque algunas redes y adaptadores virtuales filtran multicast. Un anuncio
duplicado es inocuo; una sala que no aparece, no.

Esto resuelve los dos escenarios con el mismo código:

- **dos instancias en un PC** — el multicast vuelve por loopback
  (`IP_MULTICAST_LOOP`), y el socket del grupo se abre con `SO_REUSEADDR` para
  que varios procesos compartan el puerto;
- **dos PCs en la misma LAN** — los mismos datagramas llegan a la otra máquina.

## Protocolo

Binario, con longitudes explícitas. Cada datagrama empieza por `RFX3` y el
*scope* del título (el ProductId de EOS), de modo que dos juegos distintos en la
misma red nunca se ven las salas.

| Mensaje | Dirección | Contenido |
| --- | --- | --- |
| `ANNOUNCE` | host → grupo, cada 2 s | registro completo del lobby |
| `WITHDRAW` | host → grupo | la sala se cerró |
| `QUERY` | buscador → grupo | pide reanuncio inmediato |
| `JOIN_REQ` | cliente → host | puid, nombre, endpoint |
| `JOIN_RSP` | host → cliente | resultado + registro completo |
| `LEAVE` | cliente → host | salida limpia |
| `UPDATE` | host → miembros y grupo | nuevo estado autoritativo |
| `KICK` | host → cliente | expulsión |
| `INVITE` | miembro → destino | id de invitación + registro |
| `P2P` | par ↔ par | socketName, canal, payload |

## Autoridad del anfitrión

El host es el único que modifica el registro. Un cliente que quiere cambiar sus
atributos de miembro se lo pide al host. Un anuncio remoto nunca puede
sobrescribir una sala propia, y un `UPDATE` con revisión más vieja que la cacheada
se descarta.

El `JOIN_REQ` lo valida el host contra las plazas reales y el nivel de permiso; la
dirección de origen del datagrama gana sobre la que el mensaje declare.

## Entrada no confiable

Todo lo que llega de la red pasa por un lector con comprobación de límites en
cada campo. Longitudes de cadena, número de atributos y número de miembros tienen
techo; un datagrama truncado o malicioso hace fallar el parseo en vez de
provocar una lectura fuera de rango.
