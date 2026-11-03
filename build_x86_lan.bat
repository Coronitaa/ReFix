@echo off
setlocal enabledelayedexpansion
title ReFix LAN - x86 steam_api.dll Build Script

cd /d "%~dp0"
if not exist build_x86_lan mkdir build_x86_lan

echo [*] Initializing MSVC x86 Build Environment...

:: Skip auto-detect if an x86 environment is already active
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" (
    where cl >nul 2>&1
    if !ERRORLEVEL! equ 0 goto COMPILE
)

set "VCVARS="
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat"

if defined VCVARS (
    echo [*] Found VC environment: "!VCVARS!"
    call "!VCVARS!" >nul 2>&1
) else (
    echo [WARNING] vcvars32.bat not found automatically. Attempting to build using current PATH...
)

:COMPILE
set "LAN_CORE_SRC=src\lan_core\refix_lan_firewall.cpp src\lan_core\refix_lan_transport.cpp src\lan_core\refix_lan_core.cpp"
set "UNREAL_SRC=src\unreal_detect.cpp src\unreal_steam_emu.cpp src\steam_gc_emu.cpp src\network\fault_injector.cpp src\network\message_tracker.cpp !LAN_CORE_SRC! src\generated\iface_registry.cpp src\generated\iface_gameserver.cpp src\generated\iface_client.cpp src\generated\iface_user.cpp src\generated\iface_friends.cpp src\generated\iface_utils.cpp src\generated\iface_matchmaking.cpp src\generated\iface_networking.cpp src\generated\iface_userstats.cpp src\generated\iface_apps.cpp src\generated\iface_remotestorage.cpp src\generated\iface_ugc.cpp src\generated\iface_http.cpp src\generated\iface_screenshots.cpp src\generated\iface_controller.cpp src\generated\flat_dispatch.cpp"
set "OVERLAY_SRC=src\overlay\refix_overlay.cpp src\overlay\overlay_app.cpp src\overlay\overlay_i18n.cpp src\overlay\overlay_ui_main.cpp src\overlay\overlay_ui_profile.cpp src\overlay\overlay_ui_web.cpp src\overlay\overlay_web.cpp src\overlay\overlay_host.cpp src\overlay\overlay_theme.cpp src\overlay\presence_net.cpp src\overlay\profile_store.cpp src\overlay\avatar_image.cpp src\overlay\save_swap.cpp src\overlay\steam_overlay_block.cpp src\imgui\imgui.cpp src\imgui\imgui_draw.cpp src\imgui\imgui_widgets.cpp src\imgui\imgui_tables.cpp src\imgui\imgui_impl_win32.cpp src\imgui\imgui_impl_dx11.cpp"
:: Overlay DLLs are delay-loaded: the import table the game loader resolves gains nothing new.
set "OVERLAY_LIBS=d3d11.lib dcomp.lib d3dcompiler.lib shell32.lib ole32.lib dwmapi.lib gdi32.lib imm32.lib src\webview2\x86\WebView2LoaderStatic.lib delayimp.lib"
set "OVERLAY_DELAYLOAD=/DELAYLOAD:d3d11.dll /DELAYLOAD:dcomp.dll /DELAYLOAD:d3dcompiler_47.dll /DELAYLOAD:shell32.dll /DELAYLOAD:ole32.dll /DELAYLOAD:dwmapi.dll /DELAYLOAD:gdi32.dll /DELAYLOAD:imm32.dll"

echo [*] Compiling x86 stub...
cl /nologo /c /O2 /Fobuild_x86_lan\refix_x86_stubs.obj src\refix_x86_stubs.c
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling refix_x86_stubs.c & exit /b 1 )

echo [*] Building x86 steam_api.dll (LAN edition, no asm forwarding)...
cl /nologo /O2 /Zi /EHsc /std:c++17 /utf-8 /DREFIX_LAN_EDITION=1 /LD /Fobuild_x86_lan\ /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam src\steam_proxy.cpp src\refix_module_dedupe.cpp src\refix_multi_instance.cpp src\refix_crash_log.cpp src\steam_sockets_flat.cpp !UNREAL_SRC! !OVERLAY_SRC! src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_fusion.cpp src\photon\pn_shared.cpp src\photon\pn_crypto.cpp src\photon\pn_dns.cpp src\photon\pn_p2p.cpp src\photon\pn_lan_backend.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\network\network_mode.cpp src\providers\provider_factory.cpp src\steam_network_compat.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde32.c build_x86_lan\refix_x86_stubs.obj /Febuild_x86_lan\steam_api.dll /Fdbuild_x86_lan\steam_api.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib !OVERLAY_LIBS! /link /MACHINE:X86 /DEBUG /MAP:build_x86_lan\steam_api.map !OVERLAY_DELAYLOAD!
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling x86 steam_api.dll & exit /b 1 )

if not exist bin\lan\x86 mkdir bin\lan\x86
copy /Y build_x86_lan\steam_api.dll bin\lan\x86\steam_api.dll >nul
if %ERRORLEVEL% neq 0 ( echo [!] Error copying to bin\lan\x86 & exit /b 1 )

echo [OK] x86 LAN build complete: bin\lan\x86\steam_api.dll
exit /b 0
