#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

extern void ReFixLog(const char* fmt, ...) {}

#include "../src/overlay/profile_store.h"
#include "../src/overlay/profile_sync.h"
#include "../src/overlay/cloud_account.h"
#include "../src/overlay/nvidia_reflex_block.h"
#include "../src/overlay/overlay_sound.h"

// Stubs for cloud & sync & reflex
namespace refix::overlay {
    namespace profiles {
        std::wstring RootDir() { return L"C:\\Users\\andre\\AppData\\Roaming\\ReFix"; }
    }
    namespace sound {
        void SetEnabled(bool) {}
        void Play(Cue) {}
    }
    std::wstring PickProfileFile(bool, const wchar_t*, const std::wstring&, std::string&) { return L""; }
    bool NvidiaReflexBlockArmed() { return false; }

    namespace cloud {
        const char* Label(Provider) { return ""; }
        const wchar_t* Choice(Provider) { return L""; }
        bool FromChoice(const std::wstring&, Provider&) { return false; }
        bool Available(Provider) { return false; }
        bool Connected(Provider) { return false; }
        void SignInAsync(Provider) {}
        bool SigningIn() { return false; }
        bool SigningIn(Provider) { return false; }
        void CancelSignIn() {}
        Msg LastError() { return Msg{}; }
        void Disconnect(Provider) {}
        Account AccountOf(Provider) { return Account{}; }
    }
    namespace sync {
        std::vector<CloudFolder> CloudFolders() { return {}; }
        std::wstring ChosenFolder() { return L""; }
        std::wstring EffectiveFolder() { return L""; }
        void SetFolder(const std::wstring&) {}
        bool AutoSync() { return false; }
        void SetAutoSync(bool) {}
        bool SyncLocalProfiles() { return false; }
        void SetSyncLocalProfiles(bool) {}
        bool ExportTo(const Profile&, const std::wstring&, std::string&) { return false; }
        std::optional<Profile> ImportAsNewProfile(const std::wstring&, std::string&) { return std::nullopt; }
        Status Get() { return Status{}; }
    }
    namespace nvidia_reflex_block {
        bool IsBlocked() { return true; }
        void SetBlocked(bool) {}
    }
}

// ImGui source files
#include "../src/imgui/imgui.cpp"
#include "../src/imgui/imgui_draw.cpp"
#include "../src/imgui/imgui_widgets.cpp"
#include "../src/imgui/imgui_tables.cpp"
#include "../src/imgui/imgui_impl_dx11.cpp"

// Overlay modules
#include "../src/overlay/overlay_i18n.cpp"
#include "../src/overlay/overlay_theme.cpp"
#include "../src/overlay/overlay_settings.cpp"

#define private public
#include "../src/overlay/overlay_app.h"
#undef private

namespace refix::overlay {
    static App g_mockApp;
    App& App::Get() { return g_mockApp; }
    void App::RestartGame() {}
    void App::CloseOverlay() {}
    void App::DrawAchievementsModal(ImVec2) {}
    void App::ReloadProfiles() {}
    void App::ShowInfo(const std::string&, const std::string&) {}
    host::Mode App::DesiredMode() { return host::Mode::Overlay; }
    ImVec2 App::ToastSize() { return ImVec2(0, 0); }
    ImVec2 App::DialogSize() { return ImVec2(0, 0); }
    void App::Frame(const host::GameWindow&, ImVec2) {}
    void App::OnHotkey() {}
    void App::OnModeChanged(host::Mode, host::Mode) {}
    void App::OnCloseRequested() {}
    std::wstring PickFolder(const wchar_t*, std::string&) { return L""; }
}

#include "../src/overlay/overlay_ui_settings.cpp"

