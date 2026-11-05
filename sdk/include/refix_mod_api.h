#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REFIX_MOD_API_VERSION_MAJOR 1
#define REFIX_MOD_API_VERSION_MINOR 0
#define REFIX_MOD_API_VERSION ((REFIX_MOD_API_VERSION_MAJOR << 16) | REFIX_MOD_API_VERSION_MINOR)

// Opaque ephemeral peer handle. Double-salted HMAC-SHA256. Never leaks physical IP/port/SteamID.
typedef uint64_t RefixPeerHandle;

typedef enum RefixLogLevel {
    REFIX_LOG_DEBUG = 0,
    REFIX_LOG_INFO  = 1,
    REFIX_LOG_WARN  = 2,
    REFIX_LOG_ERROR = 3
} RefixLogLevel;

typedef enum RefixLifecyclePhase {
    REFIX_PHASE_BOOT = 0,
    REFIX_PHASE_PRE_REFIX,
    REFIX_PHASE_POST_REFIX,
    REFIX_PHASE_GAME_INIT,
    REFIX_PHASE_ENGINE_READY,
    REFIX_PHASE_NETWORK_READY,
    REFIX_PHASE_PRE_FRAME,
    REFIX_PHASE_POST_FRAME,
    REFIX_PHASE_LOBBY_JOINED,
    REFIX_PHASE_MATCH_START,
    REFIX_PHASE_SHUTDOWN
} RefixLifecyclePhase;

// Versioned Declarative Patch Descriptor
typedef struct RefixDeclarativePatchDesc {
    uint32_t struct_size;          // sizeof(RefixDeclarativePatchDesc)
    uint32_t api_version;          // REFIX_MOD_API_VERSION
    const char* target_module;     // e.g. "GameAssembly.dll" or "" for game exe
    uint32_t rva;                  // Relative Virtual Address from module base
    const uint8_t* expected_bytes; // Expected original bytes (safety check)
    size_t expected_bytes_len;
    const uint8_t* patch_bytes;    // Replacement bytes
    size_t patch_bytes_len;
} RefixDeclarativePatchDesc;

// Versioned Hook Descriptor
typedef struct RefixHookDesc {
    uint32_t struct_size;          // sizeof(RefixHookDesc)
    uint32_t api_version;          // REFIX_MOD_API_VERSION
    const char* target_module;     // Target DLL name or ""
    uint32_t rva;                  // RVA of target function
    const char* wasm_callback;     // Name of exported WASM callback function
} RefixHookDesc;

// Versioned Host API Table passed to native mods and wrapped for WASM imports
typedef struct RefixHostApi {
    uint32_t struct_size;          // sizeof(RefixHostApi)
    uint32_t api_version;          // REFIX_MOD_API_VERSION

    // Logging
    void (*log)(RefixLogLevel level, const char* message);

    // Timing
    uint64_t (*get_time_ms)();

    // Isolated Persistent Storage
    int32_t (*storage_read)(const char* key, void* out_buffer, size_t max_len);
    int32_t (*storage_write)(const char* key, const void* data, size_t len);

    // Declarative Patches
    int32_t (*apply_patch)(const RefixDeclarativePatchDesc* patch_desc);

    // Mediated MinHook Hooks
    int32_t (*register_hook)(const RefixHookDesc* hook_desc);

    // Network Transport (PeerHandle + Channel, zero sockaddr leaks)
    int32_t (*net_send)(RefixPeerHandle peer, uint32_t channel, const void* data, size_t len);
    int32_t (*net_get_peer_display_name)(RefixPeerHandle peer, char* out_name, size_t max_len);

    // Overlay Integration
    int32_t (*overlay_add_tab)(const char* title, void (*render_callback)(void* user_data), void* user_data);

} RefixHostApi;

// Entrypoints exported by Native ReFix Mods
#ifdef _WIN32
  #define REFIX_MOD_EXPORT __declspec(dllexport)
#else
  #define REFIX_MOD_EXPORT __attribute__((visibility("default")))
#endif

REFIX_MOD_EXPORT int refix_mod_init(const RefixHostApi* host_api);
REFIX_MOD_EXPORT void refix_mod_shutdown(void);
REFIX_MOD_EXPORT void refix_mod_on_event(int event_id, const void* event_data);

#ifdef __cplusplus
}
#endif
