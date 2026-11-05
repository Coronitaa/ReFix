@echo off
setlocal enabledelayedexpansion
title ReFix LAN - Proxy Build Script

if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0"

echo [*] Initializing MSVC x64 Build Environment...

:: Check if cl.exe and ml64.exe are already initialized in PATH
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    where ml64 >nul 2>&1
    if %ERRORLEVEL% equ 0 goto COMPILE
)

:: Auto-detect vcvars64.bat across standard Visual Studio installations
set "VCVARS="
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if defined VCVARS (
    echo [*] Found VC environment: "!VCVARS!"
    call "!VCVARS!" >nul 2>&1
) else (
    echo [WARNING] vcvars64.bat not found automatically. Attempting to build using current PATH...
)

:COMPILE
echo [*] Assembling forwarding tables with MASM x64...
ml64 /c /nologo /Fobuild\winmm_fwd.obj src\winmm_fwd.asm
if %ERRORLEVEL% neq 0 ( echo [!] Error assembling winmm_fwd.asm & exit /b 1 )

ml64 /c /nologo /Fobuild\eos_fwd.obj src\eos_fwd.asm
if %ERRORLEVEL% neq 0 ( echo [!] Error assembling eos_fwd.asm & exit /b 1 )

ml64 /c /nologo /Fobuild\steam_fwd.obj src\steam_fwd.asm
if %ERRORLEVEL% neq 0 ( echo [!] Error assembling steam_fwd.asm & exit /b 1 )

echo [*] Building winmm.dll proxy...
cl /nologo /O2 /EHsc /LD /Fobuild\ src\winmm_proxy.cpp src\server_browser_gui.cpp build\winmm_fwd.obj /Febuild\winmm.dll user32.lib kernel32.lib advapi32.lib comctl32.lib /link /DEF:src\winmm_proxy.def
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling winmm.dll & exit /b 1 )

if "%ENABLE_EOS_LEGACY%"=="1" (
    echo [*] Building legacy EOSSDK-Win64-Shipping.dll proxy...
    cl /nologo /O2 /EHsc /LD /Fobuild\ src\eos_proxy.cpp src\upnp_firewall.cpp build\eos_fwd.obj /Febuild\EOSSDK-Win64-Shipping.dll user32.lib kernel32.lib advapi32.lib ws2_32.lib wininet.lib ole32.lib oleaut32.lib /link /DEF:src\eos_proxy.def
) else (
    echo [*] Building EOSSDK-Win64-Shipping.dll and RedboneEOS.dll [ReFix EOS v3 emulator]...
    cl /nologo /O2 /Zi /EHsc /std:c++17 /DEOS_MONOLITHIC=1 /Fobuild\ /Isrc /Isrc\lan_core /Isrc\eossdk /Isrc\include /Isrc\include\steam src\eossdk\eos_module.cpp src\eossdk\core\refix_common.cpp src\eossdk\core\refix_config.cpp src\eossdk\core\refix_log.cpp src\eossdk\core\refix_hash.cpp src\eossdk\core\eos_ids.cpp src\eossdk\core\eos_identity.cpp src\eossdk\core\eos_dispatch.cpp src\eossdk\core\lobby_model.cpp src\eossdk\core\eos_online.cpp src\eossdk\net\refix_transport.cpp src\eossdk\net\lobby_directory.cpp src\eossdk\net\steam_backend.cpp src\eossdk\api\eos_api_platform.cpp src\eossdk\api\eos_api_connect.cpp src\eossdk\api\eos_api_user.cpp src\eossdk\api\eos_api_lobby.cpp src\eossdk\api\eos_api_sessions.cpp src\eossdk\api\eos_api_p2p.cpp src\eossdk\api\eos_api_storage.cpp src\eossdk\api\eos_api_passthrough.cpp src\lan_core\refix_lan_firewall.cpp /LD build\eos_fwd.obj /Febuild\EOSSDK-Win64-Shipping.dll /Fdbuild\eos.pdb user32.lib kernel32.lib advapi32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib version.lib /link /DEF:src\eos_proxy.def /DEBUG /MAP:build\eos.map
)
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling EOSSDK-Win64-Shipping.dll & exit /b 1 )
copy /Y build\EOSSDK-Win64-Shipping.dll build\RedboneEOS.dll >nul

