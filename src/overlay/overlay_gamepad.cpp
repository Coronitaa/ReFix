// See overlay_gamepad.h.
//   Xbox / XInput: xinput1_4 (or 1_3 / 9_1_0) loaded on first use; ordinal 100 (XInputGetStateEx, exported
//   by 1_4 and 1_3) also reports the Guide button.
//   PlayStation: DualShock 4 (054C:05C4, 09CC, 0BA0) and DualSense (054C:0CE6, 0DF2) input reports read
//   from their HID device, opened shared and read-only. Report layouts:
//     DS4      0x01 (USB, and Bluetooth before the full mode)  data at +1
//     DS4      0x11 (Bluetooth)                                data at +3
//     DualSense 0x01 USB (64 bytes)                            data at +1, DualSense layout
//     DualSense 0x01 Bluetooth basic (10 bytes)                data at +1, DS4 layout
//     DualSense 0x31 (Bluetooth)                               data at +2, DualSense layout
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_gamepad.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <xinput.h>
#include <algorithm>
#include <memory>
#include <cmath>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::pad {
namespace {

constexpr ULONGLONG kXInputProbeMs = 3000;   // disconnected XInput slots are slow to query
constexpr float kDeadZone = 0.24f;
constexpr int kMaxReadsPerPoll = 32;
constexpr USHORT kSonyVid = 0x054C;
constexpr USHORT kDs4Pids[] = { 0x05C4, 0x09CC, 0x0BA0 };
constexpr USHORT kDualSensePids[] = { 0x0CE6, 0x0DF2 };

// ---- XInput
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetStateFn g_xget = nullptr;
bool g_xLoaded = false;
bool g_xConnected[XUSER_MAX_COUNT] = {};
ULONGLONG g_xNextProbe = 0;
constexpr WORD kXInputGuide = 0x0400;

void LoadXInput() {
    if (g_xLoaded) return;
    g_xLoaded = true;
    for (const wchar_t* dll : { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" }) {
        HMODULE m = LoadLibraryW(dll);
        if (!m) continue;
        g_xget = (XInputGetStateFn)GetProcAddress(m, (LPCSTR)100);   // XInputGetStateEx: has Guide
        if (!g_xget) g_xget = (XInputGetStateFn)GetProcAddress(m, "XInputGetState");
        if (g_xget) return;
    }
}

float Stick(SHORT v) { return v < 0 ? v / 32768.0f : v / 32767.0f; }

State FromXInput(const XINPUT_GAMEPAD& g) {
    State s;
    s.connected = true;
    const WORD b = g.wButtons;
    auto set = [&](WORD mask, Button out) { if (b & mask) s.buttons |= out; };
    set(XINPUT_GAMEPAD_A, kA);
    set(XINPUT_GAMEPAD_B, kB);
    set(XINPUT_GAMEPAD_X, kX);
    set(XINPUT_GAMEPAD_Y, kY);
    set(XINPUT_GAMEPAD_DPAD_UP, kUp);
    set(XINPUT_GAMEPAD_DPAD_DOWN, kDown);
    set(XINPUT_GAMEPAD_DPAD_LEFT, kLeft);
    set(XINPUT_GAMEPAD_DPAD_RIGHT, kRight);
    set(XINPUT_GAMEPAD_LEFT_SHOULDER, kLB);
    set(XINPUT_GAMEPAD_RIGHT_SHOULDER, kRB);
    set(XINPUT_GAMEPAD_LEFT_THUMB, kL3);
    set(XINPUT_GAMEPAD_RIGHT_THUMB, kR3);
    set(XINPUT_GAMEPAD_START, kStart);
    set(XINPUT_GAMEPAD_BACK, kBack);
    set(kXInputGuide, kGuide);
    s.lx = Stick(g.sThumbLX);
    s.ly = Stick(g.sThumbLY);
    s.rx = Stick(g.sThumbRX);
    s.ry = Stick(g.sThumbRY);
    s.lt = g.bLeftTrigger / 255.0f;
    s.rt = g.bRightTrigger / 255.0f;
    return s;
}

// ---- PlayStation over HID
struct HidPad {
    HANDLE h = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED ov{};
    std::vector<uint8_t> buf;
    bool pending = false;
    bool dualSense = false;
    bool dead = false;
    State last;
};

// unique_ptr: a pending read keeps a pointer to the OVERLAPPED, which must never move.
std::vector<std::unique_ptr<HidPad>> g_hid;
bool g_hidScanWanted = true;

float Axis(uint8_t v) { return (v - 128) / (v < 128 ? 128.0f : 127.0f); }

void Hat(uint8_t hat, State& s) {
    static constexpr uint32_t kDirs[8] = { kUp, kUp | kRight, kRight, kDown | kRight, kDown, kDown | kLeft, kLeft, kUp | kLeft };
    if (hat < 8) s.buttons |= kDirs[hat];
}

// Face byte (hat + Square/Cross/Circle/Triangle), shoulder byte, PS byte: the same in both families.
void Buttons(uint8_t face, uint8_t shoulders, uint8_t system, State& s) {
    Hat(face & 0x0F, s);
    if (face & 0x10) s.buttons |= kX;
    if (face & 0x20) s.buttons |= kA;
    if (face & 0x40) s.buttons |= kB;
    if (face & 0x80) s.buttons |= kY;
    if (shoulders & 0x01) s.buttons |= kLB;
    if (shoulders & 0x02) s.buttons |= kRB;
    if (shoulders & 0x10) s.buttons |= kBack;
    if (shoulders & 0x20) s.buttons |= kStart;
    if (shoulders & 0x40) s.buttons |= kL3;
    if (shoulders & 0x80) s.buttons |= kR3;
    if (system & 0x01) s.buttons |= kGuide;
}

void Sticks(const uint8_t* d, State& s) {
    s.lx = Axis(d[0]);
    s.ly = -Axis(d[1]);
    s.rx = Axis(d[2]);
    s.ry = -Axis(d[3]);
}

State Ds4Layout(const uint8_t* d) {   // lx ly rx ry face shoulders system l2 r2
    State s;
    s.connected = true;
    Sticks(d, s);
    Buttons(d[4], d[5], d[6], s);
    s.lt = d[7] / 255.0f;
    s.rt = d[8] / 255.0f;
    return s;
}

State DualSenseLayout(const uint8_t* d) {   // lx ly rx ry l2 r2 counter face shoulders system
    State s;
    s.connected = true;
    Sticks(d, s);
    s.lt = d[4] / 255.0f;
    s.rt = d[5] / 255.0f;
    Buttons(d[7], d[8], d[9], s);
    return s;
}

bool ParseReport(const HidPad& p, const uint8_t* r, DWORD n, State& out) { return ParseSonyReport(p.dualSense, r, n, out); }

} // namespace

bool ParseSonyReport(bool dualSense, const uint8_t* r, unsigned long n, State& out) {
    if (n < 10) return false;
    if (!dualSense) {
        if (r[0] == 0x01) { out = Ds4Layout(r + 1); return true; }
        if (r[0] == 0x11 && n >= 12) { out = Ds4Layout(r + 3); return true; }
        return false;
    }
    if (r[0] == 0x01 && n >= 64) { out = DualSenseLayout(r + 1); return true; }
    if (r[0] == 0x01) { out = Ds4Layout(r + 1); return true; }
    if (r[0] == 0x31 && n >= 12) { out = DualSenseLayout(r + 2); return true; }
    return false;
}

namespace {

void ClosePad(HidPad& p) {
    if (p.h != INVALID_HANDLE_VALUE) {
        CancelIo(p.h);
        CloseHandle(p.h);
    }
    if (p.event) CloseHandle(p.event);
    p.h = INVALID_HANDLE_VALUE;
    p.event = nullptr;
}

bool IsSonyPad(USHORT vid, USHORT pid, bool& dualSense) {
    if (vid != kSonyVid) return false;
    dualSense = std::find(std::begin(kDualSensePids), std::end(kDualSensePids), pid) != std::end(kDualSensePids);
    return dualSense || std::find(std::begin(kDs4Pids), std::end(kDs4Pids), pid) != std::end(kDs4Pids);
}

void ScanHid() {
    for (auto& p : g_hid) ClosePad(*p);
    g_hid.clear();
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO set = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return;
    SP_DEVICE_INTERFACE_DATA itf{ sizeof(itf) };
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &hidGuid, i, &itf); ++i) {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &itf, nullptr, 0, &size, nullptr);
        if (size == 0) continue;
        std::vector<uint8_t> raw(size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(raw.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &itf, detail, size, nullptr, nullptr)) continue;
        // Attributes first with no access rights: never disturbs devices that are not controllers.
        HANDLE probe = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (probe == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr{ sizeof(attr) };
        const bool gotAttr = HidD_GetAttributes(probe, &attr) != FALSE;
        CloseHandle(probe);
        bool dualSense = false;
        if (!gotAttr || !IsSonyPad(attr.VendorID, attr.ProductID, dualSense)) continue;
        auto pad = std::make_unique<HidPad>();
        HidPad& p = *pad;
        p.h = CreateFileW(detail->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED, nullptr);
        if (p.h == INVALID_HANDLE_VALUE) continue;   // held exclusively by another tool
        PHIDP_PREPARSED_DATA pre = nullptr;
        USHORT reportLen = 0;
        if (HidD_GetPreparsedData(p.h, &pre)) {
            HIDP_CAPS caps{};
            if (HidP_GetCaps(pre, &caps) == HIDP_STATUS_SUCCESS) reportLen = caps.InputReportByteLength;
            HidD_FreePreparsedData(pre);
        }
        if (reportLen < 10) { CloseHandle(p.h); continue; }
        HidD_SetNumInputBuffers(p.h, 2);   // only the newest reports matter
        p.buf.resize(reportLen);
        p.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        p.dualSense = dualSense;
        ReFixLog("[Overlay] Controller: %s (%04X:%04X)", dualSense ? "DualSense" : "DualShock 4", attr.VendorID,
                 attr.ProductID);
        g_hid.push_back(std::move(pad));
    }
    SetupDiDestroyDeviceInfoList(set);
}

