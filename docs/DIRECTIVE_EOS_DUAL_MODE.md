# DIRECTIVA TÉCNICA ESTRICTA: ARQUITECTURA DUAL-MODE EOS SDK (PASSTHROUGH VS EMULATED)
**Versión:** 1.0  
**Fecha:** 27 de Septiembre de 2026  
**Rama Git:** `feature/eos-online-v3`  
**Repositorio:** `D:\EOS_REFIX\ReFix-git`  
**Deploy:** `D:\EOS_REFIX\ReFix_Release_v2.0-pre`

---

## 1. Resumen Ejecutivo y Propósito
El propósito de esta directiva es estandarizar la arquitectura del middleware de compatibilidad de Epic Online Services (EOS) en ReFix (`src/eossdk/`), permitiendo soportar sin fricción dos vertientes de juego completamente distintas:

1. **Modo Passthrough (Valve Online WAN - Spacewar AppID 480)**:
   - Actúa como una capa delgada e inteligente por encima del SDK genuino de Epic Games (`EOSSDK_original.dll`).
   - Reenvía dinámicamente los 679 exports del SDK de EOS al binario genuino, permitiendo a los juegos conectarse directamente a la infraestructura oficial en la nube de Epic (`api.epicgames.dev`), sincronizar la hora del dispositivo, consultar la lista de servidores dedicados públicos mundiales y usar la red Epic Voice/P2P.
   - Intercepta `EOS_Connect_Login` cuando `[EOS] DeviceIdAuth=true` para transformar los tickets de sesión de Steam Spacewar (AppID 480) en credenciales de invitado `DeviceId` de Epic, evitando el rechazo de autenticación (`EOS_InvalidAuth`) por discordancia de AppID.
   - Sintetiza la información de cuenta externa para `EOS_EAT_STEAM` en `EOS_Connect_CopyProductUserExternalAccountByAccountType` para que `OnlineSubsystemEOS` de Unreal Engine muestre el nombre de usuario (Persona Name) y SteamID64 sin crasheos.

2. **Modo Emulated (Re:Goldberg LAN / Offline)**:
   - Mantiene activo el emulador nativo standalone ReFix EOS v3 (RedboneEOS).
   - Enruta la búsqueda y anuncio de partidas a través de multicast UDP local (`239.255.71.84:47584` vía `refix_transport.cpp`) y lobbies locales de Spacewar (`steam_backend.cpp`), permitiendo jugar en LAN física o VPNs (Radmin / Hamachi) sin necesidad de conexión a los servidores de Epic Games.

---

## 2. Esquema de Configuración (`ReFix.ini`)

En la sección `[EOS]` de `ReFix.ini`:
```ini
[EOS]
# Modo de operación del SDK de EOS:
#   auto        = Detecta automáticamente según [Online] Mode y presencia de EOSSDK_original.dll.
#   passthrough = Reenvía al SDK genuino (EOSSDK_original.dll) con intercepción DeviceIdAuth.
#   emulated    = Utiliza el emulador nativo standalone ReFix EOS v3 (RedboneEOS para LAN).
Mode = auto

# Habilita la autenticación de dispositivo (DeviceId guest credentials) en modo Passthrough.
# Recomendado: true para Spacewar (AppID 480) hacia servidores dedicados sin Easy Anti-Cheat.
# Si el usuario es dueño legítimo del juego y conecta a servidores con EAC, fijar en false.
DeviceIdAuth = true
```

