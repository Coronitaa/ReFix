# ReFix

[![Release](https://img.shields.io/badge/version-v2.6.1-blue.svg)](https://github.com/Coronitaa/ReFix/releases/tag/v2.6.1)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20x86%20%7C%20Proton-lightgrey.svg)](https://github.com/Coronitaa/ReFix)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

ReFix is a C++ Steamworks and EOS proxy for Unity, Unreal, and native PC games. It lets you play multiplayer games on a local network or with Steam friends, manages separate player profiles with their own saves and achievements, loads custom mods, and includes a SmokeAPI-based DLC unlocker.

---

## Modes

ReFix runs in three different modes depending on how you want to connect and whether you want to use Steam. You can pick your mode during installation with `AutoDeploy` or change it later in the config.

### 1. ReFix LAN Offline
Runs 100% offline over your local network. Steam does not need to be installed or running.
* **Networking:** Matchmaking and multiplayer traffic go purely through your local network (LAN). Does not require an internet connection.
* **Play over the internet:** To play with people outside your house, connect everyone to the same virtual LAN using **RadminVPN, Hamachi, ZeroTier**, or any similar VPN tool.
* **Features:** Includes the in-game **ReFix Overlay** (`Shift+Tab`), local profile management, achievement tracking with unlock notifications, and full support for **ReFix Mods**.

### 2. ReFix Mixed (LAN + Online)
Combines everything in `ReFix LAN Offline` with optional Steam P2P networking.
* **Networking:** You can switch between pure local LAN (or virtual LAN like RadminVPN/ZeroTier) and Steam's P2P network via **Spacewar (AppID 480)**.
* **Steam Friends & Local Profiles:** When online mode is active through the overlay's Steam toggle, you can mix your local ReFix profiles with your Steam friends list. You can see which friends are playing, invite them directly to your lobby, or join their games.
* **Features:** Uses the **ReFix Overlay** (with an option to load the Steam overlay alongside it), supports achievements, profile cloud syncing, and **ReFix Mods**.

### 3. ReFix Online Legacy
The classic mode, closest to standard *OnlineFix*.
* **Networking:** Completely dependent on the Steam client and requires an active internet connection. All multiplayer traffic routes through Steam's P2P network using **Spacewar (AppID 480)**.
* **Overlay:** Uses only the native **Steam Overlay**. The ReFix Overlay, local profile switcher, and ReFix mod loader are not active in this mode.
* **Updates:** Kept for backwards compatibility with older setups. It does not receive frequent updates.

---

## Comparison

| Feature | ReFix LAN Offline | ReFix Mixed | ReFix Online Legacy |
| :--- | :--- | :--- | :--- |
| **Requires Steam** | No | Optional (Required for Online P2P) | Yes |
| **Requires Internet** | No | Only for Steam P2P | Yes |
| **Network Type** | Pure LAN / Virtual LAN (Radmin, ZeroTier, Hamachi) | LAN + Steam P2P (Spacewar) | Steam P2P (Spacewar) |
| **Overlay** | ReFix Overlay | ReFix Overlay (+ optional Steam Overlay) | Steam Overlay only |
| **Achievements & Profiles** | Yes (ReFix local profiles) | Yes (Local + Steam profile mixing) | Steam default |
| **ReFix Mods Support** | Yes | Yes | No |
| **Maintenance** | Active | Active | Legacy (Rare updates) |

---

## Install

1. Download the latest `ReFix_Release.zip` from [Releases](https://github.com/Coronitaa/ReFix/releases) and extract it anywhere.
2. Run `AutoDeploy.bat` on Windows (or `AutoDeploy.sh` on Linux).
3. Select the game folder and follow the prompts:
   * Choose your mode: **ReFix LAN Offline**, **ReFix Mixed**, or **ReFix Online Legacy**.
   * Choose which overlay to display: ReFix's, Steam's, or none.
   * Choose which DLCs to unlock: all, none, or a specific list fetched from the Steam Store.
4. Start the game. Everyone playing together must use the same game version and the same ReFix version.

### Linux (Proton / Wine)
`AutoDeploy.sh` installs ReFix into Windows games running under Proton or Wine:
```bash
bash AutoDeploy.sh
```
Make sure your launcher (Steam, Heroic, Lutris, Faugus) adds the DLL override for the proxy DLL deployed into the game directory (e.g., `WINEDLLOVERRIDES="winmm=n,b"` or `version=n,b`). If it fails on your distro, open an issue with the terminal output.

---

## In-Game Overlay & Profiles

Press `Shift+Tab` in-game to open the **ReFix Overlay** (available in `LAN Offline` and `Mixed` modes).
* **Players & Invites:** See players on your LAN or connected via Steam, and send or accept game invites.
* **Profiles:** Create and switch between multiple profiles on the same PC. Each profile keeps its own SteamID, save files, stats, inventory items, and achievements.
* **Cloud Sync (Optional):** Local profiles stay on your PC unless you turn on Google Drive sync in `Settings -> Syncing`. ReFix only uses the isolated `drive.appdata` folder in your Google account to back up your saves and profile data.
* **Dual Overlay:** ReFix blocks the default Steam overlay by default to avoid input conflicts. Set `BlockSteamOverlay = false` under `[Overlay]` in the config if you want both loaded.

---

## ReFix Mods & Mod SDK

ReFix includes a built-in mod loader and a C++ header-only **Mod SDK** that works across `ReFix LAN Offline` and `ReFix Mixed`. Mods load directly through ReFix without patching game binaries on disk, and can hook into the ReFix Overlay, read/write game memory, draw custom ImGui windows, and send custom network packets between players in the same lobby.

* **Official Mods Gallery:** Browse and download ready-to-use mods from the [Official ReFix Mods Gallery](https://github.com/Coronitaa/ReFix/tree/main/refix_mods) (or install them directly via [BlueStar](https://github.com/Coronitaa/BlueStar)).
* **Writing a Mod (SDK & Wiki):** The SDK sits in the `sdk/` folder (`refix_mod_sdk.h`). It exposes a clean C ABI for lifecycle callbacks (`OnModLoad`, `OnFrame`, `OnOverlayDraw`, `OnPacketReceived`), overlay UI registration, and P2P messaging.
* **Documentation:** Read the [Mod Development Wiki](https://github.com/Coronitaa/ReFix/wiki) for setup guides, API references, and example mod templates.

To install a `.dll` mod manually, drop it into the `refix_mods/` folder inside your game directory. You can enable, disable, or configure loaded mods from the **Mods** tab inside the ReFix Overlay.

---

## DLC Unlocker

ReFix ships with a standalone DLC unlocker based on SmokeAPI.
* During `AutoDeploy`, you can unlock all DLCs automatically or pick specific ones from the Steam Store.
* You can also run `DLC_Unlocker.bat` separately if you only want to unlock DLCs on a legitimate Steam install without touching multiplayer files.

---

## Known Limits

* **Official Dedicated Servers:** ReFix cannot connect to official studio servers. Ranked matchmaking, server-side inventories, and global leaderboards stay offline.
* **Kernel Anti-Cheat:** Games protected by EasyAntiCheat (EAC), BattlEye, or Vanguard must be launched with their anti-cheat disabled or bypassed for local/P2P play.
* **FromSoftware Games:** Titles like *Dark Souls III*, *Elden Ring*, *Sekiro*, and *Armored Core VI* start in their offline mode unless paired with a dedicated Seamless Co-op mod.

---

## Building from Source

Requirements:
* Windows 10 or 11
* Visual Studio 2022 (Desktop development with C++ workload, MSVC v143, Windows 10/11 SDK)
* CMake 3.24+

```bash
git clone --recursive [https://github.com/Coronitaa/ReFix.git](https://github.com/Coronitaa/ReFix.git)
cd ReFix
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## License

You can copy, modify, and share ReFix for non-commercial use, as long as you give credit and keep the same license. See [LICENSE](LICENSE) for full details.

**ReFix Offline LAN** (AutoDeploy option [2]) is the LAN build of 2.3.2, unchanged: the same local network play, without the Steam online services, profile sync and the rest of what is new in 2.4.0.

### Steam online services

When Steam is installed, the top bar of the overlay has a **Steam** button. It asks before it changes anything and explains what happens.

- Turned on, ReFix connects to Steam as Spacewar (AppID 480), and starts Steam in the background (notification area) if it was closed. A small helper, `refix_steamhost64.exe`, talks to Steam, so turning it off makes Steam stop showing Spacewar at once.
- Your Steam friends show up in a card next to the LAN players, with their Steam pictures and status. Friends playing with ReFix come first (the ones in your game at the top) and say which game they play.
- Friends who run a ReFix game with Steam on become players of your network: lobbies, invites, Photon rooms and the overlay all work with them as on a LAN, carried over Steam P2P. Invite them or join their match from their row, like LAN players. The game itself never talks to Steam.
- One room can mix both: players on your LAN and Steam friends over the Internet. The PC with Steam on joins them, nobody else has to set anything up.
- Every Steam account that signed in on this PC becomes a profile (with a Steam badge). The one signed in is the default while Steam is on.
- Turned off, ReFix goes back to LAN only. The choice is remembered on this PC.
- `ReFix.ini` `[Steam] OnlineServices`: `auto` (default, on when Steam is already open and signed in), `on` or `off`. The switch in the overlay wins.
- The ReFix overlay replaces Steam's. `BlockSteamOverlay = false` in `[Overlay]` loads Steam's overlay as well.

### Profile sync

Settings > **Syncing** keeps each profile's saves, achievements, stats, items and emulated Steam Cloud files in one `.rfxp` file, wherever you choose. Your profiles always stay on the PC too; nothing is sent to ReFix.

- **In the cloud:** sign in to **Google Drive** or **OneDrive** right in the overlay. ReFix only gets its own app folder in your account, never your files, and the sign-in stays on the PC, encrypted for your Windows user. The card shows the account, the live sync state and a Disconnect button. The folder of a cloud desktop app (OneDrive, Google Drive, Dropbox) works too.
- **Local:** a USB drive or any folder, or "Only on this PC".
- **Export or import:** a `.rfxp` file to carry by hand. An imported file always becomes a new profile, so nothing that exists is replaced.
- The round button next to your profile shows the state: a green tick when synced, orange changes waiting, red failed, grey nowhere to sync. Click it to sync now.
- At game start ReFix takes a newer file from another PC. What it replaces is kept in `%APPDATA%\ReFix\SyncBackups` (last 3). If the destination could not be read at start, ReFix never uploads over a newer copy from another PC.
- Steam profiles sync on their own; local profiles only with "Sync local profiles too".
- Privacy policy: <https://coronitaa.github.io/privacy.html>. Building ReFix yourself: the cloud services need client IDs, see [docs/wiki/cloud-sync-setup.md](docs/wiki/cloud-sync-setup.md).

### Overlay

Press **Shift+Tab**, or the Xbox / PS button of a controller.

- See who on the network is playing, join their match or invite them. Invites show up as a notification in the other player's game.
- Steam achievements: a notice with the icon and rarity when you unlock one, your recent ones, and the full list with progress. Saved per profile, in English and Spanish.
- Settings (the gear): Notifications, Sound, Language, Performance and Syncing, each in its own page.
- The top bar shows the game's own icon, the Steam button and the sync button.
- The game's Store, Community, Guides, Workshop and News pages open inside the overlay. They need the WebView2 Runtime (included in Windows 11); without it they open in your browser.
- Works with Xbox and XInput controllers, DualShock 4 and DualSense, in English and Spanish.
- Draws over borderless and fullscreen-window games. Games in exclusive fullscreen minimise while it is open.
- If another module of the game has patched DXGI, the overlay draws through a layered window instead of DirectComposition, so it does not crash the game.
- Each game window has its own overlay: another instance or app in front hides it until you go back.
- Nothing in the game or in Windows is patched or hooked, except NVIDIA: with "Turn off NVIDIA Reflex" on (Settings > Performance, `BlockNvidiaReflex`), Reflex never runs in the game and NVIDIA App's overlay (its Alt+Z notice, stats) stays hidden while the game is in front, so the ReFix overlay does not flicker or turn see-through.

### Profiles

- The first time you start a game you create a profile: a name, a picture and a SteamID.
- Each profile keeps its own saves (Steam Cloud files, plus the save folder of Unity and Unreal games).
- A second copy of the game on the same PC asks "Who's playing?". You can pick another profile, create one, or play as a guest.
- A profile made in one ReFix game can be imported into another. It stays the same player.

## Linux

`AutoDeploy.sh` installs ReFix into a Windows game that runs through Proton or Wine:

```bash
bash AutoDeploy.sh                                        # asks for everything
bash AutoDeploy.sh "/path/to/game" lan                    # ReFix Mixed (LAN + Online)
bash AutoDeploy.sh "/path/to/game" offline                # ReFix Offline LAN (2.3.2)
bash AutoDeploy.sh "/path/to/game" online 480 none        # game folder, mode, AppID, overlay
bash Uninstall_ReFix.sh "/path/to/game"                    # or bash AutoDeploy.sh --uninstall
```

For ReFix Online Legacy, launch the game from Steam with Proton so the Steam client is running. The script has only been tested with bash on Windows so far. If it fails on your distro, open an issue with the output.

## Game mods

Some games get a ReFix mod made for them (the first one is CS:GO 1.37: working inventory, cases, stickers, Operation Shattered Web missions, LAN server browser and rooms). AutoDeploy checks [`refix_mods/catalog.tsv`](refix_mods/catalog.tsv) and, when the game has a mod, installs it into the game's `refix_mods` folder: from the release zip, or downloaded from this repository and checked against its SHA-256. The uninstaller removes it and puts back any game file the mod changed.

A mod only loads in the games listed in its `target_games`, so one game's mod never shows up in another.

Writing a mod: see the [Mod SDK](docs/wiki/Mod-SDK.md) and, for overlay panels, [Mod-SDK-Overlay](docs/wiki/Mod-SDK-Overlay.md).

## DLC unlocker

`DLC_Unlocker.bat` installs SmokeAPI into a game on its own, without the rest of ReFix. Pick the game folder, then unlock all DLCs, a list you choose, or none. AutoDeploy asks the same question during install.

## Known limits

- ReFix LAN can't reach official online services: ranked matchmaking, inventories and leaderboards stay offline. FromSoftware games (Dark Souls III, Elden Ring, Sekiro, Armored Core VI) start in their offline mode.
- ReFix does not bypass anti-cheat. Games with anti-cheat file checks refuse their protected modes.
- Purchases are never emulated.
- Most testing has been done with two copies of a game on one PC. Reports from setups with real separate PCs are welcome.

## Configuration

AutoDeploy writes a `ReFix.ini` next to the game's executable. The settings you are most likely to change:

```ini
[User]
IdentityMode = auto        ; auto: SteamID derived from this PC. fixed: use SteamId below
Name = Player
SteamId =

[Overlay]
Enabled = true             ; ReFix overlay
BlockSteamOverlay = true   ; keep Steam's overlay out of the game
Sounds = true
BlockNvidiaReflex = true   ; NVIDIA Reflex off in the game, NVIDIA App's overlay hidden
IsolateSaves = true        ; each profile keeps its own saves

[Steam]
RealAppId =                ; the game's real AppID
DLCs = all                 ; all | none | comma-separated AppIDs
OnlineServices = auto      ; Mixed: auto | on | off (the overlay's Steam button wins)

[ServerBrowser]
OverrideServerListAppId = true   ; Online Legacy: search the game's servers under its real AppID
ServerListAppId =                ; the real AppID (AutoDeploy fills both)

[Network]
DiscoveryPort = 47584      ; LAN discovery
CustomBroadcasts =         ; extra broadcast IPs for VPNs, comma-separated
InternetAccess = allow     ; Mixed: block keeps the game on the local network

[P2P]
EnableWinsockHooks = true  ; false if a game freezes when joining a lobby (games on Steam networking sockets)

[Debug]
EnableLog = false          ; set to true to get a ReFix.log for a bug report
```

## Building from source

You need Windows 10 or 11 and Visual Studio 2022 with the "Desktop development with C++" workload.

- `build.bat` builds ReFix LAN (64-bit and 32-bit) into `bin\lan`.
- `build_online.bat` builds ReFix Online into `bin\online`.
- `tools\make_release.ps1` assembles the release package.

```
AutoDeploy.bat / AutoDeploy.sh   installers (Windows / Linux)
Uninstall_ReFix.bat              restores the game's original files
DLC_Unlocker.bat                 SmokeAPI installer
bin/online, bin/lan              the two products
bin/*.ps1                        detection, deploy, DLC and firewall helpers
src/                             C++ sources (Steam and EOS proxies, LAN core, overlay, Photon emulator)
```

## Credits

- [Mr_Goldberg](https://gitlab.com/Mr_Goldberg/goldberg_emulator) and [gbe_fork](https://github.com/Detanup01/gbe_fork): ReFix LAN started as a Goldberg-based mode.
- [acidicoala](https://github.com/acidicoala): SmokeAPI.
- [TsudaKageyu](https://github.com/TsudaKageyu/minhook): MinHook.
- [ocornut](https://github.com/ocornut/imgui): Dear ImGui.
- [atom0s](https://github.com/atom0s/Steamless): Steamless.

## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/). You can copy, change and share ReFix for non-commercial use, as long as you credit it and keep the same license. See `LICENSE`.
