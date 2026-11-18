#include "mod_panels.h"
#include "../../mods/game_scope.h"
#include "../../mods/mod_host.h"
#include "../../mods/seh_guard.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::mods {

namespace {

constexpr size_t kMaxRequestText = 2048;
constexpr size_t kMaxLogMessage = 512;

bool IsIdChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

bool HeaderOk(uint32_t size, uint32_t minSize, uint32_t version) {
    return size >= minSize && (version >> 16) == REFIX_OVERLAY_ABI_MAJOR;
}

int CRegisterPanel(uint64_t t, const RefixPanelDesc* d) { return Registry::Get().HostRegisterPanel(t, d); }
int CRegisterAction(uint64_t t, const RefixActionDesc* d) { return Registry::Get().HostRegisterAction(t, d); }
int COpen(uint64_t t, const char* id) { return Registry::Get().HostSetOpen(t, id, true); }
int CClose(uint64_t t, const char* id) { return Registry::Get().HostSetOpen(t, id, false); }
int CIsOpen(uint64_t t, const char* id) { return Registry::Get().HostIsOpen(t, id); }
void CLog(uint64_t t, const char* m) { Registry::Get().HostLog(t, m); }

// Tokens only keep mods from acting for each other by mistake; they are not a security boundary in-process.
uint64_t RandomTokenBase() {
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    return ((uint64_t)qpc.QuadPart * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)GetCurrentProcessId() << 32);
}

} // namespace

bool IsValidId(const char* s) {
    if (!s) return false;
    const size_t n = strnlen(s, REFIX_OVERLAY_MAX_ID + 1);
    if (n == 0 || n > REFIX_OVERLAY_MAX_ID) return false;
    return std::all_of(s, s + n, IsIdChar);
}

bool CopyBounded(const char* s, size_t maxBytes, bool required, std::string& out) {
    out.clear();
    if (!s) return !required;
    const size_t n = strnlen(s, maxBytes + 1);
    if (n > maxBytes || (required && n == 0)) return false;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)s[i] < 0x20) return false;  // no control characters in UI strings
    }
    out.assign(s, n);
    return true;
}

int ValidatePanel(const RefixPanelDesc* d, Panel& out) {
    if (!d || !HeaderOk(d->struct_size, sizeof(RefixPanelDesc), d->abi_version)) return REFIX_E_INVALID;
    if (!IsValidId(d->id) || !d->draw) return REFIX_E_INVALID;
    if (d->slot != REFIX_PANEL_SLOT_GRID && d->slot != REFIX_PANEL_SLOT_HUD) return REFIX_E_INVALID;
    const bool needsTitle = d->slot == REFIX_PANEL_SLOT_GRID;
    if (!CopyBounded(d->title, REFIX_OVERLAY_MAX_TITLE, needsTitle, out.title)) return REFIX_E_INVALID;
    if (!CopyBounded(d->subtitle, REFIX_OVERLAY_MAX_TITLE, false, out.subtitle)) return REFIX_E_INVALID;
    out.id = d->id;
    out.slot = d->slot;
    out.draw = d->draw;
    out.preferredHeight = d->preferred_height;
    out.user = d->user;
    return REFIX_OK;
}

int ValidateAction(const RefixActionDesc* d, Action& out) {
    if (!d || !HeaderOk(d->struct_size, sizeof(RefixActionDesc), d->abi_version)) return REFIX_E_INVALID;
    if (!IsValidId(d->id) || !d->on_request) return REFIX_E_INVALID;
    out.id = d->id;
    out.onRequest = d->on_request;
    out.user = d->user;
    return REFIX_OK;
}

std::vector<Cell> LayoutGrid(float areaW, const std::vector<float>& wantedH, float minCellW, float gap) {
    std::vector<Cell> cells;
    if (wantedH.empty() || areaW <= 0.0f || minCellW <= 0.0f) return cells;
    const int fit = (int)((areaW + gap) / (minCellW + gap));
    const int cols = std::clamp(fit, 1, (int)wantedH.size());
    const float w = (areaW - gap * (cols - 1)) / cols;
    float y = 0.0f;
    for (size_t row = 0; row * cols < wantedH.size(); ++row) {
        const size_t first = row * cols, last = (std::min)(first + cols, wantedH.size());
        const float h = *std::max_element(wantedH.begin() + first, wantedH.begin() + last);
        for (size_t i = first; i < last; ++i) cells.push_back({(w + gap) * (float)(i - first), y, w, h});
        y += h + gap;
    }
    return cells;
}

Registry& Registry::Get() {
    static Registry s;
    return s;
}

Registry::Owner* Registry::FindByToken(uint64_t token) {
    for (Owner& o : owners_) if (o.token == token) return &o;
    return nullptr;
}

Registry::Owner* Registry::FindById(const std::string& id) {
    for (Owner& o : owners_) if (o.id == id) return &o;
    return nullptr;
}

