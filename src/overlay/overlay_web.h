// In-overlay web browser: a WebView2 control hosted as a child window of the overlay window, the way
// Steam shows store and community pages inside its overlay. Host thread only (it is STA and pumps the
// messages WebView2 answers through). The runtime is started on the first page, never before.
#pragma once
#include <windows.h>
#include <string>

namespace refix::overlay::web {

enum class State { Idle, Starting, Ready, Failed };

// Shows `url` (http/https only) once the control is ready. False when WebView2 cannot be used at all.
bool Open(HWND parent, const std::wstring& url);
// Hides the page and unloads it (stops video and sound); the control is kept for the next page.
void Close();
void SetBounds(const RECT& clientRect);   // in the parent's client pixels
void SetVisible(bool visible);

State GetState();
bool Loading();
bool CanGoBack();
bool CanGoForward();
std::wstring Title();
std::wstring Url();
void Back();
void Forward();
void Reload();
void Stop();
// Escape pressed while the page had the keyboard focus (the overlay never sees that key then).
bool TakeCloseRequest();

} // namespace refix::overlay::web
