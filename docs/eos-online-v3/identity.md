# Identidad

## De dónde sale el jugador

Por orden de preferencia:

1. **El ticket de sesión de Steam que el propio juego entrega** a
   `EOS_Connect_Login`. Un app ticket empieza con un bloque GC de 20 bytes:

   ```
   uint32 longitudBloque (=20) │ uint64 gcToken │ uint64 steamId │ uint32 timestamp
   ```

   El SteamID64 está en el offset 12. Se valida que los 32 bits altos sean
   `0x01100001` (cuenta individual, universo público). No hace falta hablar con
   Steam ni acoplar el emulador EOS al proxy de Steam: la identidad viene dentro
   de la credencial que el juego ya presentó.

2. `[User] SteamId` en `ReFix.ini` (o `[Unreal.Steam] SteamId`).

3. Huella de hardware (nombre del equipo + serie del volumen), para títulos que
   autentican con DeviceId. En este caso el jugador **no** declara cuenta externa,
   y `EOS_Connect_GetProductUserExternalAccountCount` devuelve 0 —decir la verdad
   evita que el llamante recorra una lista que no existe.

## Derivación

```
ámbito = ProductId de EOS_Platform_Create   (o DeploymentId, o config, o nombre del exe)
sal    = ámbito | idExterno | instancia

PUID = hex(SHA-256("refix.eos.puid|" + sal))[0..32]
EAID = hex(SHA-256("refix.eos.eaid|" + sal))[0..32]
```

Consecuencias buscadas:

- **La misma cuenta de Steam da el mismo PUID en cualquier PC**, que es lo que
  permite que dos máquinas se reconozcan.
- **Cada juego da un PUID distinto** para la misma cuenta, igual que EOS real.
- `REFIX_USER_INSTANCE` (o `[User] Instance`) añade un sufijo a la sal: dos
  copias del mismo juego en el mismo PC se convierten en dos jugadores
  independientes. Es lo que hace posible probar crear y unirse a una sala sin una
  segunda máquina.

## Interning

`EOS_ProductUserId` y `EOS_EpicAccountId` son punteros opacos. EOS garantiza que
la misma cuenta devuelve siempre el mismo puntero, y Redpoint compara con `==` y
usa los punteros como claves. Aquí los ids se internan una vez y no se destruyen.

La versión heredada usaba un `char[64]` estático compartido: todos los jugadores
remotos acababan con el PUID del anfitrión local, lo que rompía el registro de
jugadores y la conexión del NetDriver.