set "LAN_CORE_SRC=src\lan_core\refix_lan_firewall.cpp src\lan_core\refix_lan_transport.cpp src\lan_core\refix_lan_core.cpp"
set "IL2CPP_SRC=src\refix_il2cpp.cpp src\refix_il2cpp_scene.cpp src\refix_pa_offline.cpp src\refix_il2cpp_logfwd.cpp src\refix_pa_backend.cpp src\refix_pa_services.cpp src\refix_pa_gql.cpp src\refix_pa_rooms.cpp src\refix_pa_relay.cpp"
set "UNREAL_SRC=src\unreal_detect.cpp src\unreal_steam_emu.cpp src\refix_mono.cpp src\steam_gc_emu.cpp src\network\fault_injector.cpp src\network\message_tracker.cpp !LAN_CORE_SRC! src\generated\iface_registry.cpp src\generated\iface_gameserver.cpp src\generated\iface_client.cpp src\generated\iface_user.cpp src\generated\iface_friends.cpp src\generated\iface_utils.cpp src\generated\iface_matchmaking.cpp src\generated\iface_networking.cpp src\generated\iface_userstats.cpp src\generated\iface_apps.cpp src\generated\iface_remotestorage.cpp src\generated\iface_ugc.cpp src\generated\iface_http.cpp src\generated\iface_screenshots.cpp src\generated\iface_controller.cpp src\generated\flat_dispatch.cpp"
set "OVERLAY_SRC=src\overlay\refix_overlay.cpp src\overlay\overlay_app.cpp src\overlay\overlay_i18n.cpp src\overlay\overlay_ui_main.cpp src\overlay\overlay_ui_profile.cpp src\overlay\overlay_ui_web.cpp src\overlay\overlay_ui_import.cpp src\overlay\overlay_ui_mods.cpp src\overlay\overlay_web.cpp src\overlay\overlay_gamepad.cpp src\overlay\overlay_sound.cpp src\overlay\overlay_host.cpp src\overlay\overlay_theme.cpp src\overlay\presence_net.cpp src\overlay\profile_store.cpp src\overlay\avatar_image.cpp src\overlay\save_swap.cpp src\overlay\steam_overlay_block.cpp src\overlay\nvidia_reflex_block.cpp src\imgui\imgui.cpp src\imgui\imgui_draw.cpp src\imgui\imgui_widgets.cpp src\imgui\imgui_tables.cpp src\imgui\imgui_impl_win32.cpp src\imgui\imgui_impl_dx11.cpp"
set "MODS_SRC=src\mods\mod_manifest.cpp src\mods\mod_signature.cpp src\mods\mod_package.cpp src\mods\mod_trust_store.cpp src\mods\mod_scheduler.cpp src\mods\sandbox_wasm.cpp src\mods\native_host.cpp src\mods\patch_engine.cpp src\mods\privacy_gate.cpp src\mods\mod_host.cpp src\mods\adapters\engine\engine_adapter.cpp src\mods\adapters\protocol\protocol_adapter.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c src\mods\miniz\miniz.c src\mods\wasm3\m3_bind.c src\mods\wasm3\m3_code.c src\mods\wasm3\m3_compile.c src\mods\wasm3\m3_core.c src\mods\wasm3\m3_env.c src\mods\wasm3\m3_exec.c src\mods\wasm3\m3_function.c src\mods\wasm3\m3_info.c src\mods\wasm3\m3_module.c src\mods\wasm3\m3_parse.c src\mods\wasm3\m3_validate.c src\mods\wasm3\m3_xxh64.c"
:: Overlay DLLs are delay-loaded: the import table the game loader resolves gains nothing new.
set "OVERLAY_LIBS=d3d11.lib dcomp.lib d3dcompiler.lib shell32.lib ole32.lib dwmapi.lib gdi32.lib imm32.lib hid.lib setupapi.lib winmm.lib crypt32.lib src\webview2\x64\WebView2LoaderStatic.lib delayimp.lib"
set "OVERLAY_DELAYLOAD=/DELAYLOAD:d3d11.dll /DELAYLOAD:dcomp.dll /DELAYLOAD:d3dcompiler_47.dll /DELAYLOAD:shell32.dll /DELAYLOAD:ole32.dll /DELAYLOAD:dwmapi.dll /DELAYLOAD:gdi32.dll /DELAYLOAD:imm32.dll /DELAYLOAD:hid.dll /DELAYLOAD:setupapi.dll /DELAYLOAD:winmm.dll"