### Reglas de Decisión Automática (`Mode = auto`)
1. Si `[Online] Mode = valve` Y existe `EOSSDK_original.dll` (en el directorio del DLL, del ejecutable o en `RedpointEOS\`):
   $\to$ **Seleccionar `passthrough`**.
2. Si `[Online] Mode = goldberg` O NO existe `EOSSDK_original.dll`:
   $\to$ **Seleccionar `emulated`**.

---

## 3. Especificación Técnica de la Máquina de Estados `DeviceIdAuth`

Cuando `[EOS] Mode = passthrough` y `[EOS] DeviceIdAuth = true`, la función `EOS_Connect_Login` debe interceptarse y ejecutar la siguiente máquina de estados asíncrona de 3 fases:

```mermaid
stateDiagram-v2
    [*] --> CheckCredentials
    CheckCredentials --> ForwardLogin : Type != STEAM_SESSION_TICKET (18) && != STEAM_APP_TICKET (1)
    CheckCredentials --> Stage1_CreateDeviceId : Type == STEAM_TICKET
    
    state Stage1_CreateDeviceId {
        Call_Genuine_CreateDeviceId --> Wait_CreateDeviceId_Cb
        Wait_CreateDeviceId_Cb --> Advance_To_Stage2 : Result == EOS_Success (0) || EOS_DuplicateNotAllowed (16)
        Wait_CreateDeviceId_Cb --> Fail_Login : Result == Other Error
    }
    
    state Stage2_LoginWithDeviceId {
        Advance_To_Stage2 --> Call_Genuine_Login_DeviceId
        Call_Genuine_Login_DeviceId --> Wait_Login_Cb
        Wait_Login_Cb --> Deliver_Success_To_Game : Result == EOS_Success (0)
        Wait_Login_Cb --> Stage3_CreateUser : Result == EOS_InvalidUser (36)
        Wait_Login_Cb --> Fail_Login : Result == Other Error
    }
    
    state Stage3_CreateUser {
        Stage3_CreateUser --> Call_Genuine_CreateUser : With ContinuanceToken
        Call_Genuine_CreateUser --> Wait_CreateUser_Cb
        Wait_CreateUser_Cb --> Deliver_Success_To_Game : Result == EOS_Success (0)
        Wait_CreateUser_Cb --> Fail_Login : Result == Other Error
    }
    
    Deliver_Success_To_Game --> [*]
    Fail_Login --> [*]
```

### Especificación de Estructuras y Parámetros

1. **Fase 1 (`EOS_Connect_CreateDeviceId`)**:
   - `EOS_Connect_CreateDeviceIdOptions`:
     - `ApiVersion = 1` (`EOS_CONNECT_CREATEDEVICEID_API_LATEST`).
     - `DeviceModel = "PC Windows"`.
   - Si el callback retorna `EOS_Success` (0) O `EOS_DuplicateNotAllowed` (16): **Avanzar a Fase 2**.
   - *Nota Crítica:* `EOS_DuplicateNotAllowed` significa que ya existe un Device ID registrado para este usuario de Windows; **no es un fallo**.

2. **Fase 2 (`EOS_Connect_Login` con Device ID)**:
   - `EOS_Connect_Credentials`:
     - `ApiVersion = 1` (`EOS_CONNECT_CREDENTIALS_API_LATEST`).
     - `Type = 10` (`EOS_ECT_DEVICEID_ACCESS_TOKEN`).
     - `Token = nullptr` *(Obligatorio por especificación de Epic: pasar un string causará `EOS_InvalidParameters`)*.
   - `EOS_Connect_UserLoginInfo`:
     - `ApiVersion = 2` (`EOS_CONNECT_USERLOGININFO_API_LATEST`).
     - `DisplayName`: Nombre de usuario Steam (Persona Name) codificado en UTF-8, truncado a un máximo de 32 bytes (`EOS_CONNECT_USERLOGININFO_DISPLAYNAME_MAX_LENGTH`).
   - Si retorna `EOS_Success` (0): Entregar directamente al callback del juego (`CompletionDelegate`).
   - Si retorna `EOS_InvalidUser` (36): Capturar `ContinuanceToken` y **avanzar a Fase 3**.

3. **Fase 3 (`EOS_Connect_CreateUser`)**:
   - `EOS_Connect_CreateUserOptions`:
     - `ApiVersion = 1` (`EOS_CONNECT_CREATEUSER_API_LATEST`).
     - `ContinuanceToken = capturedToken`.
   - Si retorna `EOS_Success` (0):
     - Sintetizar un `EOS_Connect_LoginCallbackInfo` con `ResultCode = EOS_Success`, `LocalUserId = data->LocalUserId`, `ClientData = originalClientData`.
     - Invocar el `CompletionDelegate` original del juego.
   - Si falla: Invocar el `CompletionDelegate` original con el código de error correspondiente.

---

## 4. Síntesis de Cuentas Externas (`OnlineSubsystemEOS`)

En juegos Unreal Engine que utilizan `OnlineSubsystemEOS`, la inicialización del subsistema consulta las cuentas externas vinculadas:
- **`EOS_Connect_GetProductUserExternalAccountCount`**:
  - Si el PUID corresponde al usuario local autenticado, retornar al menos `1`.
- **`EOS_Connect_CopyProductUserExternalAccountByAccountType`**:
  - Si `TargetAccountType == EOS_EAT_STEAM` (0), sintetizar un struct `EOS_Connect_ExternalAccountInfo`:
    - `ApiVersion = 1`.
    - `AccountId`: String con el SteamID64 en formato decimal.
    - `DisplayName`: Persona Name del jugador de Steam.
    - `AccountType = EOS_EAT_STEAM`.
    - `LastLoginTime = current_timestamp`.
  - Esto previene que Unreal Engine descarte al jugador local o deje su nombre en blanco en la UI del juego.

---

## 5. Prevención de Deadlocks y Loader-Lock

1. **Tabla de Despacho Segura (`g_eosProcs`)**:
   - Durante `DllMain` (`DLL_PROCESS_ATTACH`), **siempre** ejecutar `BuildStubs()` y rellenar los 679 slots de `g_eosProcs` con `&g_stubs[i]`. Esto garantiza que ninguna llamada al SDK antes de la inicialización completa sufra una desreferencia nula (`0xC0000005`).
2. **Carga Perezosa de `EOSSDK_original.dll`**:
   - No realizar operaciones pesadas de red ni resolver DLLs externas que puedan desencadenar cargas circulares dentro del bloqueo del cargador del sistema operativo (`LoaderLock`).
   - La resolución de `EOSSDK_original.dll` y la población de `g_eosProcs` debe ser atómica o protegida por `std::call_once`.
3. **Manejo de Contextos de Memoria**:
   - Toda estructura de contexto para las llamadas asíncronas (`AuthContext`) debe asignarse en el heap con `new` y liberarse con `delete` **exclusivamente** dentro de los callbacks terminales (cuando se entrega la respuesta al juego o ante un fallo definitivo).
   - Todos los thunks de callback deben utilizar estrictamente la convención de llamada `EOS_CALL` (`__stdcall` en x86, convención estándar Microsoft en x64).

---

## 6. Plan de Modificaciones de Archivos

| Archivo | Acción | Responsabilidad Técnica |
|---|---|---|
| `src/eossdk/api/eos_api_passthrough.h` | **Crear** | Declaraciones de `InitialisePassthrough()`, hooks de `EOS_Connect_Login`, sintetizador de cuentas externas y búsqueda de `EOSSDK_original.dll`. |
| `src/eossdk/api/eos_api_passthrough.cpp` | **Crear** | Implementación del cargador dinámico, máquina de estados `DeviceIdAuth` (Fases 1, 2 y 3) y forwarding dinámico a `g_eosProcs`. |
| `src/eossdk/eos_module.cpp` | **Modificar** | Soporte de `[EOS] Mode = auto \| passthrough \| emulated`. Bifurcación entre `InitialisePassthrough()` e `InitialiseEmulated()`. |
| `build.bat` | **Modificar** | Añadir `src\eossdk\api\eos_api_passthrough.cpp` a la línea de compilación MSVC para `EOSSDK-Win64-Shipping.dll`. |
| `bin/deploy_helper.ps1` | **Modificar** | En modo `valve`: configurar `Mode=passthrough`, respaldar el DLL genuino a `EOSSDK_original.dll`. En modo `goldberg`: configurar `Mode=emulated`. |
| `AutoDeploy.bat` | **Modificar** | Garantizar paso de argumentos correctos para que `deploy_helper.ps1` configure el modo adecuado sin intervención manual. |

---

## 7. Protocolo de Verificación y Criterios de Aceptación

1. **Compilación Limpia**:
   - `build.bat` debe compilar `EOSSDK-Win64-Shipping.dll` y `RedboneEOS.dll` en x64 sin errores ni advertencias de ABI.
2. **Prueba Passthrough en Meccha Chameleon (WAN)**:
   - Al ejecutar con `[EOS] Mode=passthrough` y `[EOS] DeviceIdAuth=true`:
     - El log de ReFix debe reportar `[Passthrough] Genuine EOSSDK loaded successfully`.
     - `EOS_Connect_Login` debe transicionar con éxito mediante `DeviceId`.
     - El navegador de servidores in-game debe poblar los servidores dedicados públicos de Epic.
     - No debe mostrarse la advertencia de desincronización de hora del dispositivo.
3. **Prueba Emulated en Meccha Chameleon / LAN**:
   - Al ejecutar con `[EOS] Mode=emulated` y `[Online] Mode=goldberg`:
     - El log debe reportar el inicio de `ReFixTransport (LAN UDP Multicast 239.255.71.84:47584)`.
     - Las salas locales deben ser descubiertas entre instancias en la misma red local.
4. **Despliegue y Release**:
   - Binarios sincronizados en `D:\EOS_REFIX\ReFix_Release_v2.0-pre\bin\`.
   - Commit con mensaje semántico (`feat(eos): dual-mode architecture...`) y push al repositorio remoto.