// Reads every report that is ready and keeps the newest one.
void Drain(HidPad& p) {
    for (int i = 0; i < kMaxReadsPerPoll && !p.dead; ++i) {
        DWORD got = 0;
        if (!p.pending) {
            ResetEvent(p.event);
            p.ov = OVERLAPPED{};
            p.ov.hEvent = p.event;
            if (ReadFile(p.h, p.buf.data(), (DWORD)p.buf.size(), &got, &p.ov)) {
                ParseReport(p, p.buf.data(), got, p.last);
                continue;
            }
            if (GetLastError() != ERROR_IO_PENDING) { p.dead = true; break; }
            p.pending = true;
        }
        if (!GetOverlappedResult(p.h, &p.ov, &got, FALSE)) {
            if (GetLastError() != ERROR_IO_INCOMPLETE) p.dead = true;
            break;
        }
        p.pending = false;
        ParseReport(p, p.buf.data(), got, p.last);
    }
}

void Merge(State& into, const State& s) {
    if (!s.connected) return;
    into.connected = true;
    into.buttons |= s.buttons;
    auto bigger = [](float a, float b) { return std::fabs(b) > std::fabs(a) ? b : a; };
    into.lx = bigger(into.lx, s.lx);
    into.ly = bigger(into.ly, s.ly);
    into.rx = bigger(into.rx, s.rx);
    into.ry = bigger(into.ry, s.ry);
    into.lt = std::max(into.lt, s.lt);
    into.rt = std::max(into.rt, s.rt);
}

