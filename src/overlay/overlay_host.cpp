#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_host.h"
#include "overlay_gamepad.h"
#include "overlay_theme.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <vector>

#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

extern void ReFixLog(const char* fmt, ...);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace refix::overlay::host {
namespace {

constexpr wchar_t kClassName[] = L"ReFixLanOverlay";
constexpr UINT kWakeMsg = WM_APP + 1;
constexpr DWORD kIdleWaitMs = 15;
constexpr ULONGLONG kGameScanMs = 400;
constexpr float kToastMargin = 24.0f;

struct TextureEntry {
    ID3D11ShaderResourceView* srv = nullptr;
    bool failed = false;
};

Client* g_client = nullptr;
std::atomic<bool> g_running{false};
std::atomic<bool> g_hotkeyPending{false};
std::atomic<bool> g_wantHotkeyHook{false};
DWORD g_threadId = 0;
HANDLE g_ready = nullptr;
bool g_initOk = false;

HWND g_hwnd = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain1* g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
IDCompositionDevice* g_dcomp = nullptr;
IDCompositionTarget* g_target = nullptr;
IDCompositionVisual* g_visual = nullptr;
UINT g_width = 0, g_height = 0;
// Wine/Proton has no DirectComposition: there the frame is drawn off screen and shown through a layered
// window (UpdateLayeredWindow). [Overlay] Composition = false forces this path on Windows too.
bool g_layered = false;
ID3D11Texture2D* g_offscreen = nullptr;
ID3D11Texture2D* g_staging = nullptr;
HDC g_memDC = nullptr;
HBITMAP g_dib = nullptr;
void* g_dibBits = nullptr;
Mode g_mode = Mode::Hidden;
bool g_deviceLost = false;
std::map<std::string, TextureEntry> g_textures;

SRWLOCK g_gameLock = SRWLOCK_INIT;
GameWindow g_game;

template <typename T> void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

bool IsOurProcessWindow(HWND w) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

// Shift+Tab is read by polling while a window of the game process has focus: no system-wide hook.
// Bit 0 ("pressed since the last call") also catches taps shorter than the polling interval.
bool PollHotkey() {
    static bool wasDown = false;
    const SHORT tab = GetAsyncKeyState(VK_TAB);
    const SHORT shift = GetAsyncKeyState(VK_SHIFT);
    const bool down = (tab & 0x8001) && (shift & 0x8001) && !(GetAsyncKeyState(VK_MENU) & 0x8000) &&
                      !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed && IsOurProcessWindow(GetForegroundWindow());
}

// Shift+Tab is the overlay's own hotkey: ImGui would take its Tab as "focus the previous field" and start
// editing the search box as soon as the overlay opens.
bool IsHotkeyTab(UINT msg, WPARAM wp) {
    return (msg == WM_KEYDOWN || msg == WM_KEYUP) && wp == VK_TAB && (GetKeyState(VK_SHIFT) & 0x8000);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (IsHotkeyTab(msg, wp)) return 0;
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_MOUSEACTIVATE:
        if (g_mode == Mode::Toast) return MA_NOACTIVATE;
        return MA_ACTIVATE;
    case WM_CLOSE:
        if (g_client) g_client->OnCloseRequested();
        return 0;
    case WM_DEVICECHANGE:
        pad::DeviceChanged();
        break;
    case WM_SETCURSOR:
        if (g_mode == Mode::Overlay || g_mode == Mode::Dialog) {
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ReleaseLayeredSurface() {
    SafeRelease(g_offscreen);
    SafeRelease(g_staging);
    if (g_dib) { DeleteObject(g_dib); g_dib = nullptr; g_dibBits = nullptr; }
    if (g_memDC) { DeleteDC(g_memDC); g_memDC = nullptr; }
}

bool CreateLayeredSurface(UINT w, UINT h) {
    ReleaseLayeredSurface();
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_device->CreateTexture2D(&td, nullptr, &g_offscreen))) return false;
    td.BindFlags = 0;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g_device->CreateTexture2D(&td, nullptr, &g_staging))) return false;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = (LONG)w;
    bi.bmiHeader.biHeight = -(LONG)h;  // top-down, like the texture
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    g_memDC = CreateCompatibleDC(nullptr);
    g_dib = CreateDIBSection(g_memDC, &bi, DIB_RGB_COLORS, &g_dibBits, nullptr, 0);
    if (!g_dib) return false;
    SelectObject(g_memDC, g_dib);
    return true;
}

