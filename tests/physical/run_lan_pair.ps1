# Physical ReFix LAN test with two instances of a real game on this PC.
#   1. Backs up every file ReFix deploy can touch in the game folder.
#   2. Deploys ReFix LAN with AutoDeploy.bat (unattended, product 2 = LAN).
#   3. Launches the game twice, waits, then reads ReFix.log: both instances must initialize, get distinct
#      SteamIDs / slots and discover each other through LanCore.
#   4. Stops both instances and restores the game folder exactly as it was.
# Usage:
#   powershell -ExecutionPolicy Bypass -File tests\physical\run_lan_pair.ps1 -GameDir "D:\Games\Valheim (2)" -Exe "valheim.exe"
param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [Parameter(Mandatory = $true)][string]$Exe,          # relative to GameDir
    [int]$Seconds = 75,
    [string]$GameArgs = "",
    [string]$ReportDir = "",
    [ValidateSet("lan", "online")][string]$Product = "lan",
    [switch]$KeepDeployed
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$GameDir = $GameDir.TrimEnd('\')
$exePath = Join-Path $GameDir $Exe
if (-not (Test-Path -LiteralPath $exePath)) { Write-Host "[FAIL] Missing exe: $exePath" -ForegroundColor Red; exit 2 }
$gameKey = (Split-Path -Leaf $GameDir) -replace '[^A-Za-z0-9_.-]', '_'
if (-not $ReportDir) { $ReportDir = Join-Path $env:TEMP "refix_physical" }
$runDir = Join-Path $ReportDir $gameKey
$backupDir = Join-Path $runDir "backup"
if (Test-Path -LiteralPath $runDir) { Remove-Item -LiteralPath $runDir -Recurse -Force }
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null

# --- 1. Backup ---------------------------------------------------------------------------------------------
$filePatterns = @("steam_api*.dll", "winmm*.dll", "EOSSDK*.dll", "RedboneEOS*.dll", "ReFix*.ini", "ReFix*.log",
                  "steam_appid.txt", "steam_interfaces.txt", "Install_ReFix_Steam_Shortcut.bat", "add_steam_shortcut.ps1",
                  "*_refix_firewall*.bat", "ReFixSync.dll")
$dirNames = @("steam_settings", "ReFix", "saves")
function Get-TouchedFiles {
    $all = Get-ChildItem -LiteralPath $GameDir -Recurse -File -ErrorAction SilentlyContinue
    $all | Where-Object { $n = $_.Name; ($filePatterns | Where-Object { $n -like $_ }).Count -gt 0 }
}
function Get-TouchedDirs {
    Get-ChildItem -LiteralPath $GameDir -Recurse -Directory -ErrorAction SilentlyContinue | Where-Object { $dirNames -contains $_.Name }
}
$preFiles = @(Get-TouchedFiles | ForEach-Object { $_.FullName })
$preDirs = @(Get-TouchedDirs | ForEach-Object { $_.FullName })
foreach ($f in $preFiles) {
    $rel = $f.Substring($GameDir.Length + 1)
    $dst = Join-Path $backupDir ("files\" + $rel)
    New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
    Copy-Item -LiteralPath $f -Destination $dst -Force
}
foreach ($d in $preDirs) {
    $rel = $d.Substring($GameDir.Length + 1)
    $dst = Join-Path $backupDir ("dirs\" + $rel)
    New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
    Copy-Item -LiteralPath $d -Destination $dst -Recurse -Force
}
Write-Host "[*] Backed up $($preFiles.Count) files and $($preDirs.Count) folders to $backupDir"

function Restore-Game {
    foreach ($f in @(Get-TouchedFiles | ForEach-Object { $_.FullName })) {
        if ($preFiles -notcontains $f) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }
    }
    foreach ($d in @(Get-TouchedDirs | ForEach-Object { $_.FullName })) {
        if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction SilentlyContinue }
    }
    $filesRoot = Join-Path $backupDir "files"
    if (Test-Path -LiteralPath $filesRoot) {
        Get-ChildItem -LiteralPath $filesRoot -Recurse -File | ForEach-Object {
            $dst = Join-Path $GameDir $_.FullName.Substring($filesRoot.Length + 1)
            New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $dst -Force
        }
    }
    $dirsRoot = Join-Path $backupDir "dirs"
    foreach ($d in $preDirs) {
        $src = Join-Path $dirsRoot $d.Substring($GameDir.Length + 1)
        if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination (Split-Path -Parent $d) -Recurse -Force }
    }
    Write-Host "[*] Game folder restored"
}

