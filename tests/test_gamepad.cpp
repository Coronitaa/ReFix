// Tests for src/overlay/overlay_gamepad.cpp: PlayStation HID report parsing (DualShock 4 and DualSense,
// USB and Bluetooth), the overlay toggle combination, and a live read of whatever controller is plugged in.
#include "../src/overlay/overlay_gamepad.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace refix::overlay::pad;

extern void ReFixLog(const char* fmt, ...);
void ReFixLog(const char*, ...) {}

namespace {

int g_failed = 0;

void Check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failed;
}

bool Near(float a, float b) { return std::fabs(a - b) < 0.02f; }

// DS4 data block: lx ly rx ry face shoulders system l2 r2
void Ds4Data(unsigned char* d) {
    d[0] = 255;        // lx full right
    d[1] = 0;          // ly full up
    d[2] = 128;
    d[3] = 128;
    d[4] = 0x20 | 0;   // Cross + hat up
    d[5] = 0x01 | 0x20;   // L1 + Options
    d[6] = 0x01;       // PS
    d[7] = 255;        // L2
    d[8] = 0;
}

void TestDs4Usb() {
    std::puts("DualShock 4, USB report 0x01");
    unsigned char r[64] = { 0x01 };
    Ds4Data(r + 1);
    State s;
    Check(ParseSonyReport(false, r, sizeof(r), s), "parsed");
    Check((s.buttons & kA) && (s.buttons & kUp) && (s.buttons & kLB) && (s.buttons & kStart) && (s.buttons & kGuide),
          "Cross, up, L1, Options, PS");
    Check(!(s.buttons & (kB | kX | kY | kDown | kBack)), "nothing else pressed");
    Check(Near(s.lx, 1.0f) && Near(s.ly, 1.0f) && Near(s.rx, 0.0f), "sticks (up is positive)");
    Check(Near(s.lt, 1.0f) && Near(s.rt, 0.0f), "triggers");
}

void TestDs4Bluetooth() {
    std::puts("DualShock 4, Bluetooth report 0x11");
    unsigned char r[78] = { 0x11, 0xC0, 0x00 };
    Ds4Data(r + 3);
    State s;
    Check(ParseSonyReport(false, r, sizeof(r), s) && (s.buttons & kA) && (s.buttons & kGuide), "data at +3");
}

void TestDualSenseUsb() {
    std::puts("DualSense, USB report 0x01 (64 bytes)");
    unsigned char r[64] = { 0x01 };
    unsigned char* d = r + 1;   // lx ly rx ry l2 r2 counter face shoulders system
    d[0] = 0;  d[1] = 128; d[2] = 128; d[3] = 255;
    d[4] = 0;  d[5] = 255; d[6] = 7;
    d[7] = 0x40 | 6;    // Circle + hat left
    d[8] = 0x10 | 0x80; // Create + R3
    d[9] = 0;
    State s;
    Check(ParseSonyReport(true, r, sizeof(r), s), "parsed");
    Check((s.buttons & kB) && (s.buttons & kLeft) && (s.buttons & kBack) && (s.buttons & kR3), "Circle, left, Create, R3");
    Check(!(s.buttons & kGuide), "no PS");
    Check(Near(s.lx, -1.0f) && Near(s.ry, -1.0f) && Near(s.rt, 1.0f), "left stick left, right stick down, R2");
}

void TestDualSenseBluetooth() {
    std::puts("DualSense, Bluetooth report 0x31 and the basic 0x01 report");
    unsigned char r[78] = { 0x31, 0x00 };
    unsigned char* d = r + 2;
    d[0] = 128; d[1] = 128; d[2] = 128; d[3] = 128;
    d[7] = 0x80 | 8;    // Triangle, hat released
    d[9] = 0x01;        // PS
    State s;
    Check(ParseSonyReport(true, r, sizeof(r), s) && (s.buttons & kY) && (s.buttons & kGuide), "0x31: data at +2");
    Check(!(s.buttons & (kUp | kDown | kLeft | kRight)), "hat 8 = released");
    unsigned char basic[10] = { 0x01 };
    Ds4Data(basic + 1);
    State b;
    Check(ParseSonyReport(true, basic, sizeof(basic), b) && (b.buttons & kA), "basic report uses the DS4 layout");
}

void TestRejects() {
    std::puts("Reports that are not input");
    unsigned char feature[64] = { 0x05 };
    State s;
    Check(!ParseSonyReport(false, feature, sizeof(feature), s), "unknown report id");
    unsigned char tiny[4] = { 0x01 };
    Check(!ParseSonyReport(true, tiny, sizeof(tiny), s), "too short");
}

void TestToggle() {
    std::puts("Overlay toggle");
    State none, guide, combo, back;
    guide.buttons = kGuide;
    combo.buttons = kBack | kStart;
    back.buttons = kBack;
    ToggleRequested(none);
    Check(ToggleRequested(guide), "Guide pressed");
    Check(!ToggleRequested(guide), "held Guide does not repeat");
    ToggleRequested(none);
    Check(!ToggleRequested(back), "View alone does nothing");
    Check(ToggleRequested(combo), "View + Menu");
}

} // namespace

int main() {
    TestDs4Usb();
    TestDs4Bluetooth();
    TestDualSenseUsb();
    TestDualSenseBluetooth();
    TestRejects();
    TestToggle();
    const State live = Poll();
    std::printf("Live read: %s (buttons 0x%04X, lx %.2f ly %.2f)\n", live.connected ? "controller connected" : "no controller",
                live.buttons, live.lx, live.ly);
    std::printf(g_failed ? "[FAILED] %d check(s)\n" : "[ALL PASS] gamepad\n", g_failed);
    return g_failed ? 1 : 0;
}
