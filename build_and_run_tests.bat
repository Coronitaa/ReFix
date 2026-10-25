@echo off
setlocal enabledelayedexpansion

if not exist "%~dp0build\tests" mkdir "%~dp0build\tests"
cd /d "%~dp0"

where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    set "VCVARS="
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if defined VCVARS call "!VCVARS!" >nul 2>&1
)

echo ====================================================================
echo [*] Photon P2P emulator tests (codec, transport, server roles, Fusion plugin)
echo ====================================================================
call "%~dp0tests\run_photon_tests.bat"
if %ERRORLEVEL% neq 0 ( echo [!] Photon tests failed & exit /b 1 )

echo ====================================================================
echo [*] Steam networking compatibility tests
echo ====================================================================
cl /nologo /EHsc /std:c++17 /Isrc\include /Isrc\include\steam tests\test_steam_network_compat.cpp src\steam_network_compat.cpp /Fobuild\tests\ /Febuild\tests\test_steam_network_compat.exe ws2_32.lib advapi32.lib user32.lib
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_steam_network_compat & exit /b 1 )
build\tests\test_steam_network_compat.exe
if %ERRORLEVEL% neq 0 ( echo [!] Steam networking compatibility tests failed & exit /b 1 )

echo ====================================================================
echo [*] AutoDeploy path robustness and deployment pipeline tests
echo ====================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\test_path_robustness.ps1"
if %ERRORLEVEL% neq 0 ( echo [!] Path robustness tests failed & exit /b 1 )

echo:
echo [OK] All ReFix test suites passed.
exit /b 0
