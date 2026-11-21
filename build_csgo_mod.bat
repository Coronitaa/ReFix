@echo off
setlocal enabledelayedexpansion
title ReFix CS:GO Native Mod Build Script

cd /d "%~dp0"
if not exist build_csgo_mod mkdir build_csgo_mod

echo [*] Initializing MSVC x86 Build Environment...

if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" (
    where cl >nul 2>&1
    if !ERRORLEVEL! equ 0 goto COMPILE
)

set "VCVARS="
if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"

if defined VCVARS (
    call "!VCVARS!" >nul 2>&1
)

:COMPILE
echo [*] Building x86 refix_csgo.dll (Native Tier Mod)...
set "SRC=src\games\csgo\csgo_mod.cpp src\games\csgo\csgo_handlers.cpp"

cl /nologo /O2 /Zi /FS /EHsc /std:c++17 /utf-8 /DREFIX_MOD_EXPORTS=1 /LD /Fobuild_csgo_mod\ /Isrc /Isrc\include /Ithird_party !SRC! /Febuild_csgo_mod\refix_csgo.dll /Fdbuild_csgo_mod\refix_csgo.pdb user32.lib kernel32.lib ws2_32.lib advapi32.lib ole32.lib /link /MACHINE:X86 /DEBUG

if %ERRORLEVEL% neq 0 (
    echo [!] Error compiling refix_csgo.dll
    exit /b 1
)

if not exist bin mkdir bin
copy /Y build_csgo_mod\refix_csgo.dll bin\refix_csgo.dll >nul
echo [OK] refix_csgo.dll build successful: bin\refix_csgo.dll

if exist build_csgo_mod\unpacked (
    echo [*] Updating unpacked mod directory and manifest...
    copy /Y build_csgo_mod\refix_csgo.dll build_csgo_mod\unpacked\refix_csgo.dll >nul
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$h = (Get-FileHash 'build_csgo_mod\unpacked\refix_csgo.dll' -Algorithm SHA256).Hash.ToLowerInvariant(); $j = Get-Content 'build_csgo_mod\unpacked\refixmod.json' -Raw; $j2 = $j -replace '\"refix_csgo\.dll\":\s*\"[0-9a-fA-F]+\"', ('\"refix_csgo.dll\":\"' + $h + '\"'); Set-Content 'build_csgo_mod\unpacked\refixmod.json' -Value $j2 -NoNewline"
    if exist bin\refixmod.exe (
        echo [*] Packing refix.csgo.lan_overhaul.rfmod...
        bin\refixmod.exe pack build_csgo_mod\unpacked refix_mods\refix.csgo.lan_overhaul.rfmod
        powershell -NoProfile -ExecutionPolicy Bypass -Command "$rh = (Get-FileHash 'refix_mods\refix.csgo.lan_overhaul.rfmod' -Algorithm SHA256).Hash.ToLowerInvariant(); $cl = Get-Content 'refix_mods\catalog.tsv'; $cl2 = $cl | ForEach-Object { if ($_ -match '^refix\.csgo\.lan_overhaul\t') { $p = $_ -split \"`t\"; $p[6] = $rh; $p -join \"`t\" } else { $_ } }; Set-Content 'refix_mods\catalog.tsv' -Value $cl2"
        if exist "C:\Users\andre\Games\Counter-Strike_Global Offensive\refix_mods" (
            copy /Y refix_mods\refix.csgo.lan_overhaul.rfmod "C:\Users\andre\Games\Counter-Strike_Global Offensive\refix_mods\refix.csgo.lan_overhaul.rfmod" >nul
        )
        echo [OK] Packaged and updated refix_mods\catalog.tsv
    )
)
exit /b 0
