// Game-scoped registry of overlay mod panels and request handlers, behind the C ABI in sdk/include/refix_overlay_api.h.
// A module only registers when its targets match the running game, so another game's UI is never reachable.
// No ImGui here: overlay_ui_mods.cpp draws the panels; this file validates, owns and guards the calls.
#pragma once

#include "../../../sdk/include/refix_overlay_api.h"
#include "../../mods/mod_types.h"

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace refix::overlay::mods {

using RegisterFn = int (*)(const RefixOverlayHost* host);

// A module compiled into ReFix itself (e.g. the CS:GO UI). Same ABI as a mod DLL; it registers only when the
// running game's mod asks for it in its signed manifest ("features"), so ReFix holds no list of games.
struct BuiltinModule {
    const char* id;
    const char* feature;
    RegisterFn registerFn;
};
const std::vector<BuiltinModule>& BuiltinModules();  // builtin_modules.cpp

constexpr uint64_t kStallMs = 5000;  // a callback slower than this turns its mod off

struct Panel {
    std::string owner, id, title, subtitle;
    uint32_t slot = REFIX_PANEL_SLOT_GRID;
    void (*draw)(void*, const RefixPanelDrawArgs*) = nullptr;
    float (*preferredHeight)(void*, float) = nullptr;
    void* user = nullptr;
    bool faulted = false;  // its mod was turned off: the grid shows that instead of the panel
};

struct Action {
    std::string owner, id;
    int (*onRequest)(void*, uint32_t, const char*) = nullptr;
    void* user = nullptr;
};

// ABI checks (pure, unit tested).
bool IsValidId(const char* s);
bool CopyBounded(const char* s, size_t maxBytes, bool required, std::string& out);
int ValidatePanel(const RefixPanelDesc* d, Panel& out);
int ValidateAction(const RefixActionDesc* d, Action& out);

// Adaptive grid: as many columns of at least minCellW as fit, rows as tall as their tallest wanted height.
struct Cell { float x, y, w, h; };
std::vector<Cell> LayoutGrid(float areaW, const std::vector<float>& wantedH, float minCellW, float gap);

class Registry {
public:
    static Registry& Get();

    // Registers every builtin module whose targets match the game; once per process.
    void ActivateBuiltins(uint32_t appId, const std::string& exeName);
    // Calls a module's register function with a host table bound to ownerId. Fails closed: on error nothing stays.
    bool RegisterModule(const std::string& ownerId, RegisterFn fn);
    // Drops everything ownerId registered and waits for its running callbacks (call before FreeLibrary).
    void RemoveOwner(const std::string& ownerId);

    std::vector<Panel> Panels(uint32_t slot);
    std::vector<Panel> OpenHuds();
    bool AnyHudOpen();
    std::vector<std::string> Owners();

    // Guarded calls into a mod. false when the call crashed, threw or stalled: the mod is then off.
    bool Draw(const Panel& p, const RefixPanelDrawArgs& args);
    float PreferredHeight(const Panel& p, float width);
    bool Route(uint32_t kind, const char* text);  // true when a mod handled the game's request
    void Fault(const std::string& owner, const char* reason);

    void ResetForTests();

    // Host table entry points (C ABI side).
    int HostRegisterPanel(uint64_t token, const RefixPanelDesc* d);
    int HostRegisterAction(uint64_t token, const RefixActionDesc* d);
    int HostSetOpen(uint64_t token, const char* panelId, bool open);
    int HostIsOpen(uint64_t token, const char* panelId);
    void HostLog(uint64_t token, const char* message);

private:
    struct Owner {
        std::string id;
        uint64_t token = 0;
        bool faulted = false;
        std::vector<std::string> openHuds;
    };
    Owner* FindByToken(uint64_t token);
    Owner* FindById(const std::string& id);
    template <class F> bool Guarded(const std::string& owner, const char* what, F&& f);

    std::mutex mu_;           // data below
    std::shared_mutex gate_;  // held shared while a mod callback runs, exclusive while a mod is removed
    std::vector<Owner> owners_;
    std::vector<Panel> panels_;
    std::vector<Action> actions_;
    uint64_t nextToken_ = 0;
    bool builtinsDone_ = false;
};

} // namespace refix::overlay::mods
