// Overlay mod registry: game scoping (manifest matching), the C ABI checks, fault isolation and the grid.
// Headless: the CS:GO module is the real one, its ImGui drawing is stubbed.
#include "../src/overlay/game_actions/mod_panels.h"
#include "../src/overlay/csgo/csgo_overlay.h"
#include "../src/mods/game_scope.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace refix::overlay::mods;
using refix::mods::MatchesGame;
using refix::mods::TargetGame;

void ReFixLog(const char* fmt, ...) { (void)fmt; }

// Stubs for the CS:GO UI files (they need the live overlay).
namespace refix::overlay::csgo_ui {
int g_serverOpened = 0;
float LanCardHeight(float) { return 200.0f; }
void DrawLanCard(float, float) {}
void DrawMarketHud(float, float) {}
void DrawServerHud(float, float) { ++g_serverOpened; }
void SetServerMode(const char*) {}
void RunOnOverlayThread(std::function<void()> fn) { fn(); }
void CloseOverlayNow() {}
} // namespace refix::overlay::csgo_ui

namespace {

bool HasPanel(const char* id) {
    for (uint32_t slot : {(uint32_t)REFIX_PANEL_SLOT_GRID, (uint32_t)REFIX_PANEL_SLOT_HUD})
        for (const Panel& p : Registry::Get().Panels(slot)) if (p.id == id) return true;
    return false;
}

void TestManifestMatching() {
    const std::vector<TargetGame> csgo = {{730, "csgo.exe", "Source"}, {4465480, "csgo.exe", "Source"}};
    assert(MatchesGame(csgo, 730, "csgo.exe"));
    assert(MatchesGame(csgo, 4465480, "CSGO.EXE"));
    assert(!MatchesGame(csgo, 602960, "Barotrauma.exe"));   // the reported bug's game
    assert(!MatchesGame(csgo, 730, "Barotrauma.exe"));      // AppID alone is not enough when the exe is declared
    assert(!MatchesGame(csgo, 602960, "csgo.exe"));         // nor the exe when the AppID is known and different
    assert(MatchesGame(csgo, 480, "csgo.exe"));             // unknown AppID: the exe decides
    assert(!MatchesGame(csgo, 0, "game.exe"));
    assert(!MatchesGame({}, 730, "csgo.exe"));              // no targets: matches nothing
    assert(!MatchesGame({{0, "", ""}}, 730, "csgo.exe"));   // wildcard target: rejected
    assert(MatchesGame({{0, "REPO.exe", ""}}, 3241660, "repo.exe"));
    const std::vector<TargetGame> byApp = {{602960, "", ""}};
    assert(MatchesGame(byApp, 602960, "anything.exe"));
    assert(!MatchesGame(byApp, 480, "Barotrauma.exe"));     // AppID-only target, unknown AppID: no
}

void Draw(void*, const RefixPanelDrawArgs*) {}

void TestAbiValidation() {
    assert(IsValidId("csgo.lan") && IsValidId("a-b_c.9"));
    assert(!IsValidId("") && !IsValidId(nullptr) && !IsValidId("Upper") && !IsValidId("sp ace") && !IsValidId("a/b"));
    assert(IsValidId(std::string(63, 'a').c_str()) && !IsValidId(std::string(64, 'a').c_str()));

    std::string out;
    assert(CopyBounded("hi", 4, true, out) && out == "hi");
    assert(!CopyBounded("hello", 4, true, out));
    assert(!CopyBounded("", 4, true, out) && CopyBounded(nullptr, 4, false, out) && out.empty());
    assert(!CopyBounded("a\nb", 8, true, out));

    RefixPanelDesc d{sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, "my.panel", "Title", nullptr,
                     REFIX_PANEL_SLOT_GRID, Draw, nullptr, nullptr};
    Panel p;
    assert(ValidatePanel(&d, p) == REFIX_OK && p.title == "Title");
    RefixPanelDesc bad = d;
    bad.struct_size = 8;
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);                       // truncated struct
    bad = d;
    bad.abi_version = (2 << 16);
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);                       // other major version
    bad = d;
    bad.abi_version = REFIX_OVERLAY_ABI_VERSION + 1;
    assert(ValidatePanel(&bad, p) == REFIX_OK);                              // newer minor: fine
    bad = d;
    bad.draw = nullptr;
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);
    bad = d;
    bad.title = nullptr;
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);                       // grid cells need a title
    bad.slot = REFIX_PANEL_SLOT_HUD;
    assert(ValidatePanel(&bad, p) == REFIX_OK);
    bad.slot = 7;
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);
    const std::string longTitle(200, 'x');
    bad = d;
    bad.title = longTitle.c_str();
    assert(ValidatePanel(&bad, p) == REFIX_E_INVALID);
    assert(ValidatePanel(nullptr, p) == REFIX_E_INVALID);

    Action a;
    RefixActionDesc ad{sizeof(RefixActionDesc), REFIX_OVERLAY_ABI_VERSION, "my.act", nullptr, nullptr};
    assert(ValidateAction(&ad, a) == REFIX_E_INVALID);
}