float Dead(float v) {
    if (std::fabs(v) < kDeadZone) return 0.0f;
    return (v - std::copysign(kDeadZone, v)) / (1.0f - kDeadZone);
}

} // namespace

void DeviceChanged() {
    g_hidScanWanted = true;
    g_xNextProbe = 0;
}

State Poll() {
    State out;
    LoadXInput();
    if (g_xget) {
        const ULONGLONG now = GetTickCount64();
        const bool probe = now >= g_xNextProbe;
        if (probe) g_xNextProbe = now + kXInputProbeMs;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            if (!g_xConnected[i] && !probe) continue;
            XINPUT_STATE st{};
            g_xConnected[i] = g_xget(i, &st) == ERROR_SUCCESS;
            if (g_xConnected[i]) Merge(out, FromXInput(st.Gamepad));
        }
    }
    if (g_hidScanWanted) {
        g_hidScanWanted = false;
        ScanHid();
    }
    for (auto& p : g_hid) {
        Drain(*p);
        Merge(out, p->last);
    }
    const auto gone = std::remove_if(g_hid.begin(), g_hid.end(), [](std::unique_ptr<HidPad>& p) {
        if (p->dead) ClosePad(*p);
        return p->dead;
    });
    g_hid.erase(gone, g_hid.end());
    out.lx = Dead(out.lx);
    out.ly = Dead(out.ly);
    out.rx = Dead(out.rx);
    out.ry = Dead(out.ry);
    return out;
}

bool ToggleRequested(const State& s) {
    static bool wasDown = false;
    const bool down = (s.buttons & kGuide) || ((s.buttons & kBack) && (s.buttons & kStart));
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

} // namespace refix::overlay::pad
