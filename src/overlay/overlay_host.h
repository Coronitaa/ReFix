// The overlay's own window: a top-most, per-pixel transparent DirectComposition surface placed over the
// game's client area. Nothing is injected into the game's renderer, so it works the same for D3D9/11/12,
// OpenGL and Vulkan games in windowed or borderless mode. Runs on its own thread with D3D11 + Dear ImGui.
#pragma once
#include <windows.h>
#include <functional>
#include <string>

#include "../imgui/imgui.h"
#include "avatar_image.h"

namespace refix::overlay::host {

enum class Mode {
    Hidden,     // nothing on screen
    Toast,      // small click-through-free card in the game's bottom-right corner, never takes focus
    Overlay,    // covers the game and takes keyboard/mouse
    Dialog,     // centred window of its own (profile creation before the game window exists)
};

struct GameWindow {
    HWND hwnd = nullptr;
    RECT client{};          // screen coordinates
    bool foreground = false;
    bool minimized = false; // fullscreen game minimised by the overlay taking the focus; client = last rect
    std::wstring title;
};

// Implemented by the overlay app; called on the host thread.
struct Client {
    virtual ~Client() = default;
    virtual Mode DesiredMode() = 0;
    virtual ImVec2 ToastSize() = 0;          // used in Mode::Toast
    virtual ImVec2 DialogSize() = 0;         // used in Mode::Dialog
    virtual void Frame(const GameWindow& game, ImVec2 viewport) = 0;
    virtual void OnHotkey() = 0;             // Shift+Tab while the game has focus
    virtual void OnModeChanged(Mode from, Mode to) = 0;
    virtual void OnCloseRequested() = 0;     // Alt+F4 on the dialog
};

// Starts the host thread (idempotent). Returns false when the window or D3D11 could not be created.
bool Start(Client* client);
bool IsRunning();
void Wake();                                  // re-evaluate the mode now
void EnableHotkey();                          // start watching Shift+Tab
HWND Window();
GameWindow CurrentGame();

// Texture cache (host thread only). `load` runs once per key; failures are remembered.
ImTextureID Texture(const std::string& key, const std::function<bool(image::Rgba&)>& load);
bool HasTexture(const std::string& key);
void DropTexture(const std::string& key);

} // namespace refix::overlay::host
