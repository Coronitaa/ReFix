// WebView2 hosting for the overlay browser. See overlay_web.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_web.h"
#include "profile_store.h"

#include "../webview2/include/WebView2.h"

#include <functional>
#include <utility>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::web {
namespace {

// Minimal COM completion/event handler around a lambda (WRL is not used anywhere else in ReFix).
template <class I, class A, class B>
class Handler final : public I {
public:
    explicit Handler(std::function<HRESULT(A, B)> fn) : fn_(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(I)) {
            *out = static_cast<I*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(A a, B b) override { return fn_(a, b); }

private:
    std::function<HRESULT(A, B)> fn_;
    LONG ref_ = 1;
};

// Creates a handler, hands it to `use` (which AddRefs what it keeps) and drops our reference.
template <class I, class A, class B, class Use>
HRESULT WithHandler(std::function<HRESULT(A, B)> fn, Use use) {
    I* h = new Handler<I, A, B>(std::move(fn));
    const HRESULT hr = use(h);
    h->Release();
    return hr;
}

template <typename T> void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

HWND g_parent = nullptr;
ICoreWebView2Environment* g_env = nullptr;
ICoreWebView2Controller* g_ctl = nullptr;
ICoreWebView2* g_view = nullptr;
State g_state = State::Idle;
std::wstring g_pending;          // page asked for before the control was ready
RECT g_bounds{};
bool g_visible = false;
bool g_loading = false;
bool g_closeRequested = false;
bool g_focusOnShow = false;

bool IsWebUrl(const std::wstring& u) { return u.rfind(L"https://", 0) == 0 || u.rfind(L"http://", 0) == 0; }

std::wstring TakeString(LPWSTR s) {
    std::wstring out = s ? s : L"";
    if (s) CoTaskMemFree(s);
    return out;
}

void Fail(const char* what, HRESULT hr) {
    ReFixLog("[Overlay] Web view unavailable (%s, 0x%08X): pages open in the system browser", what, (unsigned)hr);
    g_state = State::Failed;
    SafeRelease(g_view);
    SafeRelease(g_ctl);
    SafeRelease(g_env);
}

void Navigate(const std::wstring& url) {
    if (!g_view || !IsWebUrl(url)) return;
    g_loading = true;
    g_view->Navigate(url.c_str());
}

void ApplyBounds() {
    if (g_ctl) g_ctl->put_Bounds(g_bounds);
}

void WireEvents() {
    EventRegistrationToken tok{};
    // Links that open a new window stay in this view (there is no tab strip).
    WithHandler<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            const std::wstring u = TakeString(uri);
            args->put_Handled(TRUE);
            Navigate(u);
            return S_OK;
        },
        [&](ICoreWebView2NewWindowRequestedEventHandler* h) { return g_view->add_NewWindowRequested(h, &tok); });
    // Only web pages: no file://, no app protocols launched from a page.
    WithHandler<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            const std::wstring u = TakeString(uri);
            if (!IsWebUrl(u) && u != L"about:blank") args->put_Cancel(TRUE);
            else g_loading = true;
            return S_OK;
        },
        [&](ICoreWebView2NavigationStartingEventHandler* h) { return g_view->add_NavigationStarting(h, &tok); });
    WithHandler<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) {
            g_loading = false;
            return S_OK;
        },
        [&](ICoreWebView2NavigationCompletedEventHandler* h) { return g_view->add_NavigationCompleted(h, &tok); });
    WithHandler<ICoreWebView2AcceleratorKeyPressedEventHandler, ICoreWebView2Controller*,
                ICoreWebView2AcceleratorKeyPressedEventArgs*>(
        [](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
            COREWEBVIEW2_KEY_EVENT_KIND kind{};
            UINT key = 0;
            args->get_KeyEventKind(&kind);
            args->get_VirtualKey(&key);
            if (key == VK_ESCAPE && (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)) {
                g_closeRequested = true;
                args->put_Handled(TRUE);
            }
            return S_OK;
        },
        [&](ICoreWebView2AcceleratorKeyPressedEventHandler* h) { return g_ctl->add_AcceleratorKeyPressed(h, &tok); });
}