// The frame is premultiplied BGRA already (drawn over a transparent clear), which is what ULW_ALPHA wants.
HRESULT PresentLayered() {
    g_ctx->CopyResource(g_staging, g_offscreen);
    D3D11_MAPPED_SUBRESOURCE m = {};
    const HRESULT hr = g_ctx->Map(g_staging, 0, D3D11_MAP_READ, 0, &m);
    if (FAILED(hr)) return hr;
    for (UINT y = 0; y < g_height; ++y)
        memcpy((uint8_t*)g_dibBits + (size_t)y * g_width * 4, (const uint8_t*)m.pData + (size_t)y * m.RowPitch, (size_t)g_width * 4);
    g_ctx->Unmap(g_staging, 0);
    SIZE size{ (LONG)g_width, (LONG)g_height };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwnd, nullptr, nullptr, &size, g_memDC, &src, 0, &blend, ULW_ALPHA);
    return S_OK;
}

HRESULT PresentFrame(UINT syncInterval) {
    return g_layered ? PresentLayered() : g_swap->Present(syncInterval, 0);
}

LONG_PTR BaseExStyle() { return g_layered ? WS_EX_LAYERED : WS_EX_NOREDIRECTIONBITMAP; }

void CreateRenderTarget() {
    if (g_layered) {
        g_device->CreateRenderTargetView(g_offscreen, nullptr, &g_rtv);
        return;
    }
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

// Another module patched DXGI's factory vtable (Brawlhalla's FrameFixDLL does): composition swap chains
// then run through its hook and crash inside dxgi, so the overlay draws through a layered window instead.
bool FactoryPatchedByOtherModule(IDXGIFactory2* factory) {
    const HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    void** vtable = *reinterpret_cast<void***>(factory);
    for (const int slot : { 10, 15, 24 }) {   // CreateSwapChain, CreateSwapChainForHwnd, CreateSwapChainForComposition
        HMODULE owner = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCWSTR>(vtable[slot]), &owner);
        if (owner == dxgi) continue;
        wchar_t name[MAX_PATH] = L"?";
        if (owner) GetModuleFileNameW(owner, name, MAX_PATH);
        ReFixLog("[Overlay] DXGI factory slot %d is patched by %ls", slot, name);
        return true;
    }
    return false;
}

bool CreateDeviceAndSurface(UINT w, UINT h) {
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                                   3, D3D11_SDK_VERSION, &g_device, nullptr, &g_ctx);
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                               D3D11_SDK_VERSION, &g_device, nullptr, &g_ctx);
    if (FAILED(hr)) { ReFixLog("[Overlay] D3D11CreateDevice failed 0x%08X", (unsigned)hr); return false; }

    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;
    g_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (dxgiDevice) dxgiDevice->GetAdapter(&adapter);
    if (adapter) adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = w;
    sd.Height = h;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    const bool composition = GetPrivateProfileIntA("Overlay", "Composition", 1, ".\\ReFix.ini") != 0 &&
                             !(factory && FactoryPatchedByOtherModule(factory));
    hr = !composition ? E_NOTIMPL : factory ? factory->CreateSwapChainForComposition(g_device, &sd, nullptr, &g_swap) : E_FAIL;
    if (SUCCEEDED(hr)) hr = DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_dcomp));
    // Not topmost: child windows (the overlay browser) are drawn above the ImGui visual.
    if (SUCCEEDED(hr)) hr = g_dcomp->CreateTargetForHwnd(g_hwnd, FALSE, &g_target);
    if (SUCCEEDED(hr)) hr = g_dcomp->CreateVisual(&g_visual);
    if (SUCCEEDED(hr)) hr = g_visual->SetContent(g_swap);
    if (SUCCEEDED(hr)) hr = g_target->SetRoot(g_visual);
    if (SUCCEEDED(hr)) hr = g_dcomp->Commit();
    SafeRelease(factory);
    SafeRelease(adapter);
    SafeRelease(dxgiDevice);
    if (FAILED(hr)) {
        ReFixLog("[Overlay] DirectComposition unavailable (0x%08X); drawing through a layered window", (unsigned)hr);
        SafeRelease(g_visual);
        SafeRelease(g_target);
        SafeRelease(g_dcomp);
        SafeRelease(g_swap);
        g_layered = true;
        if (!CreateLayeredSurface(w, h)) { ReFixLog("[Overlay] Layered window surface failed"); return false; }
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, (GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE) & ~WS_EX_NOREDIRECTIONBITMAP) | WS_EX_LAYERED);
    }
    g_width = w;
    g_height = h;
    CreateRenderTarget();
    if (g_rtv && g_ctx) {
        const float clear[4] = { 0, 0, 0, 0 };
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        PresentFrame(0);
    }
    return true;
}