void Registry::ActivateBuiltins(uint32_t appId, const std::string& exeName) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (builtinsDone_) return;
        builtinsDone_ = true;
    }
    for (const BuiltinModule& m : BuiltinModules()) {
        if (!refix::mods::GameFeatureRequested(m.feature)) {
            ReFixLog("[OverlayMods] %s: no mod of this game asks for %s (AppID %u, %s), not registered", m.id, m.feature,
                     appId, exeName.c_str());
            continue;
        }
        RegisterModule(m.id, m.registerFn);
    }
}

bool Registry::RegisterModule(const std::string& ownerId, RegisterFn fn) {
    if (!fn || !IsValidId(ownerId.c_str())) return false;
    RefixOverlayHost host{};
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (FindById(ownerId)) return false;
        if (nextToken_ == 0) nextToken_ = RandomTokenBase() | 1;
        Owner o;
        o.id = ownerId;
        o.token = nextToken_++;
        owners_.push_back(o);
        host = {sizeof(RefixOverlayHost), REFIX_OVERLAY_ABI_VERSION, o.token, CRegisterPanel, CRegisterAction,
                COpen, CClose, CIsOpen, CLog};
    }
    int rc = -1;
    const bool ran = Guarded(ownerId, "register", [&] { rc = fn(&host); });
    if (!ran || rc != 0) {
        ReFixLog("[OverlayMods] %s: registration failed (%d), nothing kept", ownerId.c_str(), rc);
        RemoveOwner(ownerId);
        return false;
    }
    ReFixLog("[OverlayMods] %s: registered", ownerId.c_str());
    return true;
}

void Registry::RemoveOwner(const std::string& ownerId) {
    std::unique_lock<std::shared_mutex> gate(gate_);  // waits for the mod's running callbacks
    std::lock_guard<std::mutex> lock(mu_);
    owners_.erase(std::remove_if(owners_.begin(), owners_.end(), [&](const Owner& o) { return o.id == ownerId; }),
                  owners_.end());
    panels_.erase(std::remove_if(panels_.begin(), panels_.end(), [&](const Panel& p) { return p.owner == ownerId; }),
                  panels_.end());
    actions_.erase(std::remove_if(actions_.begin(), actions_.end(), [&](const Action& a) { return a.owner == ownerId; }),
                   actions_.end());
}

std::vector<Panel> Registry::Panels(uint32_t slot) {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Panel> out;
    for (const Panel& p : panels_) {
        if (p.slot != slot) continue;
        Panel copy = p;
        const Owner* o = FindById(p.owner);
        copy.faulted = !o || o->faulted;
        out.push_back(copy);
    }
    return out;
}

std::vector<Panel> Registry::OpenHuds() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Panel> out;
    for (const Panel& p : panels_) {
        const Owner* o = FindById(p.owner);
        if (p.slot != REFIX_PANEL_SLOT_HUD || !o || o->faulted) continue;
        if (std::find(o->openHuds.begin(), o->openHuds.end(), p.id) != o->openHuds.end()) out.push_back(p);
    }
    return out;
}

bool Registry::AnyHudOpen() {
    std::lock_guard<std::mutex> lock(mu_);
    for (const Owner& o : owners_) if (!o.faulted && !o.openHuds.empty()) return true;
    return false;
}

std::vector<std::string> Registry::Owners() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> out;
    for (const Owner& o : owners_) out.push_back(o.id);
    return out;
}

template <class F> bool Registry::Guarded(const std::string& owner, const char* what, F&& f) {
    std::shared_lock<std::shared_mutex> gate(gate_);
    {
        std::lock_guard<std::mutex> lock(mu_);
        const Owner* o = FindById(owner);
        if (!o || o->faulted) return false;  // removed or off: never call into it again
    }
    unsigned long code = 0;
    const ULONGLONG start = GetTickCount64();
    if (!refix::mods::SehGuard(f, &code)) {
        char reason[96];
        snprintf(reason, sizeof(reason), "%s crashed (exception 0x%08lX)", what, code);
        Fault(owner, reason);
        return false;
    }
    if (GetTickCount64() - start > kStallMs) {
        char reason[96];
        snprintf(reason, sizeof(reason), "%s blocked the overlay for over %llu ms", what, (unsigned long long)kStallMs);
        Fault(owner, reason);
        return false;
    }
    return true;
}

void Registry::Fault(const std::string& owner, const char* reason) {
    std::lock_guard<std::mutex> lock(mu_);
    Owner* o = FindById(owner);
    if (!o || o->faulted) return;
    o->faulted = true;
    o->openHuds.clear();
    ReFixLog("[OverlayMods] %s turned off: %s", owner.c_str(), reason ? reason : "fault");
}

bool Registry::Draw(const Panel& p, const RefixPanelDrawArgs& args) {
    return Guarded(p.owner, "draw", [&] { p.draw(p.user, &args); });
}

