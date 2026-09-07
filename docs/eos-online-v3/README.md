# ReFix EOS v3 — emulador de Epic Online Services

Reimplementación completa del emulador EOS de ReFix (el que reemplaza a
`EOSSDK-Win64-Shipping.dll` / `RedboneEOS.dll`). Sustituye al proxy heredado de
`src/eos_proxy.cpp`, que traducía llamadas EOS a Steamworks mediante heurísticas
sobre memoria cruda.

- Código: `src/eossdk/`
- Headers oficiales del SDK vendorizados: `src/eossdk/sdk/` (EOS SDK 1.19.1)
- Tabla ABI generada: `src/eossdk/gen/eos_export_table.inc` (`tools/gen_eos_abi.py`)
- Prueba de interconexión de dos procesos: `tests/test_two_instance_lobby.cpp`

## Por qué se reescribió

El diagnóstico de la rama `feature/eos-online-v2` (ver `docs/eos-online-v2/`)
identificó los síntomas correctos —lobbies fantasma de 4 plazas, `AlreadyExists`
al recrear sesión, invitaciones que no llegan, PUIDs duplicados— pero la causa
común quedaba fuera de foco: **los structs del SDK estaban escritos a mano y no
coincidían con los reales**.

Se confirmó en ejecución. Con la v2 desplegada, *MECCHA CHAMELEON* crasheaba
(`0xC0000005`) inmediatamente después de un `EOS_Connect_Login` correcto. El
minidump situaba el fallo dentro de Redpoint, en la ruta
`EOS_Platform_Tick → CallbackManager::FlushCallbacks → callback del juego →
EOS_Connect_CopyProductUserExternalAccountByIndex`. El motivo:

```c
// v2 (src/eos/eos_connect.h) — orden inventado
struct EOS_Connect_ExternalAccountInfo {
    int32_t ApiVersion; EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    int32_t     AccountIdType;   // <-- invertidos
    const char* AccountId;
    int64_t LastLoginTime;
};

// SDK real (eos_connect_types.h)
EOS_STRUCT(EOS_Connect_ExternalAccountInfo, (
    int32_t ApiVersion; EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    const char* AccountId;                  // primero
    EOS_EExternalAccountType AccountIdType; // después
    int64_t LastLoginTime;
));
```

Redpoint leía `AccountId` de un campo que contenía el entero `1` y lo
desreferenciaba. Ninguna corrección puntual elimina esa clase de fallo: la
solución es estructural.

## Principios de diseño

1. **Compilar contra los headers oficiales.** El emulador incluye el SDK real y
   define las funciones con sus firmas declaradas. El compilador valida cada
   struct y cada prototipo; un desajuste de ABI es ahora un error de compilación,
   no un crash en el cliente.

2. **Nada hardcodeado.** Plazas, bucket, nivel de permiso, atributos, puertos e
   identidades salen de lo que el juego configuró o de `ReFix.ini`. La tabla de
   679 exports se genera desde los headers, así que actualizar el SDK es
   regenerar un `.inc`.

3. **El stub genérico responde según el tipo de retorno.** Una API no
   implementada que devuelve `EOS_EResult` responde `EOS_NotFound`, no
   `EOS_Success` con el parámetro de salida sin tocar —ese era el segundo modo de
   crash más común—. Las 118 APIs asíncronas no implementadas *sí* invocan su
   delegado, para que el juego nunca quede esperando.

4. **El anfitrión es la autoridad.** Un lobby es lo que su host dice que es. Los
   demás replican. No hay estado local que pueda divergir.

5. **Identidad derivada, nunca inventada.** El PUID sale de un SHA-256 sobre el
   SteamID64 que viene *dentro del ticket que el propio juego entrega* a
   `EOS_Connect_Login`, con el ProductId del título como sal. Funciona con
   cualquier cuenta de Steam en cualquier PC, sin configurar nada, y dos máquinas
   distintas calculan el mismo PUID para el mismo jugador.

Documentos: [`architecture.md`](architecture.md) · [`identity.md`](identity.md) ·
[`transport.md`](transport.md) · [`testing.md`](testing.md) ·
[`api-coverage.md`](api-coverage.md)
