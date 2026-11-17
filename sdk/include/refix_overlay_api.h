/* ReFix overlay mod ABI: plain C, versioned, so a mod built with any compiler can add overlay panels.
 *
 * A native mod exports refix_mod_register_overlay(). ReFix calls it once after refix_mod_init(), only in the
 * games the mod's signed manifest lists (target_games), and removes everything the mod registered before
 * the DLL is unloaded. Every struct starts with struct_size and abi_version; ReFix rejects a struct that is
 * smaller than this header's version or carries another major version.
 *
 * Rules the host enforces (a mod that breaks one is turned off and logged, never the game):
 *   - ids: 1..63 chars of [a-z0-9._-]; titles and subtitles: at most 127 bytes of UTF-8. The host copies them.
 *   - callbacks must not throw or unwind into ReFix; a crash or C++ exception inside one turns the mod off.
 *   - a callback that runs longer than 5 s turns the mod off once it returns.
 *   - pointers ReFix passes in (args, text) are only valid during that call. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REFIX_OVERLAY_ABI_MAJOR 1
#define REFIX_OVERLAY_ABI_MINOR 0
#define REFIX_OVERLAY_ABI_VERSION ((REFIX_OVERLAY_ABI_MAJOR << 16) | REFIX_OVERLAY_ABI_MINOR)

#define REFIX_OVERLAY_MAX_ID 63
#define REFIX_OVERLAY_MAX_TITLE 127
#define REFIX_OVERLAY_MAX_PANELS 8   /* per mod */
#define REFIX_OVERLAY_MAX_ACTIONS 8  /* per mod */

/* Return codes of the host functions. */
#define REFIX_OK 0
#define REFIX_E_INVALID -1   /* bad struct, size, version, id or string */
#define REFIX_E_LIMIT -2     /* too many panels/actions, or the id is already taken */
#define REFIX_E_DENIED -3    /* the mod is turned off, or the id belongs to another mod */

typedef enum RefixPanelSlot {
    REFIX_PANEL_SLOT_GRID = 0,  /* a cell of the overlay home's mod grid, drawn while the overlay is open */
    REFIX_PANEL_SLOT_HUD = 1    /* drawn over the game while opened with open_panel(); draws its own window */
} RefixPanelSlot;

typedef enum RefixRequestKind {
    REFIX_REQUEST_DIALOG = 1,   /* ISteamFriends::ActivateGameOverlay(dialog) */
    REFIX_REQUEST_WEBPAGE = 2   /* ISteamFriends::ActivateGameOverlayToWebPage(url) */
} RefixRequestKind;

/* Passed to draw(); valid only during the call. A DLL with its own ImGui copy calls
 * ImGui::SetCurrentContext(imgui_context) and ImGui::SetAllocatorFunctions(...) before drawing. */
typedef struct RefixPanelDrawArgs {
    uint32_t struct_size;
    uint32_t abi_version;
    float width;   /* content width in pixels (GRID) or the viewport width (HUD) */
    float height;
    void* imgui_context;
    void* (*imgui_alloc)(size_t size, void* user_data);
    void (*imgui_free)(void* ptr, void* user_data);
    void* imgui_alloc_user_data;
} RefixPanelDrawArgs;

typedef struct RefixPanelDesc {
    uint32_t struct_size;   /* sizeof(RefixPanelDesc) */
    uint32_t abi_version;   /* REFIX_OVERLAY_ABI_VERSION */
    const char* id;
    const char* title;      /* GRID: the cell header */
    const char* subtitle;   /* optional */
    uint32_t slot;          /* RefixPanelSlot */
    void (*draw)(void* user, const RefixPanelDrawArgs* args);
    float (*preferred_height)(void* user, float width);  /* optional, GRID: content height wanted */
    void* user;
} RefixPanelDesc;

/* Lets a mod answer the game's overlay requests (e.g. a dialog or URL only its game sends).
 * on_request returns 1 when it handled the request; ReFix then does nothing else with it. */
typedef struct RefixActionDesc {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* id;
    int (*on_request)(void* user, uint32_t kind, const char* text);
    void* user;
} RefixActionDesc;

/* Handed to refix_mod_register_overlay(); valid only during that call, keep the function pointers you need
 * and the token (not the table). token identifies the mod: the host refuses calls with another mod's token. */
typedef struct RefixOverlayHost {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t token;
    int (*register_panel)(uint64_t token, const RefixPanelDesc* desc);
    int (*register_action)(uint64_t token, const RefixActionDesc* desc);
    int (*open_panel)(uint64_t token, const char* panel_id);   /* HUD panels of this mod only */
    int (*close_panel)(uint64_t token, const char* panel_id);
    int (*is_panel_open)(uint64_t token, const char* panel_id);
    void (*log)(uint64_t token, const char* message);           /* message truncated to 512 bytes */
} RefixOverlayHost;

#ifdef _WIN32
#define REFIX_OVERLAY_EXPORT __declspec(dllexport)
#else
#define REFIX_OVERLAY_EXPORT __attribute__((visibility("default")))
#endif

/* Optional export of a native mod. Return 0 on success; anything else drops what it registered. */
REFIX_OVERLAY_EXPORT int refix_mod_register_overlay(const RefixOverlayHost* host);

#ifdef __cplusplus
}
#endif