$result = [ordered]@{ Game = (Split-Path -Leaf $GameDir); Exe = $Exe; Deploy = "?"; ProductFiles = "?"; Alive = "?"; Inits = 0; SteamIds = ""; Slots = ""; Discovery = "?"; Verdict = "FAIL"; Notes = ""; Inst1 = "?"; Inst2 = "?" }
$procs = @()
try {
    # --- 2. Deploy ReFix LAN ----------------------------------------------------------------------------------
    $env:REFIX_NON_INTERACTIVE = "1"
    $env:REFIX_DEPLOY_MODE = if ($Product -eq "online") { "1" } else { "2" }
    $deployLog = Join-Path $runDir "deploy.log"
    # Use the real AutoDeploy.bat; only if the working copy is LF-only (cmd.exe cannot resolve goto labels then) run a temporary CRLF copy
    $realBat = Join-Path $repo "AutoDeploy.bat"
    $batBytes = [System.IO.File]::ReadAllBytes($realBat)
    $hasCr = $batBytes -contains 13
    $bat = $realBat
    if (-not $hasCr) {
        $bat = Join-Path $repo "AutoDeploy_phys_crlf.bat"
        $batText = [System.IO.File]::ReadAllText($realBat) -replace "`r?`n", "`r`n"
        [System.IO.File]::WriteAllText($bat, $batText, [System.Text.Encoding]::ASCII)
        $result.Notes += "AutoDeploy.bat is LF-only on disk (CRLF copy used); "
    }
    try { & cmd.exe /c "`"$bat`" `"$GameDir`"" *> $deployLog } finally { if ($bat -ne $realBat) { Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue } }
    $deployOk = ($LASTEXITCODE -eq 0)
    $result.Deploy = if ($deployOk) { "OK" } else { "FAIL($LASTEXITCODE)" }
    if (-not $deployOk) { $result.Notes = "deploy failed, see $deployLog"; throw "deploy failed" }

    # Every deployed Steam DLL must be byte-identical to the chosen product's binary, never the other one's
    $productDir = Join-Path $repo ("bin\" + $Product)
    $deployed = @(Get-ChildItem -LiteralPath $GameDir -Recurse -File -ErrorAction SilentlyContinue |
                  Where-Object { $_.Name -eq "steam_api64.dll" -or $_.Name -eq "steam_api.dll" })
    $mismatch = @()
    foreach ($d in $deployed) {
        $ref = if ($d.Name -eq "steam_api64.dll") { Join-Path $productDir "steam_api64.dll" } else { Join-Path $productDir "x86\steam_api.dll" }
        $isGoldbergFallback = ($d.Name -eq "steam_api.dll") -and
            ((Get-FileHash -LiteralPath $d.FullName).Hash -eq (Get-FileHash -LiteralPath (Join-Path $repo "bin\goldberg\steam_api.dll")).Hash)
        if (-not $isGoldbergFallback -and (Get-FileHash -LiteralPath $d.FullName).Hash -ne (Get-FileHash -LiteralPath $ref).Hash) {
            $mismatch += $d.FullName.Substring($GameDir.Length + 1)
        }
    }
    $ini = @(Get-ChildItem -LiteralPath $GameDir -Recurse -File -Filter "ReFix.ini" -ErrorAction SilentlyContinue)
    $result.ProductFiles = if ($mismatch.Count -eq 0 -and $deployed.Count -gt 0 -and $ini.Count -gt 0) { "OK($($deployed.Count) dll, ini)" } else { "BAD" }
    if ($mismatch.Count -gt 0) { $result.Notes += "not from bin\$Product : $($mismatch -join ', '); " }
    if ($ini.Count -eq 0) { $result.Notes += "no ReFix.ini; " }
    if ($Product -eq "online") {
        # Online is verified statically (needs the Steam client to actually play)
        $result.Verdict = if ($result.ProductFiles -like "OK*") { "PASS" } else { "FAIL" }
        throw "online-static-done"
    }

    Get-ChildItem -LiteralPath $GameDir -Recurse -File -Filter "ReFix*.log" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

    # --- 3. Two instances ---------------------------------------------------------------------------------------
    $start = Get-Date
    $exeName = [System.IO.Path]::GetFileNameWithoutExtension($exePath)
    for ($i = 0; $i -lt 2; $i++) {
        $sp = @{ FilePath = $exePath; WorkingDirectory = (Split-Path -Parent $exePath); PassThru = $true }
        if ($GameArgs) { $sp.ArgumentList = $GameArgs }
        $procs += Start-Process @sp
        Start-Sleep -Seconds 6
    }
    $instState = @("running", "running")
    $instExit = @($null, $null)
    $waitEnd = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $waitEnd) {
        for ($k = 0; $k -lt $procs.Count; $k++) {
            if ($null -eq $instExit[$k] -and $procs[$k].HasExited) {
                $instExit[$k] = [int]$procs[$k].ExitCode
                $instState[$k] = "exited($($instExit[$k])) after $([int]((Get-Date) - $start).TotalSeconds - $k * 6)s"
            }
        }
        Start-Sleep -Seconds 1
    }
    $result.Inst1 = $instState[0]; $result.Inst2 = $instState[1]
    # Game processes still running from this folder (covers launchers that respawn the real exe)
    $alive = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $path = $null
        try { $path = $_.Path } catch {}
        $path -and $path.StartsWith($GameDir, [StringComparison]::OrdinalIgnoreCase) -and ($_.ProcessName -like "$exeName*")
    })
    $result.Alive = "$($alive.Count)"

    # --- Analyse logs ---------------------------------------------------------------------------------------------
    Get-ChildItem -LiteralPath $GameDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $start } |
        ForEach-Object { "$($_.LastWriteTime.ToString('HH:mm:ss')) $($_.Length) $($_.FullName.Substring($GameDir.Length + 1))" } | Set-Content -LiteralPath (Join-Path $runDir "modified_files.txt")
    # Unity player logs (most recently written Player.log under LocalLow)
    $pl = Get-ChildItem -Path (Join-Path $env:USERPROFILE "AppData\LocalLow") -Recurse -Filter "Player.log" -ErrorAction SilentlyContinue |
          Where-Object { $_.LastWriteTime -ge $start } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($pl) {
        Copy-Item -LiteralPath $pl.FullName -Destination (Join-Path $runDir "Unity_Player.log") -Force
        $prev = Join-Path $pl.DirectoryName "Player-prev.log"
        if (Test-Path -LiteralPath $prev) { Copy-Item -LiteralPath $prev -Destination (Join-Path $runDir "Unity_Player-prev.log") -Force }
    }
    $logs = @(Get-ChildItem -LiteralPath $GameDir -Recurse -File -Filter "ReFix*.log" -ErrorAction SilentlyContinue |
              Where-Object { $_.LastWriteTime -ge $start })
    $text = ($logs | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue }) -join "`n"
    foreach ($l in $logs) { Copy-Item -LiteralPath $l.FullName -Destination (Join-Path $runDir ($l.Directory.Name + "_" + $l.Name)) -Force }
    $ids = @([regex]::Matches($text, "SteamID Source: '[^']*' -> (\d+)") | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
    $slots = @([regex]::Matches($text, "Multi-Instance Slot: (\d+)") | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
    $result.Inits = ([regex]::Matches($text, "Universal LAN Core initialized")).Count
    $result.SteamIds = ($ids -join ",")
    $result.Slots = ($slots -join ",")
    $disc = @([regex]::Matches($text, "LanCore PeerDiscovered: steamID=(\d+)") | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
    $mutual = ($ids.Count -ge 2) -and (@($ids | Where-Object { $disc -contains $_ }).Count -ge 2)
    $result.Discovery = if ($mutual) { "MUTUAL" } elseif ($disc.Count -gt 0) { "ONE-WAY" } else { "NONE" }
    if ($text -match "IDENTITY COLLISION") { $result.Notes += "identity collision logged; " }
    if ($logs.Count -eq 0) { $result.Notes += "no ReFix.log written (proxy not loaded?); " }
    $ok = ($alive.Count -ge 2) -and ($ids.Count -ge 2) -and $mutual
    $result.Verdict = if ($ok) { "PASS" } else { "FAIL" }
}
catch {
    if ("$_" -ne "online-static-done" -and -not $result.Notes) { $result.Notes = "$_" }
}
finally {
    foreach ($p in $procs) { try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {} }
    Start-Sleep -Seconds 1
    Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $path = $null
        try { $path = $_.Path } catch {}
        $path -and $path.StartsWith($GameDir, [StringComparison]::OrdinalIgnoreCase)
    } | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Remove-Item Env:REFIX_NON_INTERACTIVE, Env:REFIX_DEPLOY_MODE -ErrorAction SilentlyContinue
    if (-not $KeepDeployed) { Restore-Game }
}
$line = ($result.Values | ForEach-Object { "$_" }) -join " | "
Write-Host "RESULT | $line"
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir "result.json") -Encoding UTF8
if ($result.Verdict -eq "PASS") { exit 0 } else { exit 1 }