float Registry::PreferredHeight(const Panel& p, float width) {
    if (!p.preferredHeight) return 0.0f;
    float h = 0.0f;
    if (!Guarded(p.owner, "preferred_height", [&] { h = p.preferredHeight(p.user, width); })) return 0.0f;
    return (h > 0.0f && h < 100000.0f) ? h : 0.0f;  // NaN and absurd sizes fall back to the default
}

bool Registry::Route(uint32_t kind, const char* text) {
    std::string bounded;
    if (!CopyBounded(text, kMaxRequestText, false, bounded)) return false;
    std::vector<Action> actions;
    {
        std::lock_guard<std::mutex> lock(mu_);
        actions = actions_;
    }
    for (const Action& a : actions) {
        int handled = 0;
        if (Guarded(a.owner, "on_request", [&] { handled = a.onRequest(a.user, kind, bounded.c_str()); }) && handled == 1) {
            ReFixLog("[OverlayMods] %s handled request %u '%s'", a.owner.c_str(), kind, bounded.c_str());
            return true;
        }
    }
    return false;
}

int Registry::HostRegisterPanel(uint64_t token, const RefixPanelDesc* d) {
    Panel p;
    const int rc = ValidatePanel(d, p);
    std::lock_guard<std::mutex> lock(mu_);
    Owner* o = FindByToken(token);
    if (!o || o->faulted) return REFIX_E_DENIED;
    if (rc != REFIX_OK) {
        ReFixLog("[OverlayMods] %s: invalid panel rejected", o->id.c_str());
        return rc;
    }
    const auto mine = std::count_if(panels_.begin(), panels_.end(), [&](const Panel& x) { return x.owner == o->id; });
    const bool taken = std::any_of(panels_.begin(), panels_.end(), [&](const Panel& x) { return x.id == p.id; });
    if (mine >= REFIX_OVERLAY_MAX_PANELS || taken) return REFIX_E_LIMIT;
    p.owner = o->id;
    panels_.push_back(p);
    return REFIX_OK;
}

int Registry::HostRegisterAction(uint64_t token, const RefixActionDesc* d) {
    Action a;
    const int rc = ValidateAction(d, a);
    std::lock_guard<std::mutex> lock(mu_);
    Owner* o = FindByToken(token);
    if (!o || o->faulted) return REFIX_E_DENIED;
    if (rc != REFIX_OK) return rc;
    const auto mine = std::count_if(actions_.begin(), actions_.end(), [&](const Action& x) { return x.owner == o->id; });
    const bool taken = std::any_of(actions_.begin(), actions_.end(), [&](const Action& x) { return x.id == a.id; });
    if (mine >= REFIX_OVERLAY_MAX_ACTIONS || taken) return REFIX_E_LIMIT;
    a.owner = o->id;
    actions_.push_back(a);
    return REFIX_OK;
}

int Registry::HostSetOpen(uint64_t token, const char* panelId, bool open) {
    if (!IsValidId(panelId)) return REFIX_E_INVALID;
    std::lock_guard<std::mutex> lock(mu_);
    Owner* o = FindByToken(token);
    if (!o || o->faulted) return REFIX_E_DENIED;
    const bool ownsHud = std::any_of(panels_.begin(), panels_.end(), [&](const Panel& p) {
        return p.owner == o->id && p.id == panelId && p.slot == REFIX_PANEL_SLOT_HUD;
    });
    if (!ownsHud) return REFIX_E_DENIED;  // only its own HUD panels
    auto it = std::find(o->openHuds.begin(), o->openHuds.end(), panelId);
    if (open && it == o->openHuds.end()) o->openHuds.push_back(panelId);
    if (!open && it != o->openHuds.end()) o->openHuds.erase(it);
    return REFIX_OK;
}

int Registry::HostIsOpen(uint64_t token, const char* panelId) {
    if (!IsValidId(panelId)) return 0;
    std::lock_guard<std::mutex> lock(mu_);
    const Owner* o = FindByToken(token);
    if (!o || o->faulted) return 0;
    return std::find(o->openHuds.begin(), o->openHuds.end(), panelId) != o->openHuds.end() ? 1 : 0;
}

void Registry::HostLog(uint64_t token, const char* message) {
    std::string text;
    if (message) text.assign(message, strnlen(message, kMaxLogMessage));
    std::string owner;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const Owner* o = FindByToken(token);
        if (!o) return;
        owner = o->id;
    }
    ReFixLog("[Mod %s] %s", owner.c_str(), text.c_str());
}

void Registry::ResetForTests() {
    std::unique_lock<std::shared_mutex> gate(gate_);
    std::lock_guard<std::mutex> lock(mu_);
    owners_.clear();
    panels_.clear();
    actions_.clear();
    builtinsDone_ = false;
}

} // namespace refix::overlay::mods