void ResizeSurface(UINT w, UINT h) {
    w = std::max<UINT>(w, 8);
    h = std::max<UINT>(h, 8);
    if (w == g_width && h == g_height) return;
    SafeRelease(g_rtv);
    const HRESULT hr = g_layered ? (CreateLayeredSurface(w, h) ? S_OK : E_FAIL)
                                 : g_swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    g_width = w;
    g_height = h;
    if (FAILED(hr)) g_deviceLost = true;   // recreated before the next frame
    else {
        CreateRenderTarget();
        if (g_rtv && g_ctx) {
            const float clear[4] = { 0, 0, 0, 0 };
            g_ctx->ClearRenderTargetView(g_rtv, clear);
            PresentFrame(0);
        }
    }
}

BOOL CALLBACK FindGameWindow(HWND w, LPARAM lp) {
    auto* best = reinterpret_cast<std::pair<HWND, LONG>*>(lp);
    if (w == g_hwnd || !IsWindowVisible(w) || !IsOurProcessWindow(w)) return TRUE;
    if (GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    if (GetWindow(w, GW_OWNER) != nullptr) return TRUE;
    RECT rc{};
    GetClientRect(w, &rc);
    const LONG area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > best->second) *best = { w, area };
    return TRUE;
}

// Last game window seen restored: fullscreen games minimise when the overlay takes the focus, and the
// overlay then stays on the area the game had.
HWND g_lastGameHwnd = nullptr;
RECT g_lastGameClient{};

void ScanGameWindow() {
    std::pair<HWND, LONG> best{ nullptr, 160 * 120 };
    EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&best));
    GameWindow gw;
    gw.hwnd = best.first;
    if (!gw.hwnd && g_lastGameHwnd && IsWindow(g_lastGameHwnd) && IsIconic(g_lastGameHwnd)) {
        gw.hwnd = g_lastGameHwnd;
        gw.minimized = true;
        gw.client = g_lastGameClient;
        AcquireSRWLockShared(&g_gameLock);
        gw.title = g_game.title;
        ReleaseSRWLockShared(&g_gameLock);
    } else if (gw.hwnd && !IsIconic(gw.hwnd)) {
        RECT rc{};
        GetClientRect(gw.hwnd, &rc);
        POINT tl{ rc.left, rc.top }, br{ rc.right, rc.bottom };
        ClientToScreen(gw.hwnd, &tl);
        ClientToScreen(gw.hwnd, &br);
        gw.client = { tl.x, tl.y, br.x, br.y };
        wchar_t title[256] = {};
        // Never GetWindowText: it sends WM_GETTEXT to the game thread, which may be blocked on our dialog.
        InternalGetWindowText(gw.hwnd, title, 256);
        gw.title = title;
        g_lastGameHwnd = gw.hwnd;
        g_lastGameClient = gw.client;
    } else {
        gw.hwnd = nullptr;
    }
    HWND fg = GetForegroundWindow();
    gw.foreground = fg && IsOurProcessWindow(fg);
    AcquireSRWLockExclusive(&g_gameLock);
    g_game = gw;
    ReleaseSRWLockExclusive(&g_gameLock);
}

// UI scale from the height the overlay is drawn on: the game's picture when there is one (a 1280x720
// window on a 4K monitor gets small text, a 4K game big text), the monitor otherwise. 1080 px -> 1.2,
// so text stays readable over a game; steps of 0.05 keep font atlas rebuilds rare.
constexpr float kScaleAt1080 = 1.2f;
constexpr float kMinScale = 0.9f;   // below this captions drop under ~13 px
constexpr float kMaxScale = 3.0f;

