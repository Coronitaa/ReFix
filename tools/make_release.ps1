# Builds the redistributable ReFix release folder from the current repository files.
#
# The package used to be a hand-made copy that drifted from the repo: it kept an old
# deploy_helper.ps1 that still deployed Goldberg and had no bin\lan, so option [2] failed with
# "Goldberg steam_api64.dll not found". Regenerate it after every build instead of editing it.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 [-OutDir <folder>]

param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $repo "ReFix_LAN_2.0_Release" }
$bin = Join-Path $repo "bin"

# Product binaries AutoDeploy.bat / deploy_helper.ps1 cannot run without.
$required = @(
    "bin\lan\steam_api64.dll", "bin\lan\EOSSDK-Win64-Shipping.dll", "bin\lan\RedboneEOS.dll",
    "bin\lan\x86\steam_api.dll",
    "bin\online\steam_api64.dll", "bin\online\winmm.dll", "bin\online\EOSSDK-Win64-Shipping.dll",
    "bin\online\x86\steam_api.dll",
    "bin\deploy_helper.ps1", "AutoDeploy.bat", "AutoDeploy.sh", "Uninstall_ReFix.bat", "ReFix.ini", "LICENSE"
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $repo $_)) })
if ($missing.Count -gt 0) {
    throw "Missing build outputs (run build.bat and build_online.bat first): $($missing -join ', ')"
}

# Empty the folder instead of deleting it: a File Explorer window open on it would make that fail.
if (Test-Path -LiteralPath $OutDir) { Get-ChildItem -LiteralPath $OutDir -Force | Remove-Item -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $OutDir "bin") -Force | Out-Null

function Copy-Into([string]$relative) {
    $src = Join-Path $repo $relative
    $dst = Join-Path $OutDir $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
}

# Entry points
foreach ($f in "AutoDeploy.bat", "AutoDeploy.sh", "Uninstall_ReFix.bat", "ReFix.ini", "LICENSE") { Copy-Into $f }

# Deploy scripts and the small helpers they call
Get-ChildItem -LiteralPath $bin -File | Where-Object {
    $_.Extension -in ".ps1", ".py" -or
    $_.Name -in "Install_ReFix_Steam_Shortcut.bat", "ReFix.ini", "ReFixSync.dll", "steam_api64_valve.dll"
} | ForEach-Object { Copy-Into ("bin\" + $_.Name) }

# Product folders: ReFix LAN, ReFix Online, the legacy x86 fallback and the deploy tools
foreach ($d in "lan", "online", "goldberg", "valve", "bepinex", "tools") {
    if (Test-Path -LiteralPath (Join-Path $bin $d)) { Copy-Into ("bin\" + $d) }
}

# Never ship per-machine state: logs, identities or AppID files written by local runs or tests.
Get-ChildItem -LiteralPath $OutDir -Recurse -File | Where-Object {
    $_.Name -in "ReFix.log", "steam_appid.txt", "force_steamid.txt", "user_steam_id.txt" -or $_.Name -like "*_test*"
} | Remove-Item -Force
Get-ChildItem -LiteralPath $OutDir -Recurse -Directory -Filter "steam_settings" | Remove-Item -Recurse -Force

# Line endings are whatever the working copy has, which is not always what .gitattributes says. cmd.exe
# needs CRLF in .bat/.cmd (with LF, "goto LABEL" misses labels and the window closes halfway through
# AutoDeploy) and bash needs LF in .sh ($'\r': command not found). Normalise both here.
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
Get-ChildItem -LiteralPath $OutDir -Recurse -File | Where-Object { $_.Extension -in ".bat", ".cmd", ".sh" } | ForEach-Object {
    $text = [System.IO.File]::ReadAllText($_.FullName) -replace "`r`n", "`n"
    if ($_.Extension -ne ".sh") { $text = $text -replace "`n", "`r`n" }
    [System.IO.File]::WriteAllText($_.FullName, $text, $utf8NoBom)
}

$readme = @"
ReFix 2.0 - LAN / Online
========================

1. Run AutoDeploy.bat.
2. Pick the game folder.
3. Choose the mode:
   [1] ReFix Online via Steam  - uses the Steam client.
   [2] ReFix LAN without Steam - plays on the local network, Steam not needed.
4. Launch the game with its own .exe (or a shortcut to that .exe).
   steam:// shortcuts start the game through Steam, without ReFix LAN.

ReFix LAN
- Every PC (and every instance on one PC) gets its own identity automatically
  ([User] IdentityMode = auto in ReFix.ini). A patched game folder can be copied to
  another PC as is.
- Several instances of the same game can run on one PC.
- Players on the same network find each other automatically.

- In-game overlay (AutoDeploy asks, [Overlay] in ReFix.ini): the ReFix overlay (Shift+Tab, or the
  Xbox / PS button of a controller), Steam's own overlay, or none. Overlay sounds: [Overlay] Sounds.
- Debug logging is off ([Debug] EnableLog = false); turn it on only to report a problem.

Uninstall_ReFix.bat restores the game's original files and gives the game this PC's own saves back.
ReFix LAN profiles (and the saves each one keeps) stay in %APPDATA%\ReFix LAN: every ReFix game of
the PC shares them.

Linux (Steam Play / Proton)
- Run: bash AutoDeploy.sh
  or:  bash AutoDeploy.sh "<game folder>" lan|online [appid]
- ReFix Online: start the game from Steam. ReFix LAN: start the .exe with Proton/Wine
  or add it to Steam as a non-Steam game.
- Undo: bash AutoDeploy.sh --uninstall "<game folder>"
"@
Set-Content -LiteralPath (Join-Path $OutDir "README.txt") -Value $readme -Encoding UTF8

$files = Get-ChildItem -LiteralPath $OutDir -Recurse -File
"Release written to {0}: {1} files, {2:N1} MB" -f $OutDir, $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB)
