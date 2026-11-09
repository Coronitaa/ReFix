# ReFix

[![Release](https://img.shields.io/badge/version-v2.4.0-blue.svg)](https://github.com/Coronitaa/ReFix/releases/tag/v2.4.0)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20x86%20%7C%20Proton-lightgrey.svg)](https://github.com/Coronitaa/ReFix)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

ReFix gets multiplayer working in PC games that normally need Steam or Epic online services. It replaces the game's `steam_api64.dll` (and the EOS DLLs when the game has them) with its own, and backs up the originals so you can undo it.

One download contains three products. You pick one per game:

- **ReFix Online + LAN** plays on your local network (or a VPN like Radmin, Hamachi or ZeroTier) with no Steam client at all. If Steam is installed, one switch in the overlay also lets you play with your Steam friends over the Internet.
- **ReFix Offline LAN** is the LAN version of the previous release (2.3.2), for local network play only.
- **ReFix Online Legacy** is the online version of the previous release (2.3.2): the whole game runs through your Steam client as Spacewar.

ReFix also ships a DLC unlocker based on SmokeAPI.

## Install

1. Download `ReFix_Release_v2.4.0.zip` from [Releases](https://github.com/Coronitaa/ReFix/releases/latest) and extract it anywhere.
2. Run `AutoDeploy.bat` on Windows, or `bash AutoDeploy.sh` on Linux.
3. Pick the game folder, confirm the game's `.exe`, then choose:
   - `[1] ReFix Online + LAN`, `[2] ReFix Offline LAN` or `[3] ReFix Online Legacy via Steam`
   - which overlay the game shows (LAN products only): ReFix's, Steam's, or none
   - which DLCs to unlock: all, none, or a list you pick from the Steam Store
4. Start the game.

Everyone who plays together needs the same ReFix version and the same mode.

To undo everything, run `Uninstall_ReFix.bat` on Windows (or `bash Uninstall_ReFix.sh` on Linux). It restores the game's original DLLs and gives the game this PC's own saves back. ReFix LAN profiles stay in `%APPDATA%\ReFix`.

To update ReFix in a game, run the new `AutoDeploy` on it again.

## ReFix Online Legacy

- Needs the Steam client running. The game runs as Spacewar (AppID 480), so you get Steam lobbies, invites, P2P and Valve's relay.
- Free-to-play games keep their real AppID (AutoDeploy asks the Steam store), so their own servers accept the Steam login.
- If Steam has no install folder for the real AppID, the game gets its own folder instead of an empty path.
- The real AppID goes into the lobby data, so you only see players of the same game.
- Photon games (PUN 2, Realtime, Voice, Fusion) run against a local Photon emulator, and their traffic goes over Steam P2P. No Photon servers or AppIDs are needed.
- Epic Online Services games get a DeviceID login through the bundled EOS proxy.

## ReFix LAN

- No Steam client needed. ReFix LAN replaces the game's Steam and EOS libraries with a local emulator.
- Players on the same network find each other through the game's own lobbies and server lists. Some older Source games (CS:GO) still join by IP: `connect <host IP>:27015` in the console.
- Each PC gets its own SteamID automatically, so a patched game folder can be copied to another PC as is.
- Several copies of the same game can run on one PC, each as a different player.
- 32-bit and 64-bit games, including older Steamworks SDKs (Dark Souls III and other games from around 2016).
- Photon games work over LAN, public room lists included. Unreal Engine lobbies show up in the server list.
- The game can still reach its own Internet services (news, analytics, web APIs). Steam and Photon stay emulated. `InternetAccess = block` in `ReFix.ini` keeps it on the local network only.

### Steam online services

When Steam is installed, the top bar of the overlay has a **Steam** button. It asks before it changes anything and explains what happens.

- Turned on, ReFix connects to Steam as Spacewar (AppID 480), and opens Steam in the background if it was closed.
- Your Steam friends show up in a card next to the LAN players, with their Steam pictures and status. Friends playing with ReFix come first (the ones in your game at the top) and say which game they play.
- Friends who run a ReFix game with Steam on become players of your network: lobbies, invites, Photon rooms and the overlay all work with them as on a LAN, carried over Steam P2P. The game itself never talks to Steam.
- One room can mix both: players on your LAN and Steam friends over the Internet. The PC with Steam on joins them, nobody else has to set anything up.
- Every Steam account that signed in on this PC becomes a profile (with a Steam badge). The one signed in is the default while Steam is on.
- Turned off, ReFix goes back to LAN only. The choice is remembered on this PC.
- `ReFix.ini` `[Steam] OnlineServices`: `auto` (default, on when Steam is already open and signed in), `on` or `off`. The switch in the overlay wins.
- The ReFix overlay replaces Steam's. `BlockSteamOverlay = false` in `[Overlay]` loads Steam's overlay as well.

### Profile sync

Steam profiles keep everything (saves, achievements, stats, items, emulated Steam Cloud files) in one `.rfxp` file in a folder you choose in Settings. Pick the folder of OneDrive, Google Drive or Dropbox and its app uploads it, so your games follow you to any PC with that Steam account; a USB drive works too. ReFix picks a cloud folder by itself when it finds one.

- The round button next to your profile shows the state: green synced, orange changes waiting, red failed, grey nowhere to sync. Click it to sync now; it spins while it works.
- At game start ReFix takes a newer file from another PC. What it replaces is kept in `%APPDATA%\ReFix\SyncBackups` (last 3).
- The file is compressed and scrambled so it is not edited by hand by mistake, and a checksum rejects damaged files. Nothing is sent to ReFix.
- Local profiles stay on the PC unless you turn on "Sync local profiles too".
- Or sign in to Google Drive or OneDrive in Settings and ReFix keeps the file in its own folder of your account, with no desktop app needed. ReFix can only see that folder, and the sign-in stays encrypted for your Windows user. Builds need ReFix's client IDs: see [docs/cloud-sync-setup.md](docs/cloud-sync-setup.md). "Only on this PC" turns syncing off; your profiles always stay on the PC either way.
- Settings also exports your profile to a `.rfxp` file anywhere, and imports one. An imported file always becomes a new profile, so nothing that exists is replaced.
### Overlay

Press **Shift+Tab**, or the Xbox / PS button of a controller.

- See who on the network is playing, join their match or invite them. Invites show up as a notification in the other player's game.
- Steam achievements: a notice with the icon and rarity when you unlock one, your recent ones, and the full list with progress. Saved per profile, in English and Spanish.
- Settings (the gear): turn each kind of notification and sound on or off, change the language, and the NVIDIA Reflex option below.
- The game's Store, Community, Guides, Workshop and News pages open inside the overlay. They need the WebView2 Runtime (included in Windows 11); without it they open in your browser.
- Works with Xbox and XInput controllers, DualShock 4 and DualSense, in English and Spanish.
- Draws over borderless and fullscreen-window games. Games in exclusive fullscreen minimise while it is open.
- If another module of the game has patched DXGI, the overlay draws through a layered window instead of DirectComposition, so it does not crash the game.
- Nothing in the game or in Windows is patched or hooked, except NVIDIA Reflex: it is paused while the overlay is open, so the overlay does not flicker or turn see-through (Settings, or `BlockNvidiaReflex = false`, turns this off).

### Profiles

- The first time you start a game you create a profile: a name, a picture and a SteamID.
- Each profile keeps its own saves (Steam Cloud files, plus the save folder of Unity and Unreal games).
- A second copy of the game on the same PC asks "Who's playing?". You can pick another profile, create one, or play as a guest.
- A profile made in one ReFix game can be imported into another. It stays the same player.

## Linux

`AutoDeploy.sh` installs ReFix into a Windows game that runs through Proton or Wine:

```bash
bash AutoDeploy.sh                                        # asks for everything
bash AutoDeploy.sh "/path/to/game" lan                    # ReFix LAN
bash AutoDeploy.sh "/path/to/game" online 480 none        # game folder, mode, AppID, overlay
bash Uninstall_ReFix.sh "/path/to/game"                    # or bash AutoDeploy.sh --uninstall
```

For ReFix Online, launch the game from Steam with Proton so the Steam client is running. The script has only been tested with bash on Windows so far. If it fails on your distro, open an issue with the output.

## Game mods

Some games get a ReFix mod made for them (the first one is CS:GO 1.37: working inventory, cases, stickers, Operation Shattered Web missions, LAN server browser and rooms). AutoDeploy checks [`refix_mods/catalog.tsv`](refix_mods/catalog.tsv) and, when the game has a mod, installs it into the game's `refix_mods` folder: from the release zip, or downloaded from this repository and checked against its SHA-256. The uninstaller removes it and puts back any game file the mod changed.

Writing a mod: see the [Mod SDK](docs/wiki/Mod-SDK.md).

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
BlockNvidiaReflex = true   ; pause NVIDIA Reflex while the overlay is open
IsolateSaves = true        ; each profile keeps its own saves

[Steam]
RealAppId =                ; the game's real AppID
DLCs = all                 ; all | none | comma-separated AppIDs

[Network]
DiscoveryPort = 47584      ; LAN discovery
CustomBroadcasts =         ; extra broadcast IPs for VPNs, comma-separated

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
tests/                           test suites
```

## Credits

- [Mr_Goldberg](https://gitlab.com/Mr_Goldberg/goldberg_emulator) and [gbe_fork](https://github.com/Detanup01/gbe_fork): ReFix LAN started as a Goldberg-based mode.
- [acidicoala](https://github.com/acidicoala): SmokeAPI.
- [TsudaKageyu](https://github.com/TsudaKageyu/minhook): MinHook.
- [ocornut](https://github.com/ocornut/imgui): Dear ImGui.
- [atom0s](https://github.com/atom0s/Steamless): Steamless.

## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/). You can copy, change and share ReFix for non-commercial use, as long as you credit it and keep the same license. See `LICENSE`.
