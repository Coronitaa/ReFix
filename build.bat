@echo off
REM =============================================================================
REM ReFix Build Script (Compiles & Digitally Signs winmm.dll, EOSSDK-Win64-Shipping.dll, steam_api64.dll)
REM =============================================================================

if not exist build mkdir build

echo [*] Initializing MSVC x64 Build Environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo [*] Assembling forwarding tables with MASM x64...
ml64 /c /nologo /Fobuild\winmm_fwd.obj src\winmm_fwd.asm
ml64 /c /nologo /Fobuild\eos_fwd.obj src\eos_fwd.asm
ml64 /c /nologo /Fobuild\steam_fwd.obj src\steam_fwd.asm

echo [*] Building winmm.dll proxy...
cl /nologo /O2 /EHsc /LD src\winmm_proxy.cpp src\server_browser_gui.cpp build\winmm_fwd.obj /Febuild\winmm.dll user32.lib kernel32.lib advapi32.lib comctl32.lib /link /DEF:src\winmm_proxy.def

echo [*] Building EOSSDK-Win64-Shipping.dll and RedboneEOS.dll proxy...
cl /nologo /O2 /EHsc /LD src\eos_proxy.cpp src\upnp_firewall.cpp build\eos_fwd.obj /Febuild\EOSSDK-Win64-Shipping.dll user32.lib kernel32.lib advapi32.lib ws2_32.lib wininet.lib ole32.lib oleaut32.lib /link /DEF:src\eos_proxy.def
copy /Y build\EOSSDK-Win64-Shipping.dll build\RedboneEOS.dll >nul

echo [*] Building steam_api64.dll proxy...
cl /nologo /O2 /EHsc /LD src\steam_proxy.cpp src\steam_p2p_hook.cpp src\upnp_firewall.cpp src\minhook\buffer.c src\minhook\hook.c src\minhook\trampoline.c src\minhook\hde\hde64.c build\steam_fwd.obj /Febuild\steam_api64.dll user32.lib kernel32.lib ws2_32.lib iphlpapi.lib ole32.lib oleaut32.lib /link /DEF:src\steam_api64.def

if %ERRORLEVEL% equ 0 (
    if exist sign_dlls.ps1 (
        echo [*] Digitally Signing All Proxy DLLs with Authenticode Certificate...
        powershell -ExecutionPolicy Bypass -File sign_dlls.ps1
    )
    echo [OK] All ReFix DLL proxies successfully built in build\
) else (
    echo [!] Build had errors! Check output above.
)