HRESULT OnController(HRESULT hr, ICoreWebView2Controller* ctl) {
    if (FAILED(hr) || !ctl) { Fail("controller", hr); return S_OK; }
    g_ctl = ctl;
    g_ctl->AddRef();
    g_ctl->get_CoreWebView2(&g_view);
    if (!g_view) { Fail("core view", E_NOINTERFACE); return S_OK; }

    ICoreWebView2Controller2* ctl2 = nullptr;
    if (SUCCEEDED(g_ctl->QueryInterface(IID_PPV_ARGS(&ctl2))) && ctl2) {
        ctl2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{ 255, 18, 22, 29 });   // no white flash
        ctl2->Release();
    }
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(g_view->get_Settings(&settings)) && settings) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(TRUE);
        settings->Release();
    }
    WireEvents();
    g_state = State::Ready;
    ApplyBounds();
    g_ctl->put_IsVisible(g_visible ? TRUE : FALSE);
    ReFixLog("[Overlay] Web view ready");
    if (!g_pending.empty()) {
        Navigate(g_pending);
        g_pending.clear();
    }
    return S_OK;
}

HRESULT OnEnvironment(HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) { Fail("environment", hr); return S_OK; }
    g_env = env;
    g_env->AddRef();
    hr = WithHandler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>(
        OnController,
        [](ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* h) {
            return g_env->CreateCoreWebView2Controller(g_parent, h);
        });
    if (FAILED(hr)) Fail("create controller", hr);
    return S_OK;
}

void StartRuntime() {
    LPWSTR version = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    const std::wstring v = TakeString(version);
    if (FAILED(hr) || v.empty()) { Fail("runtime not installed", hr); return; }
    g_state = State::Starting;
    // One profile folder for the browser (cookies, logins) next to the ReFix LAN profiles.
    const std::wstring dataDir = profiles::RootDir() + L"\\WebView2";
    hr = WithHandler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
        OnEnvironment,
        [&](ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* h) {
            return CreateCoreWebView2EnvironmentWithOptions(nullptr, dataDir.c_str(), nullptr, h);
        });
    if (FAILED(hr)) Fail("create environment", hr);
}

} // namespace

bool Open(HWND parent, const std::wstring& url) {
    if (!IsWebUrl(url) || !parent) return false;
    if (g_state == State::Failed) return false;
    g_parent = parent;
    g_focusOnShow = true;
    g_closeRequested = false;
    if (g_state == State::Ready) {
        Navigate(url);
    } else {
        g_pending = url;
        if (g_state == State::Idle) StartRuntime();
    }
    return g_state != State::Failed;
}

void Close() {
    g_pending.clear();
    g_closeRequested = false;
    SetVisible(false);
    if (g_view) g_view->Navigate(L"about:blank");
    g_loading = false;
}

void SetBounds(const RECT& r) {
    if (EqualRect(&r, &g_bounds)) return;
    g_bounds = r;
    ApplyBounds();
}

void SetVisible(bool visible) {
    if (visible == g_visible) return;
    g_visible = visible;
    if (!g_ctl) return;
    g_ctl->put_IsVisible(visible ? TRUE : FALSE);
    if (visible && g_focusOnShow) {
        g_ctl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        g_focusOnShow = false;
    }
    if (!visible && g_parent) SetFocus(g_parent);   // keys go back to the overlay
}

State GetState() { return g_state; }
bool Loading() { return g_state == State::Starting || g_loading; }

bool CanGoBack() {
    BOOL b = FALSE;
    if (g_view) g_view->get_CanGoBack(&b);
    return b != FALSE;
}

bool CanGoForward() {
    BOOL b = FALSE;
    if (g_view) g_view->get_CanGoForward(&b);
    return b != FALSE;
}

std::wstring Title() {
    LPWSTR s = nullptr;
    if (g_view) g_view->get_DocumentTitle(&s);
    return TakeString(s);
}

std::wstring Url() {
    if (!g_view) return g_pending;
    LPWSTR s = nullptr;
    g_view->get_Source(&s);
    return TakeString(s);
}

void Back() { if (g_view) g_view->GoBack(); }
void Forward() { if (g_view) g_view->GoForward(); }
void Reload() { if (g_view) { g_loading = true; g_view->Reload(); } }
void Stop() { if (g_view) { g_view->Stop(); g_loading = false; } }

bool TakeCloseRequest() {
    const bool r = g_closeRequested;
    g_closeRequested = false;
    return r;
}

} // namespace refix::overlay::web
