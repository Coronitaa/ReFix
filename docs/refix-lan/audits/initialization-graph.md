# Initialization Graph Audit

## Puntos de Entrada Identificados en `steam_proxy.cpp`

### 1. `SteamAPI_Init`
- **Tipo:** Export DLL primario.
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca a `g_pfn_Init` (o fallback) -> `ReFixInitializePost()`.
- **Ruteador:** Pasa explícitamente por `NetworkModeManager` (en `Pre`) y decide provider.

### 2. `SteamAPI_InitSafe`
- **Tipo:** Export DLL primario (Steamworks v0.12+).
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_InitSafe` (o fallback) -> `ReFixInitializePost()`.
- **Ruteador:** Pasa explícitamente.

### 3. `SteamAPI_InitFlat`
- **Tipo:** Export DLL primario (C wrapper).
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_InitFlat` (o fallback) -> `ReFixInitializePost()`.
- **Ruteador:** Pasa explícitamente.

### 4. `SteamAPI_InitAnonymousUser`
- **Tipo:** Export DLL primario (Legacy).
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_InitAnon` -> `ReFixInitializePost()`.
- **Ruteador:** Pasa explícitamente.

### 5. `SteamInternal_SteamAPI_Init`
- **Tipo:** Export DLL primario (Steamworks v0.86+).
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_SteamAPIInit_Internal` -> `ReFixInitializePost()`.
- **Ruteador:** Pasa explícitamente.

### 6. `SteamInternal_GameServer_Init`
- **Tipo:** Export DLL GameServer.
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_GSInit`. (No llama a Post porque los hooks de Winsock cliente no se aplican al server).
- **Ruteador:** Pasa explícitamente.

### 7. `SteamGameServer_InitSafe`
- **Tipo:** Export DLL GameServer (Safe).
- **Flujo:** `ReFixInitializePre()` -> `EnsureOriginal()` -> invoca `g_pfn_GSInitSafe`.
- **Ruteador:** Pasa explícitamente.

## Diagrama Demostrativo
```text
(Any Game Initialization Request)
        |
        +-- SteamAPI_Init
        +-- SteamAPI_InitSafe
        +-- SteamInternal_SteamAPI_Init
        +-- SteamGameServer_Init
        |
        V
[ ReFixInitializePre() ]
        |
        V
[ NetworkModeManager::GetMode() ] ---> (Online / Lan / Offline)
        |
        V
[ ProviderFactory::GetSteamProvider() ] ---> (OnlineSteamProvider / LanSteamProvider)
        |
        V
[ EnsureOriginal() ] ---> Carga DLL base de Valve o Goldberg (según modo)
        |
        V
[ ReFixInitializePost() ] ---> Instala hooks de vtable y de Winsock si Mode != Online
```

**Conclusión:** Absolutamente todos los puntos de inicialización posibles expuestos por el DLL proxy transitan por el enrutador de Provider y ejecutan `EnsureOriginal()` a posteriori. No hay "fugas" de inicialización que eviten la aplicación de la política.
