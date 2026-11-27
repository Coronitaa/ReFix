# ReFix v2.6.5

## What's New Since v2.6.4

ReFix v2.6.5 adds generic deployment and cross-store multiplayer support for **Epic Games Store** and **Microsoft Store / Xbox PC** titles on Windows and Linux, expands DLC unlocking and in-game purchase emulation across both stores, and resolves Microsoft Store relaunch lockups (`0x800705b4`) and overlay focus issues.

---

### Epic Games Store & Microsoft Store Support
- **Automatic Store Detection & Deployment (`AutoDeploy.bat`, `AutoDeploy.sh`, `detect_game.ps1`, `deploy_helper.ps1`):** Added detection for Epic Games manifests (`.egstore/*.item` and ProgramData manifests) and Microsoft Store / GDK layouts (`MicrosoftGame.Config`, `appxmanifest.xml`), resolving Steam AppIDs via Store search when needed and skipping wrapper executables (`gamelaunchhelper.exe`, `EOSBootstrapper.exe`, crash handlers).
- **Proxy Bootstrapper for Non-Steam Binaries (`winhttp.dll`, `winmm.dll`):** Added `[Paths] AutoLoadSteamApi = true` and `*_Data\Plugins` resolution so games without a native `steam_api*.dll` import automatically load ReFix, start the LAN/SteamLink stack and overlay off the loader lock, and isolate cross-platform players seamlessly.
- **Uninstaller Parity (`Uninstall_ReFix.bat`, `AutoDeploy.sh`):** Updated Windows and Linux uninstallers to clean up ReFix-injected `steam_api*.dll` / `EOSSDK-Win64-Shipping.dll` files on stores that had no original Steam DLL and guarded `.refix_installed` cleanup strictly to `refix_mods`.
- **Batch Parsing Fix:** Quoted `%GAME_NAME%` in `AutoDeploy.bat` summary output so titles containing `&` (such as `Bread&Fred`) deploy cleanly without `cmd.exe` command-chaining errors.

### Microsoft Store Relaunch Fix & Overlay Stability
- **Automatic `0x800705b4` Recovery (`winhttp.dll`):** Added `MsStoreLaunchWatchdogThread` inside `gamelaunchhelper.exe` using Windows UI Automation (`IUIAutomation`) to detect the Microsoft Store `"already running (0x800705b4)"` XAML error dialog, clear lingering `refix_steamhost` / `GamingServicesUI.exe` processes, and let the game launch immediately on back-to-back restarts.
- **Helper Process Cleanup (`steam_link`):** Detached `refix_steamhost` helpers from MSIX/GamingServices job objects where allowed (`CREATE_BREAKAWAY_FROM_JOB` with fallback) and forcefully terminated tracked helpers on `DLL_PROCESS_DETACH`.
- **Overlay Focus & Xbox Sign-In Handling (`overlay_host.cpp`):** Prevented ReFix from stealing focus back when external windows (such as Xbox Live sign-in prompts) are active, and kept the overlay visible when minimized solely by holding its own focus.

### Cross-Store DLC Unlocker & Purchase Emulator
- **Microsoft Store XStore Hooks (`xstore_hook.cpp`):** Targeted `XGamingRuntimeThunks.dll` / `XGamingRuntime.dll` (verified GDK struct layouts against `XStorePrice` / `XStoreProduct` and registered via `LdrRegisterDllNotification` off the loader lock) to unlock add-ons via `XStoreEnumerateProductsQuery`, `XStoreIsLicenseValid`, `XStoreQueryGameLicenseResult`, `XStoreQueryAddOnLicensesResult`, and license-acquisition checks.
- **Epic Online Services Ecom Interface (`eos_api_ecom.cpp`):** Implemented `EOS_Ecom_QueryOwnership*`, `EOS_Ecom_QueryEntitlements*`, `EOS_Ecom_QueryOffers`, `EOS_Ecom_RedeemEntitlements`, and transaction/entitlement queries with `[EOS] UnlockDLCs` support written automatically by deployment and `DLC_Unlocker.bat`.
- **In-Game Purchase Emulator (`ReFix_PromptPurchase`):** Wired `EOS_Ecom_Checkout` and `XStoreShowPurchaseUIAsync` / `XStoreShowPurchaseUIResult` to the ReFix overlay confirmation modal (`ReFixOverlay::PromptPurchase`), with safe fallbacks when the overlay is disabled.