void SaveTextureToBmp(ID3D11DeviceContext* ctx, ID3D11Texture2D* tex, ID3D11Texture2D* staging, int w, int h, const char* filename) {
    ctx->CopyResource(staging, tex);
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        FILE* f = fopen(filename, "wb");
        if (f) {
            #pragma pack(push, 1)
            struct BMPHeader {
                uint16_t bfType = 0x4D42;
                uint32_t bfSize;
                uint16_t bfReserved1 = 0;
                uint16_t bfReserved2 = 0;
                uint32_t bfOffBits = 54;
                uint32_t biSize = 40;
                int32_t  biWidth;
                int32_t  biHeight;
                uint16_t biPlanes = 1;
                uint16_t biBitCount = 32;
                uint32_t biCompression = 0;
                uint32_t biSizeImage = 0;
                int32_t  biXPelsPerMeter = 2835;
                int32_t  biYPelsPerMeter = 2835;
                uint32_t biClrUsed = 0;
                uint32_t biClrImportant = 0;
            } hdr;
            #pragma pack(pop)
            hdr.bfSize = 54 + w * h * 4;
            hdr.biWidth = w;
            hdr.biHeight = -h;
            fwrite(&hdr, sizeof(hdr), 1, f);
            const uint8_t* src = (const uint8_t*)mapped.pData;
            for (int r = 0; r < h; ++r) {
                const uint32_t* rowIn = (const uint32_t*)(src + r * mapped.RowPitch);
                std::vector<uint32_t> rowOut(w);
                for (int c = 0; c < w; ++c) {
                    uint32_t p = rowIn[c];
                    uint8_t r_col = p & 0xFF;
                    uint8_t g_col = (p >> 8) & 0xFF;
                    uint8_t b_col = (p >> 16) & 0xFF;
                    uint8_t a_col = 255;
                    rowOut[c] = (a_col << 24) | (r_col << 16) | (g_col << 8) | b_col;
                }
                fwrite(rowOut.data(), w * 4, 1, f);
            }
            fclose(f);
            printf("[OK] Wrote %s\n", filename);
        }
        ctx->Unmap(staging, 0);
    }
}

