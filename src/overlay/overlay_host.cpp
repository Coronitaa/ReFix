#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_host.h"
#include "overlay_theme.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>

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
Mode g_mode = Mode::Hidden;
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_MOUSEACTIVATE:
        if (g_mode == Mode::Toast) return MA_NOACTIVATE;
        break;
    case WM_CLOSE:
        if (g_client) g_client->OnCloseRequested();
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
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
    hr = factory ? factory->CreateSwapChainForComposition(g_device, &sd, nullptr, &g_swap) : E_FAIL;
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
    if (FAILED(hr)) { ReFixLog("[Overlay] DirectComposition setup failed 0x%08X", (unsigned)hr); return false; }
    g_width = w;
    g_height = h;
    CreateRenderTarget();
    return true;
}

void ResizeSurface(UINT w, UINT h) {
    w = std::max<UINT>(w, 8);
    h = std::max<UINT>(h, 8);
    if (w == g_width && h == g_height) return;
    SafeRelease(g_rtv);
    g_swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    g_width = w;
    g_height = h;
    CreateRenderTarget();
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
// Exclusive-fullscreen games that lost the focus to the profile dialog stop presenting until they are
// minimised and restored: ApplyMode minimises, the loop restores at this tick.
ULONGLONG g_restoreGameAt = 0;
HWND g_restoreGame = nullptr;
constexpr ULONGLONG kRestoreDelayMs = 400;

bool CoversMonitor(HWND w) {
    RECT wr{};
    if (!GetWindowRect(w, &wr)) return false;
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi);
    return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right &&
           wr.bottom >= mi.rcMonitor.bottom;
}

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

float ScaleFor(HWND anchor) {
    HMONITOR mon = MonitorFromWindow(anchor ? anchor : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    const float h = (float)(mi.rcMonitor.bottom - mi.rcMonitor.top);
    return std::clamp(std::round(h / 1080.0f * 4.0f) / 4.0f, 1.0f, 2.0f);
}

void SetStyles(Mode m) {
    LONG_PTR ex = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST;
    ex |= (m == Mode::Dialog) ? WS_EX_APPWINDOW : (WS_EX_TOOLWINDOW | (m == Mode::Toast ? WS_EX_NOACTIVATE : 0));
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
}

RECT TargetRect(Mode m, const GameWindow& gw) {
    RECT r{};
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
    // Dialog (or overlay without a game window): centred on the game's or the primary monitor.
    const ImVec2 sz = m == Mode::Dialog ? g_client->DialogSize() : ImVec2(1280, 720);
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
        if (m == Mode::Hidden) {
            ShowWindow(g_hwnd, SW_HIDE);
        } else {
            SetStyles(m);
        }
        // Give the focus back to the game (restoring it if it minimised when we took the focus).
        if ((from == Mode::Overlay || from == Mode::Dialog) && gw.hwnd) {
            if (gw.minimized) {
                ShowWindowAsync(gw.hwnd, SW_RESTORE);
            } else if (from == Mode::Dialog && CoversMonitor(gw.hwnd)) {
                ShowWindowAsync(gw.hwnd, SW_MINIMIZE);
                g_restoreGame = gw.hwnd;
                g_restoreGameAt = GetTickCount64() + kRestoreDelayMs;
            }
            SetForegroundWindow(gw.hwnd);
        }
        g_client->OnModeChanged(from, m);
    }
    if (m == Mode::Hidden) return;
    const RECT r = TargetRect(m, gw);
    static RECT placed{};
    static Mode placedMode = Mode::Hidden;
    if (!EqualRect(&r, &placed) || placedMode != m || !IsWindowVisible(g_hwnd)) {
        const UINT flags = SWP_FRAMECHANGED | (m == Mode::Toast ? SWP_NOACTIVATE : 0) | SWP_SHOWWINDOW;
        SetWindowPos(g_hwnd, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, flags);
        placed = r;
        placedMode = m;
    }
    ResizeSurface((UINT)(r.right - r.left), (UINT)(r.bottom - r.top));
    if ((m == Mode::Overlay || m == Mode::Dialog) && GetForegroundWindow() != g_hwnd) {
        SetForegroundWindow(g_hwnd);
        if (m == Mode::Overlay) ClipCursor(nullptr);
    }
}

void Render(const GameWindow& gw) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    g_client->Frame(gw, ImVec2((float)g_width, (float)g_height));
    ImGui::Render();
    const float clear[4] = { 0, 0, 0, 0 };
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(1, 0);
}

bool InitThread() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClassName, L"ReFix LAN",
                             WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd || !CreateDeviceAndSurface(64, 64)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    const float scale = ScaleFor(nullptr);
    ui::BuildFonts(scale);
    ui::ApplyStyle(scale);
    return true;
}

void MaybeRescale(const GameWindow& gw) {
    const float want = ScaleFor(gw.hwnd);
    if (std::fabs(want - ui::F().scale) < 0.01f) return;
    ui::BuildFonts(want);
    ui::ApplyStyle(want);
    ImGui_ImplDX11_InvalidateDeviceObjects();
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
        if (now - lastScan >= kGameScanMs || g_mode == Mode::Overlay || g_mode == Mode::Toast) {
            ScanGameWindow();
            lastScan = now;
        }
        if (g_restoreGame && now >= g_restoreGameAt) {
            ShowWindowAsync(g_restoreGame, SW_RESTORE);
            SetForegroundWindow(g_restoreGame);
            g_restoreGame = nullptr;
        }
        if (g_hotkeyPending.exchange(false)) g_client->OnHotkey();
        const GameWindow gw = CurrentGame();
        Mode m = g_client->DesiredMode();
        if ((m == Mode::Toast && (!gw.hwnd || gw.minimized)) || (m == Mode::Overlay && !gw.hwnd)) m = Mode::Hidden;
        if (m != Mode::Hidden && m != g_mode) MaybeRescale(gw);
        ApplyMode(m, gw);
        if (g_mode == Mode::Hidden) {
            // Keep ImGui's frame clock and the client's timers running cheaply.
            MsgWaitForMultipleObjects(0, nullptr, FALSE, kIdleWaitMs, QS_ALLINPUT);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            g_client->Frame(gw, ImVec2(0, 0));
            ImGui::EndFrame();
            continue;
        }
        Render(gw);
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
