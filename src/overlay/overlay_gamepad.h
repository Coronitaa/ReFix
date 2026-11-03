// Controllers for the overlay: Xbox (and every XInput pad) and PlayStation DualShock 4 / DualSense over
// USB or Bluetooth (read from HID in shared mode, so the game keeps reading them too). Host thread only.
#pragma once
#include <cstdint>

namespace refix::overlay::pad {

enum Button : uint32_t {
    kA = 1u << 0,        // PlayStation: Cross
    kB = 1u << 1,        // Circle
    kX = 1u << 2,        // Square
    kY = 1u << 3,        // Triangle
    kUp = 1u << 4,
    kDown = 1u << 5,
    kLeft = 1u << 6,
    kRight = 1u << 7,
    kLB = 1u << 8,
    kRB = 1u << 9,
    kL3 = 1u << 10,
    kR3 = 1u << 11,
    kStart = 1u << 12,   // Menu / Options
    kBack = 1u << 13,    // View / Share / Create
    kGuide = 1u << 14,   // Xbox / PS button
};

struct State {
    bool connected = false;
    uint32_t buttons = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1, up is positive
    float lt = 0, rt = 0;                   // 0..1
};

// Merged state of every connected controller. Cheap to call every frame: empty XInput slots are probed
// every few seconds, PlayStation pads are looked for on the first call and after DeviceChanged().
State Poll();
void DeviceChanged();   // WM_DEVICECHANGE
// Overlay toggle on a controller: Guide/PS, or View+Menu (Share+Options) together. True once per press.
bool ToggleRequested(const State& s);
// One DualShock 4 / DualSense HID input report (report ID first), as read from the device.
bool ParseSonyReport(bool dualSense, const uint8_t* report, unsigned long length, State& out);

} // namespace refix::overlay::pad
