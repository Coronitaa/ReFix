@echo off
REM =============================================================================
REM ReFix Build Script v2.0
REM =============================================================================
REM Compiles all three ReFix DLL proxies using MSVC 2022.
REM All DLLs use x64 ASM trampolines for function forwarding.
REM =============================================================================

setlocal enabledelayedexpansion

echo.
echo  ====================================
echo   ReFix Build System v2.0
echo  ====================================
echo.

REM --- Find and initialize MSVC environment ---
if not defined VSINSTALLDIR (
    echo [*] Initializing Visual Studio 2022 environment...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        echo [!] ERROR: Could not find Visual Studio 2022.
        exit /b 1
    )
)
echo [OK] MSVC x64 environment ready.

REM --- Create build output directory ---
if not exist "build" mkdir build
cd /d "%~dp0src"

echo.
echo  --- [1/3] winmm.dll (Loader + Config, 180 forwarded exports) ---
echo.

echo [*] Assembling winmm_fwd.asm (180 trampolines)...
ml64 /nologo /c /Fo"..\build\winmm_fwd.obj" winmm_fwd.asm
if errorlevel 1 ( echo [!] FAIL: winmm_fwd.asm & exit /b 1 )

echo [*] Compiling winmm_proxy.cpp...
cl /nologo /LD /O2 /EHsc /MD winmm_proxy.cpp "..\build\winmm_fwd.obj" ^
    /Fe"..\build\winmm.dll" /Fo"..\build\winmm_proxy.obj" ^
    /link /DEF:winmm_proxy.def /MACHINE:X64
if errorlevel 1 ( echo [!] FAIL: winmm.dll & exit /b 1 )
echo [OK] winmm.dll built.

echo.
echo  --- [2/3] steam_api64.dll (Steam Proxy, 1055 forwarded + 4 intercepted) ---
echo.

echo [*] Assembling steam_fwd.asm (1055 trampolines)...
ml64 /nologo /c /Fo"..\build\steam_fwd.obj" steam_fwd.asm
if errorlevel 1 ( echo [!] FAIL: steam_fwd.asm & exit /b 1 )

echo [*] Compiling steam_proxy.cpp...
cl /nologo /LD /O2 /EHsc /MD steam_proxy.cpp "..\build\steam_fwd.obj" ^
    /Fe"..\build\steam_api64.dll" /Fo"..\build\steam_proxy.obj" ^
    /link /DEF:steam_api64.def /MACHINE:X64
if errorlevel 1 ( echo [!] FAIL: steam_api64.dll & exit /b 1 )
echo [OK] steam_api64.dll built.

echo.
echo  --- [3/3] EOSSDK-Win64-Shipping.dll (EOS Proxy, 679 forwarded + 1 custom) ---
echo.

echo [*] Assembling eos_fwd.asm (679 trampolines)...
ml64 /nologo /c /Fo"..\build\eos_fwd.obj" eos_fwd.asm
if errorlevel 1 ( echo [!] FAIL: eos_fwd.asm & exit /b 1 )

echo [*] Compiling eos_proxy.cpp...
cl /nologo /LD /O2 /EHsc /MD eos_proxy.cpp "..\build\eos_fwd.obj" ^
    /Fe"..\build\EOSSDK-Win64-Shipping.dll" /Fo"..\build\eos_proxy.obj" ^
    /link /DEF:eos_proxy.def /MACHINE:X64
if errorlevel 1 ( echo [!] FAIL: EOSSDK-Win64-Shipping.dll & exit /b 1 )
echo [OK] EOSSDK-Win64-Shipping.dll built.

echo.
echo  ====================================
echo   BUILD COMPLETE
echo  ====================================
echo.
echo  Output DLLs:
for %%f in ("..\build\*.dll") do echo    %%~nxf  (%%~zf bytes)
echo.
echo  Run deploy.bat to apply ReFix to the game.
echo.

cd /d "%~dp0"
