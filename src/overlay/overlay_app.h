// State and screens of the ReFix LAN overlay. Everything here runs on the host thread except Post()
// and the dialog wait, which the game threads use.
#pragma once
#include "overlay_host.h"
#include "overlay_theme.h"
#include "presence_net.h"
#include "profile_store.h"

#include <windows.h>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace refix::overlay {

constexpr ULONGLONG kToastFadeMs = 260;
constexpr float kToastWidth = 360.0f;
constexpr float kToastGap = 10.0f;
inline float ToastHeight(bool invite) { return ui::S(invite ? 128.0f : 84.0f); }

// Restores the identity variables the emulator exported (refix_overlay.cpp), for the profile restart.
void RestoreLaunchEnvironment();

enum class DialogKind { None, FirstRun, Picker };

struct EditorState {
    bool open = false;
    bool creating = true;
    char name[kMaxNameChars * 4 + 1] = {};
    int stock = 0;
    bool custom = false;
    std::wstring imagePath;          // converted PNG to use as the avatar
    int imageVersion = 0;            // bumps the preview texture
    uint64_t steamId = 0;            // imported from Steam, 0 = generate
    bool offerNative = false;        // first profile on this PC: may keep the existing saves
    bool native = false;
    std::string error;
    std::string info;
    std::wstring savedId;            // profile written by the last successful save
};

struct Toast {
    enum class Kind { Welcome, Invite, Info } kind = Kind::Info;
    std::string title;
    std::string body;
    net::Invite invite;
    ULONGLONG born = 0;
    ULONGLONG life = 0;
    bool closing = false;
    ULONGLONG closingAt = 0;
};

struct PlayerRow {
    net::RemotePlayer p;
    bool favorite = false;
    bool sameGame = false;
};

class App : public host::Client {
public:
    static App& Get();

    // ---- any thread ----
    void Post(std::function<void()> fn);
    // Blocks the calling (game) thread on the profile window. Returns the chosen profile, or none for guest.
    std::optional<Profile> RunDialog(DialogKind kind);
    void Begin(uint32_t appId, std::optional<Profile> active);   // after SteamAPI init
    bool Started() const { return started_; }

    // ---- host::Client ----
    host::Mode DesiredMode() override;
    ImVec2 ToastSize() override;
    ImVec2 DialogSize() override;
    void Frame(const host::GameWindow& game, ImVec2 viewport) override;
    void OnHotkey() override;
    void OnModeChanged(host::Mode from, host::Mode to) override;
    void OnCloseRequested() override;

    // ---- host thread: actions used by the screens ----
    void OpenOverlay(bool inviteOnly = false, uint64_t inviteLobby = 0, const std::string& connect = {});
    void CloseOverlay();
    void Join(const net::RemotePlayer& p);
    void Invite(const net::RemotePlayer& p);
    void ToggleFavorite(const net::RemotePlayer& p);
    void OpenUrl(const std::wstring& url);
    void RestartWithProfile(const std::wstring& id);
    void DeferProfile(const std::wstring& id);
    void AcceptInvite(const net::Invite& inv);
    bool DeleteProfile(const std::wstring& id);   // false when it is in use somewhere
    void ShowInfo(const std::string& title, const std::string& body);

    // Data for the screens.
    uint32_t appId_ = 0;
    std::optional<Profile> active_;
    std::vector<Profile> profiles_;
    std::vector<PlayerRow> rows_;
    std::map<uint64_t, ULONGLONG> invitedAt_;
    uint64_t localLobby_ = 0;
    std::string localConnect_;
    std::wstring gameTitle_;
    uint64_t highlight_ = 0;
    bool inviteOnly_ = false;
    uint64_t inviteLobby_ = 0;
    std::string inviteConnect_;
    EditorState editor_;
    std::wstring pendingSwitch_;     // profile id waiting for the restart confirmation
    std::wstring pendingDelete_;     // profile id waiting for the delete confirmation
    bool deleteUnderstood_ = false;  // "I understand" box of that confirmation
    char filter_[64] = {};
    ImVec2 chipMin_{}, chipMax_{};
    bool popupOpenLastFrame_ = false;   // profile chip in the top bar (anchors its menu)

    ui::AvatarRef ProfileAvatar(const Profile& p);
    ui::AvatarRef PlayerAvatar(const net::RemotePlayer& p);
    ui::AvatarRef EditorAvatar();
    std::string LocalName() const;
    const net::RemotePlayer* FindPlayer(uint64_t steamId) const;

private:
    App() = default;
    void DrainPosts();
    void Refresh(const host::GameWindow& game);
    void PublishPresence();
    void PollInvites();
    void PostOverlayActivated(bool active);
    void FinishDialog(std::optional<Profile> result);

    // Screens (overlay_ui_main.cpp / overlay_ui_profile.cpp).
    void DrawOverlay(ImVec2 viewport);
    void DrawTopBar(ImVec2 viewport);
    void DrawCommunity(ImVec2 pos, float width);
    void DrawPlayers(ImVec2 pos, ImVec2 size, bool pickerStyle);
    void DrawPlayerRow(const PlayerRow& row, float width, bool pickerStyle);
    void DrawInvitePicker(ImVec2 viewport);
    void DrawToasts(ImVec2 viewport, bool ownWindow);
    void DrawProfileMenu();
    void DrawSwitchConfirm();
    void DrawDeleteConfirm();
    void DrawLanguageSwitch(ImVec2 rightEdgeCenter);
    void DrawEditorModal();
    bool DrawEditorBody(bool firstRun);   // true when the editor finished (saved or cancelled)
    void DrawDialog(ImVec2 viewport);
    void DrawPicker();
    void BeginEditor(bool creating);
    bool SaveEditor();
    void ImportImage();
    void ImportFromSteam();

    std::mutex postMu_;
    std::vector<std::function<void()>> posts_;
    bool started_ = false;
    bool overlayOpen_ = false;
    DialogKind dialog_ = DialogKind::None;
    bool dialogEditing_ = false;
    HANDLE dialogDone_ = nullptr;
    std::optional<Profile> dialogResult_;
    std::deque<Toast> toasts_;
    bool welcomed_ = false;
    ULONGLONG lastRefresh_ = 0;
    uint32_t avatarKey_ = 0;
    std::wstring avatarKeyFor_;
    int profileVersion_ = 0;
};

} // namespace refix::overlay