float ScaleFor(const GameWindow* gw) {
    char scaleBuf[32] = {};
    if (GetPrivateProfileStringA("Overlay", "Scale", "", scaleBuf, sizeof(scaleBuf), ".\\ReFix.ini") > 0) {
        float customScale = (float)atof(scaleBuf);
        if (customScale >= 0.5f && customScale <= 4.0f) {
            return customScale;
        }
    }
    float h = 0;
    if (gw && gw->hwnd && !gw->minimized) h = (float)(gw->client.bottom - gw->client.top);
    if (h < 240) {
        HMONITOR mon = MonitorFromWindow(gw && gw->hwnd ? gw->hwnd : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(mon, &mi);
        h = (float)(mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    const float scale = h / 1080.0f * kScaleAt1080;
    return std::clamp(std::round(scale * 20.0f) / 20.0f, kMinScale, kMaxScale);
}

// While the Shift+Tab overlay is open the overlay window is a child of the game window: taking the
// keyboard into a child does not deactivate the game's top-level window. Unity (and others) minimise a
// fullscreen window on WM_ACTIVATE(WA_INACTIVE), which is what activating our own popup sent.
// Games in real exclusive Direct3D fullscreen would hide a child behind their exclusive image (the shell
// reports that state): those keep the popup, and minimise as before. Borderless/"fullscreen window" games,
// topmost or not, take the child.
HWND g_childOf = nullptr;

bool CanHostAsChild(const GameWindow& gw) {
    // Top-level topmost popup avoids DirectComposition visual tree invalidation,
    // coordinate mismatch with window borders, and cross-thread mouse input latency.
    return false;
}

// The game keeps the activation while the overlay is its child, so its raw input (camera, movement,
// controllers read as HID) would keep running under the overlay. Raw input registrations belong to the
// process: take the game's away while the overlay is open and give back exactly what it had.
std::vector<RAWINPUTDEVICE> g_suspendedRaw;

// Generic Desktop usages a game reads its player input from: mouse, joystick, gamepad, keyboard, multi-axis
// controller (PlayStation pads and DirectInput devices arrive as joystick/gamepad raw input).
bool IsPlayerInput(const RAWINPUTDEVICE& d) {
    if (d.usUsagePage != 0x01) return false;
    return d.usUsage == 0x02 || d.usUsage == 0x04 || d.usUsage == 0x05 || d.usUsage == 0x06 || d.usUsage == 0x08;
}

void SuspendGameRawInput() {
    if (!g_suspendedRaw.empty()) return;
    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    if (count == 0) return;
    std::vector<RAWINPUTDEVICE> all(count);
    if (GetRegisteredRawInputDevices(all.data(), &count, sizeof(RAWINPUTDEVICE)) == (UINT)-1) return;
    all.resize(count);
    for (const RAWINPUTDEVICE& d : all) {
        if (!IsPlayerInput(d)) continue;
        RAWINPUTDEVICE remove = d;
        remove.dwFlags = RIDEV_REMOVE;
        remove.hwndTarget = nullptr;
        if (RegisterRawInputDevices(&remove, 1, sizeof(remove))) g_suspendedRaw.push_back(d);
    }
}

void ResumeGameRawInput() {
    for (const RAWINPUTDEVICE& d : g_suspendedRaw) {
        if (d.hwndTarget && !IsWindow(d.hwndTarget)) continue;
        RegisterRawInputDevices(&d, 1, sizeof(d));
    }
    g_suspendedRaw.clear();
}

void BecomeChild(HWND game) {
    if (g_childOf == game) return;
    // SetParent's documentation: set WS_CHILD before parenting a former top-level window.
    SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_CHILD);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, BaseExStyle() | WS_EX_NOPARENTNOTIFY);
    SetParent(g_hwnd, game);
    g_childOf = game;
    SuspendGameRawInput();
    // XInput cannot be taken away from the game, and it never lost the activation. Tell it what Windows
    // would tell it if another app came to the front (not WM_ACTIVATE, which makes fullscreen Unity games
    // minimise): games stop reading controllers while in the background (Unreal by default, Unity's input
    // background behaviour). Undone in BecomePopup.
    PostMessageW(game, WM_ACTIVATEAPP, FALSE, (LPARAM)GetCurrentThreadId());
}

void BecomePopup() {
    if (!g_childOf) return;
    ResumeGameRawInput();
    if (IsWindow(g_childOf)) PostMessageW(g_childOf, WM_ACTIVATEAPP, TRUE, (LPARAM)GetCurrentThreadId());
    ShowWindow(g_hwnd, SW_HIDE);
    SetParent(g_hwnd, nullptr);
    SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_POPUP);
    g_childOf = nullptr;
}