echo [*] Building steam_api64.dll proxy (production)...
cl /nologo /O2 /Zi /EHsc /std:c++17 /utf-8 /Zc:preprocessor /DREFIX_LAN_EDITION=1 /LD /Fobuild\ /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Isrc\mods /Isrc\mods\crypto /Isrc\mods\miniz /Isrc\mods\wasm3 src\steam_proxy.cpp src\refix_module_dedupe.cpp src\refix_multi_instance.cpp src\refix_crash_log.cpp src\steam_sockets_flat.cpp !UNREAL_SRC! !OVERLAY_SRC! !MODS_SRC! !IL2CPP_SRC! src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_fusion.cpp src\photon\pn_shared.cpp src\photon\pn_crypto.cpp src\photon\pn_dns.cpp src\photon\pn_p2p.cpp src\photon\pn_lan_backend.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\network\network_mode.cpp src\providers\provider_factory.cpp src\steam_network_compat.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64.dll /Fdbuild\steam_api64.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib !OVERLAY_LIBS! /link /DEF:src\steam_api64.def /DEBUG /MAP:build\steam_api64.map !OVERLAY_DELAYLOAD!
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling steam_api64.dll & exit /b 1 )

if not exist build\test_obj mkdir build\test_obj
echo [*] Building steam_api64_test.dll proxy (test variant with test hooks)...
cl /nologo /O2 /Zi /EHsc /std:c++17 /utf-8 /Zc:preprocessor /DREFIX_BUILD_TESTS=1 /DREFIX_LAN_EDITION=1 /LD /Fobuild\test_obj\ /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Isrc\mods /Isrc\mods\crypto /Isrc\mods\miniz /Isrc\mods\wasm3 src\steam_proxy.cpp src\refix_module_dedupe.cpp src\refix_multi_instance.cpp src\refix_crash_log.cpp src\steam_sockets_flat.cpp !UNREAL_SRC! !OVERLAY_SRC! !MODS_SRC! !IL2CPP_SRC! src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_fusion.cpp src\photon\pn_shared.cpp src\photon\pn_crypto.cpp src\photon\pn_dns.cpp src\photon\pn_p2p.cpp src\photon\pn_lan_backend.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\network\network_mode.cpp src\providers\provider_factory.cpp src\steam_network_compat.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64_test.dll /Fdbuild\steam_api64_test.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib !OVERLAY_LIBS! /link /DEF:src\steam_api64.def /DEBUG /MAP:build\steam_api64_test.map !OVERLAY_DELAYLOAD!
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling steam_api64_test.dll & exit /b 1 )

echo.
echo ====================================================================
echo [OK] All ReFix DLL proxies successfully built in build\
echo   - build\winmm.dll
echo   - build\EOSSDK-Win64-Shipping.dll
echo   - build\RedboneEOS.dll
echo   - build\steam_api64.dll (Production - Clean EAT)
echo   - build\steam_api64_test.dll (Testing - Instrumented EAT)
echo ====================================================================

:: Automatically synchronize freshly built binaries to bin/
if exist bin\ (
    if not exist bin\lan mkdir bin\lan
    copy /Y build\EOSSDK-Win64-Shipping.dll bin\lan\EOSSDK-Win64-Shipping.dll >nul
    copy /Y build\RedboneEOS.dll bin\lan\RedboneEOS.dll >nul
    copy /Y build\steam_api64.dll bin\lan\steam_api64.dll >nul
    copy /Y build\steam_api64.dll bin\steam_api64.dll >nul
    copy /Y build\steam_api64_test.dll bin\steam_api64_test.dll >nul
    echo [OK] Synchronized ReFix LAN binaries to bin\lan\ ^(ReFix Online: run build_online.bat^)
)