void TestCsgoScoping() {
    Registry& r = Registry::Get();
    r.ResetForTests();
    r.ActivateBuiltins(602960, "Barotrauma.exe");
    assert(r.Owners().empty());
    assert(!HasPanel(refix::overlay::csgo_ui::kLanCard) && !HasPanel(refix::overlay::csgo_ui::kMarketHud));
    // The bug: Barotrauma's "Discover mods" opens the Steam workshop; nothing may claim it.
    assert(!r.Route(REFIX_REQUEST_WEBPAGE, "https://steamcommunity.com/app/602960/workshop/"));
    assert(!r.Route(REFIX_REQUEST_DIALOG, "Community"));
    assert(!r.AnyHudOpen());

    r.ResetForTests();
    r.ActivateBuiltins(730, "csgo.exe");
    assert(r.Owners().size() == 1 && r.Owners()[0] == "refix.csgo.overlay");
    assert(HasPanel(refix::overlay::csgo_ui::kLanCard) && HasPanel(refix::overlay::csgo_ui::kServerHud));
    assert(r.Panels(REFIX_PANEL_SLOT_GRID).size() == 1);
    assert(r.Route(REFIX_REQUEST_DIALOG, "Market"));
    assert(r.OpenHuds().size() == 1 && r.OpenHuds()[0].id == refix::overlay::csgo_ui::kMarketHud);
    assert(r.Route(REFIX_REQUEST_WEBPAGE, "refix://server/competitive"));
    assert(r.OpenHuds().size() == 2);
    assert(!r.Route(REFIX_REQUEST_WEBPAGE, "https://example.org/news"));
    r.ActivateBuiltins(602960, "Barotrauma.exe");   // once per process: no second activation
    assert(r.Owners().size() == 1);
}

uint64_t g_token = 0;
int g_tokenOk = 0;
const RefixOverlayHost* g_hostTable = nullptr;
void Crash(void*, const RefixPanelDrawArgs*) { *(volatile int*)nullptr = 1; }
void Throws(void*, const RefixPanelDrawArgs*) { throw 42; }
int RegisterCrashy(const RefixOverlayHost* h) {
    g_token = h->token;
    g_hostTable = h;
    RefixPanelDesc d{sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, "crashy.cell", "Crashy", nullptr,
                     REFIX_PANEL_SLOT_GRID, Crash, nullptr, nullptr};
    g_tokenOk = h->register_panel(h->token, &d);
    // Another mod's id, a HUD it does not own, and a duplicate are refused.
    assert(h->open_panel(h->token, "csgo.market") == REFIX_E_DENIED);
    assert(h->register_panel(h->token, &d) == REFIX_E_LIMIT);
    assert(h->register_panel(h->token + 1, &d) == REFIX_E_DENIED);
    return 0;
}
int RegisterThrower(const RefixOverlayHost* h) {
    RefixPanelDesc d{sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, "thrower.cell", "Thrower", nullptr,
                     REFIX_PANEL_SLOT_GRID, Throws, nullptr, nullptr};
    return h->register_panel(h->token, &d);
}
int RegisterFails(const RefixOverlayHost* h) {
    RefixPanelDesc d{sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, "fails.cell", "Fails", nullptr,
                     REFIX_PANEL_SLOT_GRID, Draw, nullptr, nullptr};
    h->register_panel(h->token, &d);
    return 5;
}
int RegisterCrashesItself(const RefixOverlayHost*) { *(volatile int*)nullptr = 1; return 0; }

void TestFaultIsolation() {
    Registry& r = Registry::Get();
    r.ResetForTests();
    assert(r.RegisterModule("crashy", RegisterCrashy) && g_tokenOk == REFIX_OK);
    assert(r.RegisterModule("thrower", RegisterThrower));
    assert(!r.RegisterModule("crashy", RegisterCrashy));              // one registration per mod id
    assert(!r.RegisterModule("fails", RegisterFails) && !HasPanel("fails.cell"));  // failed: nothing kept
    assert(!r.RegisterModule("selfcrash", RegisterCrashesItself));
    assert(!r.RegisterModule("Bad Id", RegisterFails));

    for (const Panel& p : r.Panels(REFIX_PANEL_SLOT_GRID)) {
        RefixPanelDrawArgs args{sizeof(args), REFIX_OVERLAY_ABI_VERSION, 100, 100};
        assert(!r.Draw(p, args));   // crash and C++ exception both stay inside the guard
    }
    for (const Panel& p : r.Panels(REFIX_PANEL_SLOT_GRID)) assert(p.faulted);
    // A turned-off mod cannot register more or act any longer.
    RefixPanelDesc d{sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, "crashy.more", "More", nullptr,
                     REFIX_PANEL_SLOT_GRID, Draw, nullptr, nullptr};
    assert(Registry::Get().HostRegisterPanel(g_token, &d) == REFIX_E_DENIED);

    r.RemoveOwner("crashy");
    assert(!HasPanel("crashy.cell"));
    assert(Registry::Get().HostRegisterPanel(g_token, &d) == REFIX_E_DENIED);  // stale token after unload
    r.ResetForTests();
}

void TestGrid() {
    const float gap = 10.0f;
    assert(LayoutGrid(800, {}, 300, gap).empty());
    auto one = LayoutGrid(400, {120}, 300, gap);
    assert(one.size() == 1 && one[0].w == 400 && one[0].h == 120);
    auto wide = LayoutGrid(1000, {100, 200, 150, 80}, 300, gap);   // 3 columns fit
    assert(wide.size() == 4);
    assert(wide[0].y == 0 && wide[1].y == 0 && wide[2].y == 0 && wide[3].y == 210);  // row height = tallest
    assert(wide[1].x > wide[0].x && wide[3].x == 0);
    auto narrow = LayoutGrid(500, {100, 200}, 300, gap);           // reflows to one column
    assert(narrow[0].x == 0 && narrow[1].x == 0 && narrow[1].y == 110);
    auto twoOfThree = LayoutGrid(700, {50, 60}, 300, gap);         // never more columns than cells
    assert(twoOfThree[0].w == twoOfThree[1].w && twoOfThree[0].w == 345);
}

} // namespace

int main() {
    TestManifestMatching();
    TestAbiValidation();
    TestCsgoScoping();
    TestFaultIsolation();
    TestGrid();
    std::puts("test_overlay_mods: OK");
    return 0;
}
