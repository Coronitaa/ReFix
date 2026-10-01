@echo off
setlocal enabledelayedexpansion
title ReFix - Proxy Build Script

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
    cl /nologo /O2 /Zi /EHsc /std:c++17 /DEOS_MONOLITHIC=1 /Fobuild\ /Isrc\eossdk /Isrc\include /Isrc\include\steam src\eossdk\eos_module.cpp src\eossdk\core\refix_common.cpp src\eossdk\core\refix_config.cpp src\eossdk\core\refix_log.cpp src\eossdk\core\refix_hash.cpp src\eossdk\core\eos_ids.cpp src\eossdk\core\eos_identity.cpp src\eossdk\core\eos_dispatch.cpp src\eossdk\core\lobby_model.cpp src\eossdk\core\eos_online.cpp src\eossdk\net\refix_transport.cpp src\eossdk\net\lobby_directory.cpp src\eossdk\net\steam_backend.cpp src\eossdk\api\eos_api_platform.cpp src\eossdk\api\eos_api_connect.cpp src\eossdk\api\eos_api_user.cpp src\eossdk\api\eos_api_lobby.cpp src\eossdk\api\eos_api_sessions.cpp src\eossdk\api\eos_api_p2p.cpp src\eossdk\api\eos_api_storage.cpp src\eossdk\api\eos_api_passthrough.cpp /LD build\eos_fwd.obj /Febuild\EOSSDK-Win64-Shipping.dll /Fdbuild\eos.pdb user32.lib kernel32.lib advapi32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib version.lib /link /DEF:src\eos_proxy.def /DEBUG /MAP:build\eos.map
)
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling EOSSDK-Win64-Shipping.dll & exit /b 1 )
copy /Y build\EOSSDK-Win64-Shipping.dll build\RedboneEOS.dll >nul

set "UNREAL_SRC=src\unreal_detect.cpp"
if "%ENABLE_UNREAL_EOS%"=="1" (
    set "UNREAL_SRC=src\unreal_detect.cpp src\unreal_steam_emu.cpp"
)

echo [*] Building steam_api64.dll proxy...
cl /nologo /O2 /Zi /EHsc /std:c++17 /LD /Fobuild\ /Isrc\include /Isrc\include\steam src\steam_proxy.cpp !UNREAL_SRC! src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64.dll /Fdbuild\steam_api64.pdb user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib advapi32.lib /link /DEF:src\steam_api64.def /DEBUG /MAP:build\steam_api64.map
if %ERRORLEVEL% neq 0 ( echo [!] Error compiling steam_api64.dll & exit /b 1 )

echo.
echo ====================================================================
echo [OK] All ReFix DLL proxies successfully built in build\
echo   - build\winmm.dll
echo   - build\EOSSDK-Win64-Shipping.dll
echo   - build\RedboneEOS.dll
echo   - build\steam_api64.dll
echo ====================================================================

:: Automatically synchronize freshly built binaries to bin/
if exist bin\ (
    copy /Y build\winmm.dll bin\winmm.dll >nul
    copy /Y build\EOSSDK-Win64-Shipping.dll bin\EOSSDK-Win64-Shipping.dll >nul
    copy /Y build\RedboneEOS.dll bin\RedboneEOS.dll >nul
    copy /Y build\steam_api64.dll bin\steam_api64.dll >nul
    echo [OK] Synchronized freshly compiled proxies to bin\
)