void SetStyles(Mode m) {
    LONG_PTR ex = BaseExStyle() | WS_EX_TOPMOST;
    if (m == Mode::Toast) {
        ex |= (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
}

RECT TargetRect(Mode m, const GameWindow& gw) {
    RECT r{};
    if (m == Mode::Overlay && g_childOf) return { 0, 0, gw.client.right - gw.client.left, gw.client.bottom - gw.client.top };
    if (m == Mode::Overlay && gw.hwnd) return gw.client;
    if (m == Mode::Toast && gw.hwnd) {
        const ImVec2 sz = g_client->ToastSize();
        const LONG margin = (LONG)ui::S(kToastMargin);
        r.right = gw.client.right - margin;
        r.bottom = gw.client.bottom - margin;
        r.left = r.right - (LONG)sz.x;
        r.top = r.bottom - (LONG)sz.y;
        return r;
    }
    const ImVec2 sz = m == Mode::Dialog ? g_client->DialogSize() : ImVec2(1280, 720);
    if (m == Mode::Dialog && g_childOf) {
        const LONG w = gw.client.right - gw.client.left;
        const LONG h = gw.client.bottom - gw.client.top;
        const LONG cx = w / 2, cy = h / 2;
        return { cx - (LONG)sz.x / 2, cy - (LONG)sz.y / 2, cx + (LONG)sz.x / 2, cy + (LONG)sz.y / 2 };
    }
    // Centred on the game's or the primary monitor.
    HMONITOR mon = MonitorFromWindow(gw.hwnd ? gw.hwnd : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    const LONG cx = (mi.rcWork.left + mi.rcWork.right) / 2, cy = (mi.rcWork.top + mi.rcWork.bottom) / 2;
    return { cx - (LONG)sz.x / 2, cy - (LONG)sz.y / 2, cx + (LONG)sz.x / 2, cy + (LONG)sz.y / 2 };
}

void ApplyMode(Mode m, const GameWindow& gw) {
    if (m != g_mode) {
        const Mode from = g_mode;
        g_mode = m;
        // Give the focus back to the game (restoring it if it minimised when we took the focus) while our
        // window still has it: hiding first lets Windows activate the desktop, and then we may no longer
        // move the foreground.
        const bool giveBack = (from == Mode::Overlay || from == Mode::Dialog) && gw.hwnd;
        if (from == Mode::Overlay || from == Mode::Dialog) {
            ResumeGameRawInput();
            if (gw.hwnd) {
                DWORD gameThread = GetWindowThreadProcessId(gw.hwnd, NULL);
                DWORD myThread = GetCurrentThreadId();
                if (gameThread && gameThread != myThread) AttachThreadInput(gameThread, myThread, FALSE);
            }
        }
        if (g_childOf) {
            // Child of the game: the game never lost the activation, only the keyboard.
            if (IsWindow(g_childOf)) SetFocus(g_childOf);
            BecomePopup();
        } else if (giveBack) {
            if (gw.minimized) ShowWindowAsync(gw.hwnd, SW_RESTORE);
            SetForegroundWindow(gw.hwnd);
        }
        if ((m == Mode::Overlay || m == Mode::Dialog) && CanHostAsChild(gw)) {
            BecomeChild(gw.hwnd);
        } else if (m == Mode::Hidden) {
            ShowWindow(g_hwnd, SW_HIDE);
        } else {
            SetStyles(m);
            if ((m == Mode::Overlay || m == Mode::Dialog) && gw.hwnd) {
                SetWindowLongPtrW(g_hwnd, GWLP_HWNDPARENT, (LONG_PTR)gw.hwnd);
                ReleaseCapture();
                SuspendGameRawInput();
                DWORD gameThread = GetWindowThreadProcessId(gw.hwnd, NULL);
                DWORD myThread = GetCurrentThreadId();
                if (gameThread && gameThread != myThread) AttachThreadInput(gameThread, myThread, TRUE);
            }
        }
        if (giveBack && !g_childOf && GetForegroundWindow() != gw.hwnd) SetForegroundWindow(gw.hwnd);
        g_client->OnModeChanged(from, m);
    } else if (!g_childOf && (m == Mode::Overlay || m == Mode::Dialog) && CanHostAsChild(gw)) {
        BecomeChild(gw.hwnd);
    }
    if (m == Mode::Hidden) return;
    const RECT r = TargetRect(m, gw);
    static RECT placed{};
    ResizeSurface((UINT)(r.right - r.left), (UINT)(r.bottom - r.top));
    static Mode placedMode = Mode::Hidden;
    static HWND placedChildOf = nullptr;

    if (!EqualRect(&r, &placed) || placedMode != m || placedChildOf != g_childOf || !IsWindowVisible(g_hwnd)) {
        const UINT flags = SWP_FRAMECHANGED | (m == Mode::Toast ? SWP_NOACTIVATE : 0) | SWP_SHOWWINDOW;
        SetWindowPos(g_hwnd, g_childOf ? HWND_TOP : HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, flags);
        placed = r;
        placedMode = m;
        placedChildOf = g_childOf;
    }

    if (g_childOf) {
        // The parent and this thread share an input queue now, so SetFocus works across threads.
        if (GetFocus() != g_hwnd) {
            SetFocus(g_hwnd);
            ClipCursor(nullptr);
        }
    } else if (m == Mode::Overlay || m == Mode::Dialog) {
        ReleaseCapture();
        if (GetForegroundWindow() != g_hwnd) {
            SetForegroundWindow(g_hwnd);
        }
        SetActiveWindow(g_hwnd);
        SetFocus(g_hwnd);
        ClipCursor(nullptr);
    }
}

// Games that capture the mouse (cursor locked to the window centre, hidden) would leave the overlay without
// a usable pointer: free the clip each frame and let ImGui draw its own cursor while the OS one is hidden.
void FreeMouseForOverlay() {
    static DWORD s_lastFree = 0;
    DWORD now = GetTickCount();
    if (now - s_lastFree > 100) {
        s_lastFree = now;
        RECT clip{}, screen{ GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0 };
        screen.right = screen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        screen.bottom = screen.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (GetClipCursor(&clip) && !EqualRect(&clip, &screen)) ClipCursor(nullptr);
    }
    while (ShowCursor(TRUE) < 0);
    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
    ImGui::GetIO().MouseDrawCursor = false;
}

// Controller state into ImGui's gamepad keys (navigation: A/Cross activates, B/Circle goes back).
void FeedGamepad(const pad::State& s) {
    ImGuiIO& io = ImGui::GetIO();
    if (!s.connected) {
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        return;
    }
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    auto key = [&](ImGuiKey k, uint32_t b) { io.AddKeyEvent(k, (s.buttons & b) != 0); };
    key(ImGuiKey_GamepadStart, pad::kStart);
    key(ImGuiKey_GamepadBack, pad::kBack);
    key(ImGuiKey_GamepadFaceDown, pad::kA);
    key(ImGuiKey_GamepadFaceRight, pad::kB);
    key(ImGuiKey_GamepadFaceLeft, pad::kX);
    key(ImGuiKey_GamepadFaceUp, pad::kY);
    key(ImGuiKey_GamepadDpadUp, pad::kUp);
    key(ImGuiKey_GamepadDpadDown, pad::kDown);
    key(ImGuiKey_GamepadDpadLeft, pad::kLeft);
    key(ImGuiKey_GamepadDpadRight, pad::kRight);
    key(ImGuiKey_GamepadL1, pad::kLB);
    key(ImGuiKey_GamepadR1, pad::kRB);
    key(ImGuiKey_GamepadL3, pad::kL3);
    key(ImGuiKey_GamepadR3, pad::kR3);
    auto analog = [&](ImGuiKey k, float v) { io.AddKeyAnalogEvent(k, v > 0.10f, std::max(v, 0.0f)); };
    analog(ImGuiKey_GamepadL2, s.lt);
    analog(ImGuiKey_GamepadR2, s.rt);
    analog(ImGuiKey_GamepadLStickLeft, -s.lx);
    analog(ImGuiKey_GamepadLStickRight, s.lx);
    analog(ImGuiKey_GamepadLStickUp, s.ly);
    analog(ImGuiKey_GamepadLStickDown, -s.ly);
    analog(ImGuiKey_GamepadRStickLeft, -s.rx);
    analog(ImGuiKey_GamepadRStickRight, s.rx);
    analog(ImGuiKey_GamepadRStickUp, s.ry);
    analog(ImGuiKey_GamepadRStickDown, -s.ry);
}

void Render(const GameWindow& gw, const pad::State& padState) {
    FeedGamepad(padState);
    if (g_mode == Mode::Overlay || g_mode == Mode::Dialog) FreeMouseForOverlay();
    else ImGui::GetIO().MouseDrawCursor = false;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    g_client->Frame(gw, ImVec2((float)g_width, (float)g_height));
    ImGui::Render();
    const float clear[4] = { 0, 0, 0, 0 };
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT hr = PresentFrame(1);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) g_deviceLost = true;
}

// The GPU device can go away under us (driver update or crash/TDR, a laptop switching GPUs, an eGPU
// unplugged). Rebuild device, swap chain and composition on whatever adapter is there now; textures are
// reloaded on demand.
bool RecreateDevice() {
    const HRESULT reason = g_device ? g_device->GetDeviceRemovedReason() : E_FAIL;
    ReFixLog("[Overlay] GPU device lost (0x%08X); recreating it", (unsigned)reason);
    ImGui_ImplDX11_Shutdown();
    for (auto& [key, t] : g_textures) SafeRelease(t.srv);
    g_textures.clear();
    SafeRelease(g_rtv);
    ReleaseLayeredSurface();
    SafeRelease(g_visual);
    SafeRelease(g_target);
    SafeRelease(g_dcomp);
    SafeRelease(g_swap);
    SafeRelease(g_ctx);
    SafeRelease(g_device);
    const UINT w = std::max<UINT>(g_width, 8), h = std::max<UINT>(g_height, 8);
    g_deviceLost = false;
    if (!CreateDeviceAndSurface(w, h) || !ImGui_ImplDX11_Init(g_device, g_ctx)) {
        ReFixLog("[Overlay] Could not recreate the GPU device; overlay stopped");
        return false;
    }
    return true;
}

bool InitThread() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST, kClassName, L"ReFix LAN",
                             WS_POPUP, 0, 0, std::max(64, GetSystemMetrics(SM_CXSCREEN)), std::max(64, GetSystemMetrics(SM_CYSCREEN)), nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd || !CreateDeviceAndSurface((UINT)std::max(64, GetSystemMetrics(SM_CXSCREEN)), (UINT)std::max(64, GetSystemMetrics(SM_CYSCREEN)))) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    const float scale = ScaleFor(nullptr);
    ui::BuildFonts(scale);
    ui::ApplyStyle(scale);
    return true;
}