int main() {
    printf("[*] Initializing Standalone UI Render Test...\n");

    const int width = 1280;
    const int height = 720;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                                   D3D11_SDK_VERSION, &device, &featureLevel, &context);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                               D3D11_SDK_VERSION, &device, &featureLevel, &context);
    }
    if (FAILED(hr) || !device || !context) {
        printf("[!] Failed to create D3D11 device: 0x%08X\n", (unsigned)hr);
        return 1;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;

    ID3D11Texture2D* rtTex = nullptr;
    hr = device->CreateTexture2D(&td, nullptr, &rtTex);
    if (FAILED(hr)) return 1;

    ID3D11RenderTargetView* rtv = nullptr;
    hr = device->CreateRenderTargetView(rtTex, nullptr, &rtv);
    if (FAILED(hr)) return 1;

    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* stagingTex = nullptr;
    hr = device->CreateTexture2D(&td, nullptr, &stagingTex);
    if (FAILED(hr)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;

    ImGui_ImplDX11_Init(device, context);
    refix::overlay::ui::BuildFonts(1.0f);
    refix::overlay::ui::ApplyStyle(1.0f);

    auto& app = refix::overlay::App::Get();
    app.gameTitle_ = L"Counter-Strike: Global Offensive";

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    context->RSSetViewports(1, &vp);

    auto renderFrame = [&](const char* bmpPath, std::function<void(int)> preDraw) {
        for (int frame = 0; frame < 3; ++frame) {
            float clearColor[4] = { 0.06f, 0.07f, 0.09f, 1.0f };
            context->ClearRenderTargetView(rtv, clearColor);
            context->OMSetRenderTargets(1, &rtv, nullptr);

            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();

            preDraw(frame);

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
        SaveTextureToBmp(context, rtTex, stagingTex, width, height, bmpPath);
    };

    const std::string outDir = "C:\\Users\\andre\\.gemini\\antigravity\\brain\\d7e914b3-be88-453f-9f51-ab1f284eb800\\scratch\\";

    auto scrollSettingsToBottom = []() {
        ImGuiWindow* win = ImGui::FindWindowByName("##settings");
        if (win) {
            for (ImGuiWindow* child : win->DC.ChildWindows) {
                if (strstr(child->Name, "##settingsbody")) {
                    child->Scroll.y = child->ScrollMax.y;
                }
            }
        }
    };

    // Warm-up frame
    renderFrame((outDir + "warmup.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
    });

    // ================= ENGLISH =================
    refix::overlay::i18n::Set(refix::overlay::i18n::Lang::En);

    // Frame 1: General settings (Default: Fullscreen Windowed)
    renderFrame((outDir + "test_settings_general.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        auto vals = refix::overlay::settings::Get();
        vals.displayMode = refix::overlay::settings::DisplayMode::FullscreenWindowed;
        refix::overlay::settings::Set(vals);
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
    });

    // Frame 2: General settings scrolled to Overlay Mode (horizontal buttons)
    renderFrame((outDir + "test_settings_overlay_mode.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        auto vals = refix::overlay::settings::Get();
        vals.displayMode = refix::overlay::settings::DisplayMode::FullscreenWindowed;
        refix::overlay::settings::Set(vals);
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
        scrollSettingsToBottom();
    });

    // Frame 3: General settings with Windowed selected (showing resolution selector)
    renderFrame((outDir + "test_settings_windowed.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        auto vals = refix::overlay::settings::Get();
        vals.displayMode = refix::overlay::settings::DisplayMode::Windowed;
        vals.windowWidth = 1920;
        vals.windowHeight = 1080;
        refix::overlay::settings::Set(vals);
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
    });

    // Frame 4: Caution Modal 1 (Steam Overlay warning - English)
    renderFrame((outDir + "test_modal1_steam.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) refix::overlay::g_overlayWarn1Open = true;
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 5: Caution Modal 2 (Steam Overlay final confirmation with Hold button - English)
    renderFrame((outDir + "test_modal2_steam.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
            refix::overlay::g_overlayHoldTime = 0.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 6: Caution Modal 2 (Disable Overlay final confirmation in Danger color - English)
    renderFrame((outDir + "test_modal2_disable.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::DisableAll;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
            refix::overlay::g_overlayHoldTime = 0.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 7: Caution Modal 2 mid-animation (Holding at 2.5s with decremental progress bar filled - English)
    renderFrame((outDir + "test_modal2_holding.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
        }
        if (frame > 0) {
            refix::overlay::g_overlayHoldTime = 2.5f + (1.0f / 60.0f) * 8.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // ================= SPANISH =================
    refix::overlay::i18n::Set(refix::overlay::i18n::Lang::Es);

    // Frame 8: General settings (Default: Spanish)
    renderFrame((outDir + "test_settings_general_es.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        auto vals = refix::overlay::settings::Get();
        vals.displayMode = refix::overlay::settings::DisplayMode::FullscreenWindowed;
        refix::overlay::settings::Set(vals);
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
    });

    // Frame 9: General settings scrolled to Overlay Mode (Spanish horizontal buttons)
    renderFrame((outDir + "test_settings_overlay_mode_es.bmp").c_str(), [&](int) {
        app.settingsOpen_ = true;
        app.settingsOpenedAt_ = GetTickCount64() - 1000;
        app.settingsSection_ = 0;
        auto vals = refix::overlay::settings::Get();
        vals.displayMode = refix::overlay::settings::DisplayMode::FullscreenWindowed;
        refix::overlay::settings::Set(vals);
        app.DrawSettingsModal(ImVec2((float)width, (float)height));
        scrollSettingsToBottom();
    });

    // Frame 10: Caution Modal 1 (Spanish)
    renderFrame((outDir + "test_modal1_steam_es.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) refix::overlay::g_overlayWarn1Open = true;
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 11: Caution Modal 2 (Steam Overlay - Spanish)
    renderFrame((outDir + "test_modal2_steam_es.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
            refix::overlay::g_overlayHoldTime = 0.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 12: Caution Modal 2 (Disable Overlay - Spanish)
    renderFrame((outDir + "test_modal2_disable_es.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::DisableAll;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
            refix::overlay::g_overlayHoldTime = 0.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    // Frame 13: Caution Modal 2 mid-animation (Holding at 2.5s - Spanish)
    renderFrame((outDir + "test_modal2_holding_es.bmp").c_str(), [&](int frame) {
        app.settingsOpen_ = false;
        refix::overlay::g_pendingOverlayAction = refix::overlay::OverlayAction::Steam;
        if (frame == 0) {
            refix::overlay::g_overlayWarn2Open = true;
        }
        if (frame > 0) {
            refix::overlay::g_overlayHoldTime = 2.5f + (1.0f / 60.0f) * 8.0f;
        }
        app.DrawOverlayCautionModals(ImVec2((float)width, (float)height));
    });

    printf("[SUCCESS] All UI test frames rendered successfully!\n");
    return 0;
}