:: 32-bit ReFix LAN (bin\lan\x86\steam_api.dll) in a fresh cmd so vcvars32 does not mix with the x64 env
echo [*] Building ReFix LAN x86 (32-bit games)...
cmd /c ""%~dp0build_x86_lan.bat" <nul"
if %ERRORLEVEL% neq 0 ( echo [!] Error building ReFix LAN x86 & exit /b 1 )

echo [*] Building refix_net_test.exe (Phase 3.6 loopback tester)...
cl /nologo /O2 /EHsc /std:c++17 /Fobuild\ src\network\refix_net_test.cpp /Febin\refix_net_test.exe
if %ERRORLEVEL% neq 0 ( echo [!] Error building refix_net_test.exe & exit /b 1 )
echo [OK] refix_net_test.exe built and placed in bin\

echo [*] Building test_refix_lan_core.exe (Universal LAN Core and Transport tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Fobuild\ tests\test_refix_lan_core.cpp src\lan_core\refix_lan_firewall.cpp src\lan_core\refix_lan_transport.cpp src\lan_core\refix_lan_core.cpp /Febin\test_refix_lan_core.exe ws2_32.lib advapi32.lib iphlpapi.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_refix_lan_core.exe & exit /b 1 )
echo [OK] test_refix_lan_core.exe built and placed in bin\

echo [*] Building test_steam_lancore_adapter.exe (Steamworks to LanCore Adapter tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Fobuild\ tests\test_steam_lancore_adapter.cpp /Febin\test_steam_lancore_adapter.exe user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_steam_lancore_adapter.exe & exit /b 1 )
echo [OK] test_steam_lancore_adapter.exe built and placed in bin\

echo [*] Building test_transport_ownership.exe (H-01 Transport Ownership regression tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Fobuild\ tests\test_transport_ownership.cpp /Febin\test_transport_ownership.exe ws2_32.lib user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_transport_ownership.exe & exit /b 1 )
echo [OK] test_transport_ownership.exe built and placed in bin\

echo [*] Building test_steam_sockets_lancore.exe (ISteamNetworkingSockets LanCore tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Fobuild\ tests\test_steam_sockets_lancore.cpp /Febin\test_steam_sockets_lancore.exe ws2_32.lib user32.lib advapi32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_steam_sockets_lancore.exe & exit /b 1 )
echo [OK] test_steam_sockets_lancore.exe built and placed in bin\

echo [*] Building test_lan_concurrency_contract.exe (Dedicated Concurrency Contract and Mesh tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam /Isrc\eossdk /Fobuild\ tests\test_lan_concurrency_contract.cpp src\lan_core\refix_lan_firewall.cpp src\lan_core\refix_lan_transport.cpp src\lan_core\refix_lan_core.cpp src\eossdk\core\lobby_model.cpp /Febin\test_lan_concurrency_contract.exe ws2_32.lib advapi32.lib iphlpapi.lib user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_lan_concurrency_contract.exe & exit /b 1 )
echo [OK] test_lan_concurrency_contract.exe built and placed in bin\

echo [*] Building test_multi_instance.exe (singleton mutex release tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Fobuild\ tests\test_multi_instance.cpp /Febin\test_multi_instance.exe user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_multi_instance.exe & exit /b 1 )
echo [OK] test_multi_instance.exe built and placed in bin\

echo [*] Building test_module_dedupe.exe (one-emulator-per-process regression tester)...
cl /nologo /O2 /EHsc /std:c++17 /Fobuild\ tests\test_module_dedupe.cpp /Febin\test_module_dedupe.exe
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_module_dedupe.exe & exit /b 1 )
echo [OK] test_module_dedupe.exe built and placed in bin\

echo [*] Building test_iface_versions.exe (versioned interface adapter tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Isrc\include /Isrc\include\steam /Fobuild\ tests\test_iface_versions.cpp /Febin\test_iface_versions.exe user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_iface_versions.exe & exit /b 1 )
echo [OK] test_iface_versions.exe built and placed in bin\