void MaybeRescale(const GameWindow& gw) {
    const float want = ScaleFor(&gw);
    if (std::fabs(want - ui::F().scale) < 0.01f) return;
    ui::BuildFonts(want);
    ui::ApplyStyle(want);
    ImGui_ImplDX11_InvalidateDeviceObjects();
    if (g_device && g_ctx) {
        ImGui_ImplDX11_CreateDeviceObjects();
    }
}

DWORD WINAPI HostThread(LPVOID) {
    g_initOk = InitThread();
    g_running = g_initOk;
    SetEvent(g_ready);
    if (!g_initOk) {
        ReFixLog("[Overlay] Host could not start; overlay disabled for this session");
        return 0;
    }
    ULONGLONG lastScan = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_wantHotkeyHook && PollHotkey()) g_hotkeyPending = true;
        const ULONGLONG now = GetTickCount64();
        if (now - lastScan >= kGameScanMs) {
            ScanGameWindow();
            lastScan = now;
        }
        // Controllers: read only while the game has the foreground or the overlay is up (nothing to do
        // otherwise); Guide/PS or View+Menu toggles the overlay like Shift+Tab.
        pad::State padState;
        if (g_wantHotkeyHook && (g_mode != Mode::Hidden || CurrentGame().foreground)) {
            padState = pad::Poll();
            if (pad::ToggleRequested(padState) && IsOurProcessWindow(GetForegroundWindow())) g_hotkeyPending = true;
        }
        if (g_hotkeyPending.exchange(false)) g_client->OnHotkey();
        const GameWindow gw = CurrentGame();
        Mode m = g_client->DesiredMode();
        if ((m == Mode::Toast && (!gw.hwnd || gw.minimized)) || (m == Mode::Overlay && !gw.hwnd)) m = Mode::Hidden;
        // Another app (or another game instance) is in front: our topmost window steps aside without changing
        // mode, and comes back over its own game when that game is in front again.
        const HWND fg = GetForegroundWindow();
        const bool away = !g_childOf && m != Mode::Hidden && gw.hwnd && fg && !IsOurProcessWindow(fg);
        if (away) {
            if (IsWindowVisible(g_hwnd)) ShowWindow(g_hwnd, SW_HIDE);
        } else {
            if (m != Mode::Hidden && m != g_mode) MaybeRescale(gw);
            ApplyMode(m, gw);
        }
        if (g_mode == Mode::Hidden || away) {
            // Keep ImGui's frame clock and the client's timers running cheaply.
            MsgWaitForMultipleObjects(0, nullptr, FALSE, kIdleWaitMs, QS_ALLINPUT);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            g_client->Frame(gw, ImVec2(0, 0));
            ImGui::EndFrame();
            continue;
        }
        if (!IsWindow(g_hwnd)) {
            // Only possible while we were a child of a game window that got destroyed.
            ReFixLog("[Overlay] Overlay window destroyed with the game window; overlay stopped");
            g_running = false;
            return 0;
        }
        if (g_deviceLost && !RecreateDevice()) {
            g_running = false;
            return 0;
        }
        Render(gw, padState);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 2, QS_ALLINPUT);
    }
}

} // namespace

