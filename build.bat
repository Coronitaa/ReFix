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
set "UNREAL_SRC=src\unreal_detect.cpp src\unreal_steam_emu.cpp src\network\fault_injector.cpp src\network\message_tracker.cpp !LAN_CORE_SRC! src\generated\iface_registry.cpp src\generated\iface_gameserver.cpp src\generated\iface_client.cpp src\generated\iface_user.cpp src\generated\iface_friends.cpp src\generated\iface_utils.cpp src\generated\iface_matchmaking.cpp src\generated\iface_networking.cpp src\generated\iface_userstats.cpp src\generated\iface_apps.cpp src\generated\iface_remotestorage.cpp src\generated\iface_ugc.cpp src\generated\iface_http.cpp src\generated\iface_screenshots.cpp src\generated\flat_dispatch.cpp"

echo [*] Building steam_api64.dll proxy (production)...
cl /nologo /O2 /Zi /EHsc /std:c++17 /DREFIX_LAN_EDITION=1 /LD /Fobuild\ /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam src\steam_proxy.cpp src\refix_module_dedupe.cpp src\refix_multi_instance.cpp src\refix_crash_log.cpp src\steam_sockets_flat.cpp !UNREAL_SRC! src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_fusion.cpp src\photon\pn_shared.cpp src\photon\pn_crypto.cpp src\photon\pn_dns.cpp src\photon\pn_p2p.cpp src\photon\pn_lan_backend.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\network\network_mode.cpp src\providers\provider_factory.cpp src\steam_network_compat.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64.dll /Fdbuild\steam_api64.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib /link /DEF:src\steam_api64.def /DEBUG /MAP:build\steam_api64.map
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling steam_api64.dll & exit /b 1 )

if not exist build\test_obj mkdir build\test_obj
echo [*] Building steam_api64_test.dll proxy (test variant with test hooks)...
cl /nologo /O2 /Zi /EHsc /std:c++17 /DREFIX_BUILD_TESTS=1 /DREFIX_LAN_EDITION=1 /LD /Fobuild\test_obj\ /Isrc /Isrc\lan_core /Isrc\include /Isrc\include\steam src\steam_proxy.cpp src\refix_module_dedupe.cpp src\refix_multi_instance.cpp src\refix_crash_log.cpp src\steam_sockets_flat.cpp !UNREAL_SRC! src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_fusion.cpp src\photon\pn_shared.cpp src\photon\pn_crypto.cpp src\photon\pn_dns.cpp src\photon\pn_p2p.cpp src\photon\pn_lan_backend.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\network\network_mode.cpp src\providers\provider_factory.cpp src\steam_network_compat.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64_test.dll /Fdbuild\steam_api64_test.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib /link /DEF:src\steam_api64.def /DEBUG /MAP:build\steam_api64_test.map
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