echo [*] Building test_identity_probe.exe (IdentityMode auto/fixed probe)...
cl /nologo /O2 /EHsc /std:c++17 /Fobuild\ tests\test_identity_probe.cpp /Febin\test_identity_probe.exe
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_identity_probe.exe & exit /b 1 )
echo [OK] test_identity_probe.exe built and placed in bin\


echo [*] Building test_gamepad.exe (controller reports and overlay toggle)...
cl /nologo /O2 /EHsc /std:c++17 /utf-8 /Fobuild\ tests\test_gamepad.cpp src\overlay\overlay_gamepad.cpp /Febin\test_gamepad.exe hid.lib setupapi.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_gamepad.exe & exit /b 1 )
echo [OK] test_gamepad.exe built and placed in bin\

echo [*] Building test_lan_profiles.exe (LAN profiles per-game isolation and savedata tester)...
cl /nologo /O2 /EHsc /std:c++17 /utf-8 /Isrc /Fobuild\ tests\test_lan_profiles.cpp src\overlay\profile_store.cpp src\overlay\overlay_i18n.cpp /Febin\test_lan_profiles.exe ole32.lib shell32.lib advapi32.lib user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_lan_profiles.exe & exit /b 1 )
echo [OK] test_lan_profiles.exe built and placed in bin\

echo [*] Building test_nvidia_reflex_block.exe (NVIDIA Reflex Overlay suppression tester)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc /Fobuild\ tests\test_nvidia_reflex_block.cpp src\overlay\nvidia_reflex_block.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c /Febin\test_nvidia_reflex_block.exe user32.lib advapi32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Error building test_nvidia_reflex_block.exe & exit /b 1 )

echo [*] Building refixmod.exe (ReFix Mods CLI tool)...
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Isrc\mods\miniz /Fobuild\ tools\refixmod\main.cpp src\mods\mod_package.cpp src\mods\mod_manifest.cpp src\mods\mod_signature.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c src\mods\miniz\miniz.c Advapi32.lib /Febin\refixmod.exe
if %ERRORLEVEL% neq 0 ( echo [!] Error building refixmod.exe & exit /b 1 )
echo [OK] refixmod.exe built and placed in bin\

echo [*] Building ReFix Mods test suite...
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Fobuild\ tests\test_mod_manifest.cpp src\mods\mod_manifest.cpp /Febin\test_mod_manifest.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Fobuild\ tests\test_mod_signature.cpp src\mods\mod_signature.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c Advapi32.lib /Febin\test_mod_signature.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Isrc\mods\miniz /Fobuild\ tests\test_mod_package.cpp src\mods\mod_package.cpp src\mods\mod_manifest.cpp src\mods\mod_signature.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c src\mods\miniz\miniz.c Advapi32.lib /Febin\test_mod_package.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Fobuild\ tests\test_mod_trust_store.cpp src\mods\mod_trust_store.cpp src\mods\mod_signature.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c Advapi32.lib Crypt32.lib Shell32.lib /Febin\test_mod_trust_store.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Fobuild\ tests\test_mod_scheduler.cpp src\mods\mod_scheduler.cpp /Febin\test_mod_scheduler.exe
cl /nologo /O2 /EHsc /std:c++17 /Zc:preprocessor /Isrc\mods /Isrc\mods\wasm3 /Fobuild\ tests\test_sandbox_wasm.cpp src\mods\sandbox_wasm.cpp src\mods\wasm3\*.c /Febin\test_sandbox_wasm.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\minhook /Fobuild\ tests\test_patch_engine.cpp src\mods\patch_engine.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c /Febin\test_patch_engine.exe
cl /nologo /O2 /EHsc /std:c++17 /Isrc\mods /Isrc\mods\crypto /Fobuild\ tests\test_privacy_gate.cpp src\mods\privacy_gate.cpp src\mods\mod_signature.cpp src\mods\crypto\monocypher.c src\mods\crypto\monocypher-ed25519.c Advapi32.lib /Febin\test_privacy_gate.exe
cl /nologo /O2 /EHsc /std:c++17 /Fobuild\ tests\test_refixmod_cli.cpp /Febin\test_refixmod_cli.exe
echo [OK] All ReFix Mods unit tests built in bin\
echo [OK] test_nvidia_reflex_block.exe built and placed in bin\
