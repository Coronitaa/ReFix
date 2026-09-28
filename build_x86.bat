@echo off
setlocal

echo [ReFix x86] Setting up MSVC 32-bit build environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC x86 environment.
    exit /b 1
)

set BUILD_DIR=%~dp0build_x86
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set SRC_DIR=%~dp0src\proxy32
set OUT_DIR=%~dp0bin\x86
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [ReFix x86] Compiling steam_api.dll (32-bit)...
cl /nologo /O2 /MD /W3 /EHsc /std:c++17 ^
   /Fo"%BUILD_DIR%\\" /Fd"%BUILD_DIR%\\" ^
   "%SRC_DIR%\steam_api_proxy.cpp" ^
   /link /DLL ^
   /OUT:"%OUT_DIR%\steam_api.dll" ^
   kernel32.lib user32.lib

if errorlevel 1 (
    echo [ERROR] Failed to compile steam_api.dll x86!
    exit /b 1
)

echo [ReFix x86] Successfully built: %OUT_DIR%\steam_api.dll
exit /b 0
