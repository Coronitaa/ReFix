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

set CFLAGS=/nologo /EHsc /std:c++17 /O2 /Isrc /DREFIX_TESTING=1
set LIBS=ws2_32.lib advapi32.lib user32.lib

echo [*] Compiling test_p2p_network_harness...
cl %CFLAGS% tests\test_p2p_network_harness.cpp src\steam_p2p_hook.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c /Febuild\tests\test_p2p_network_harness.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_p2p_network_harness & exit /b 1 )

echo [*] Executing test_p2p_network_harness...
build\tests\test_p2p_network_harness.exe