bool Start(Client* client) {
    if (g_running) return true;
    if (g_ready) return false;   // a previous start failed
    g_client = client;
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE t = CreateThread(nullptr, 0, HostThread, nullptr, 0, &g_threadId);
    if (!t) return false;
    CloseHandle(t);
    WaitForSingleObject(g_ready, INFINITE);
    return g_initOk;
}

bool IsRunning() { return g_running; }
void Wake() { if (g_threadId) PostThreadMessageW(g_threadId, kWakeMsg, 0, 0); }
void EnableHotkey() { g_wantHotkeyHook = true; Wake(); }
HWND Window() { return g_hwnd; }

GameWindow CurrentGame() {
    AcquireSRWLockShared(&g_gameLock);
    GameWindow gw = g_game;
    ReleaseSRWLockShared(&g_gameLock);
    return gw;
}

ImTextureID Texture(const std::string& key, const std::function<bool(image::Rgba&)>& load) {
    auto it = g_textures.find(key);
    if (it != g_textures.end()) return it->second.failed ? 0 : (ImTextureID)it->second.srv;
    TextureEntry& t = g_textures[key];
    image::Rgba img;
    if (!load(img) || img.width <= 0 || img.height <= 0) { t.failed = true; return 0; }
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = (UINT)img.width;
    desc.Height = (UINT)img.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = { img.pixels.data(), (UINT)img.width * 4, 0 };
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g_device->CreateTexture2D(&desc, &data, &tex))) { t.failed = true; return 0; }
    g_device->CreateShaderResourceView(tex, nullptr, &t.srv);
    tex->Release();
    if (!t.srv) t.failed = true;
    return t.failed ? 0 : (ImTextureID)t.srv;
}

bool HasTexture(const std::string& key) {
    auto it = g_textures.find(key);
    return it != g_textures.end() && !it->second.failed;
}

void DropTexture(const std::string& key) {
    auto it = g_textures.find(key);
    if (it == g_textures.end()) return;
    SafeRelease(it->second.srv);
    g_textures.erase(it);
}

} // namespace refix::overlay::host
