// ReFix LAN overlay: replaces the Steam overlay (Shift+Tab), owns the player profiles and the LAN player list.
//
// Flow:
//   DllMain            -> ArmEarly()          Steam overlay blocker (environment switches only)
//   LoadConfig         -> WaitForRestartParent() + ResolveIdentity()
//                                             picks/creates the profile (may show the profile window) and
//                                             swaps the game's save folders to it (save_swap.h)
//   after emulator init-> Start()             overlay window, Shift+Tab hotkey, LAN presence, toast
//   ISteamFriends      -> Activate*()         the game's overlay requests (invite dialog, store, ...)
//
// Config (ReFix.ini [Overlay]): Enabled (default true), BlockSteamOverlay (true), IsolateSaves (true).
// REFIX_NO_OVERLAY=1 disables everything; REFIX_PROFILE_ID=<id> forces a profile and REFIX_RESTART_PID makes
// the new process wait for the old one (both set by the restart that switches profiles). Console processes
// and -batchmode / -nographics / -server runs never use profiles or the overlay.
#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace ReFixOverlay {

struct Identity {
    uint64_t steamId = 0;
    std::string name;      // UTF-8
};

void ArmEarly();

// Profile restart: waits (up to 30 s) for the process that relaunched the game to exit, so its files and
// instance slot are free. No-op otherwise.
void WaitForRestartParent();

// IdentityMode=auto only. Returns false when no profile applies (console process, UI disabled or the
// player chose "not now"): the caller keeps the legacy machine-derived identity.
bool ResolveIdentity(uint32_t appId, const std::string& iniPath, Identity& out);

// Folder for the active profile's emulated Steam Cloud files, empty for the legacy "saves" folder.
std::wstring CloudFolder(uint32_t appId);
// Folder for the active profile's emulated Steam UserStats (stats.ini).
std::wstring StatsFolder(uint32_t appId);
std::wstring InventoryFolder(uint32_t appId);
std::wstring ActiveProfileId();

void Start();
bool IsEnabled();          // ISteamUtils::IsOverlayEnabled

void ActivateOverlay(const char* dialog);
void ActivateOverlayToUser(const char* dialog, uint64_t steamId);
void ActivateWebPage(const char* url);
void ActivateStore(uint32_t appId);
void OpenMarketGiveMenu();
void OpenMatchmakerModal();
void PromptPurchase(uint32_t appId, uint64_t orderId, const std::string& itemName, const std::string& itemDesc, bool isMicroTxn, std::function<void(bool)> onComplete = nullptr);
void ActivateInviteDialog(uint64_t lobbyId, const char* connect);
// ISteamFriends::InviteUserToGame / ISteamMatchmaking::InviteUserToLobby.
bool InviteUser(uint64_t steamId, uint64_t lobbyId, const char* connect);

// Steam Avatar & Image provider (ISteamFriends::Get*Avatar, ISteamUtils::GetImage*)
int GetAvatarHandle(uint64_t steamId, int size);
bool GetImageSize(int iImage, uint32_t* pnWidth, uint32_t* pnHeight);
bool GetImageRGBA(int iImage, uint8_t* pubDest, int nDestBufferSize);
void InvalidateAvatar(uint64_t steamId);

} // namespace ReFixOverlay

// Implemented by the emulator (unreal_steam_emu.cpp) for the overlay.
namespace UnrealSteamEmu {
struct OverlayLobbyInfo {
    uint64_t lobbyId = 0;
    bool joinable = false;
    uint32_t members = 0;
    uint32_t maxMembers = 0;
};
OverlayLobbyInfo Overlay_LocalLobby();
std::string Overlay_ConnectString();            // rich presence "connect" of the local player
void Overlay_SetPersonaName(const std::string& name);
} // namespace UnrealSteamEmu
