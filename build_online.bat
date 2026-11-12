@echo off
setlocal enabledelayedexpansion
title ReFix Online - Build Script

cd /d "%~dp0"
set "ROOT=%~dp0"
set "BRANCH=feature/photon-p2p-phasmophobia-interop"
set "WT=%ROOT%build\online_src"
set "OUT=%ROOT%bin\online"

echo [*] Preparing ReFix Online source worktree (%BRANCH%)...
if not exist "%ROOT%build" mkdir "%ROOT%build"

if exist "%WT%\.git" (
    echo [*] Refreshing existing worktree...
    git -C "%WT%" checkout --detach %BRANCH%
    if errorlevel 1 ( echo [!] Failed to refresh worktree & exit /b 1 )
) else (
    git worktree prune
    git worktree add --detach "%WT%" %BRANCH%
    if errorlevel 1 ( echo [!] Failed to create worktree & exit /b 1 )
)

echo [*] Building ReFix Online x64 proxies...
:: Remove stale outputs so success is judged on freshly built files
for %%F in (steam_api64.dll winmm.dll EOSSDK-Win64-Shipping.dll RedboneEOS.dll) do if exist "%WT%\build\%%F" del /Q "%WT%\build\%%F"
pushd "%WT%"
call "%WT%\build.bat"
set "RC=!ERRORLEVEL!"
popd
:: build.bat's final exit code is unreliable (trailing optional dotnet publish), so verify outputs
for %%F in (steam_api64.dll winmm.dll EOSSDK-Win64-Shipping.dll RedboneEOS.dll) do (
    if not exist "%WT%\build\%%F" ( echo [^^!] Online x64 build failed: %%F not produced & exit /b 1 )
)
if not "!RC!"=="0" echo [^^!] Warning: online build.bat exit code !RC! [binaries were produced; likely optional FusionShared publish]

echo [*] Building ReFix Online x86 steam_api proxy from this tree (it forwards to steam_api_original.dll)...
if exist "%ROOT%bin\x86\steam_api.dll" del /Q "%ROOT%bin\x86\steam_api.dll"
call "%ROOT%build_x86.bat"
set "RC=!ERRORLEVEL!"
if not "!RC!"=="0" ( echo [^^!] Online x86 build failed, exit code !RC! & exit /b 1 )

echo [*] Collecting binaries into bin\online\ ...
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\x86" mkdir "%OUT%\x86"

for %%F in (steam_api64.dll winmm.dll) do (
    if exist "%WT%\build\%%F" (
        copy /Y "%WT%\build\%%F" "%OUT%\%%F" >nul
        echo   - bin\online\%%F
    ) else (
        echo [!] Missing build output: %%F & exit /b 1
    )
)

REM EOS SDK in ReFix is universal (supports both LAN and Online via ReFix.ini config and has the latest DeviceIdAuth and VerifyIdToken hooks)
for %%F in (EOSSDK-Win64-Shipping.dll RedboneEOS.dll) do (
    if exist "%ROOT%bin\lan\%%F" (
        copy /Y "%ROOT%bin\lan\%%F" "%OUT%\%%F" >nul
        echo   - bin\online\%%F [Universal EOS v3 with DeviceIdAuth + VerifyIdToken]
    ) else (
        if exist "%WT%\build\%%F" (
            copy /Y "%WT%\build\%%F" "%OUT%\%%F" >nul
            echo   - bin\online\%%F
        ) else (
            echo [!] Missing build output: %%F & exit /b 1
        )
    )
)

if exist "%ROOT%bin\x86\steam_api.dll" (
    copy /Y "%ROOT%bin\x86\steam_api.dll" "%OUT%\x86\steam_api.dll" >nul
    echo   - bin\online\x86\steam_api.dll
) else (
    echo [!] Missing x86 steam_api.dll & exit /b 1
)

if exist "%WT%\bin\ReFix\" (
    xcopy /E /I /Y /Q "%WT%\bin\ReFix" "%OUT%\ReFix" >nul
    echo   - bin\online\ReFix\ [FusionShared companion]
) else (
    echo [!] Warning: bin\ReFix not produced [dotnet publish failed or skipped]
)

echo.
echo ====================================================================
echo [OK] ReFix Online binaries staged in bin\online\
for /R "%OUT%" %%F in (*) do echo   %%~zF bytes  %%F
echo ====================================================================
exit /b 0
