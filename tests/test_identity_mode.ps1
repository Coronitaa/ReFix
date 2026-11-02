# ReFix LAN identity modes (requirement: a patched game copied to another PC must not reuse the SteamID
# stored in its folder).
#   auto  -> the SteamID pinned in ReFix.ini / steam_settings is ignored; the ID comes from this PC
#   fixed -> the pinned SteamID is used as-is
# Usage: powershell -ExecutionPolicy Bypass -File tests\test_identity_mode.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $root "bin\test_identity_probe.exe"
$proxy = Join-Path $root "bin\steam_api64_test.dll"
foreach ($f in @($probe, $proxy)) {
    if (-not (Test-Path -LiteralPath $f)) { Write-Host "[FAIL] Missing $f (run build.bat)" -ForegroundColor Red; exit 1 }
}

# Same derivation as GenerateDeterministicSteamID() / deploy_helper.ps1
$guid = (Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Cryptography" -Name "MachineGuid").MachineGuid
$hash = [System.Security.Cryptography.MD5]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($guid))
$accountId = ([System.BitConverter]::ToUInt32($hash, 0) -band 0x1FFFFFFF) + 100000000
$machineSteamId = [uint64]76561197960265728 + [uint64]$accountId

$pinnedSteamId = "76561198000000042"
$failures = 0

function Invoke-Probe([string]$IdentityMode, [string]$Name) {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("refix_identity_" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path (Join-Path $dir "steam_settings") -Force | Out-Null
    Copy-Item -LiteralPath $probe -Destination $dir
    Copy-Item -LiteralPath $proxy -Destination (Join-Path $dir "steam_api64.dll")
    $modeLine = if ($IdentityMode) { "IdentityMode = $IdentityMode" } else { "" }
    @"
[Network]
Mode = force_lan

[User]
$modeLine
Name = $Name
SteamId = $pinnedSteamId
"@ | Set-Content -LiteralPath (Join-Path $dir "ReFix.ini") -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $dir "steam_settings\force_steamid.txt") -Value $pinnedSteamId -Encoding ASCII

    $out = & (Join-Path $dir "test_identity_probe.exe") 2>&1 | Out-String
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    $sid = if ($out -match "STEAMID=(\d+)") { $Matches[1] } else { "" }
    $nm = if ($out -match "NAME=([^\r\n]*)") { $Matches[1] } else { "" }
    return @{ SteamId = $sid; Name = $nm; Raw = $out }
}

function Assert-Equal([string]$Label, [string]$Actual, [string]$Expected) {
    if ($Actual -eq $Expected) {
        Write-Host "  [PASS] $Label = $Actual" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $Label = '$Actual' (expected '$Expected')" -ForegroundColor Red
        $script:failures++
    }
}

Write-Host "--- IdentityMode=auto: copied folder with a pinned SteamID ---"
$auto = Invoke-Probe "auto" "CopiedPlayer"
Assert-Equal "SteamID (machine-derived, pinned ID ignored)" $auto.SteamId "$machineSteamId"
Assert-Equal "Persona name" $auto.Name "CopiedPlayer"

Write-Host "--- IdentityMode unset: auto is the default ---"
$default = Invoke-Probe "" "CopiedPlayer"
Assert-Equal "SteamID (default mode)" $default.SteamId "$machineSteamId"

Write-Host "--- IdentityMode=fixed: pinned SteamID honoured ---"
$fixed = Invoke-Probe "fixed" "FixedPlayer"
Assert-Equal "SteamID (pinned)" $fixed.SteamId $pinnedSteamId

if ($failures -gt 0) {
    Write-Host "[FAIL] $failures identity assertion(s) failed" -ForegroundColor Red
    if ($auto.Raw) { Write-Host $auto.Raw }
    exit 1
}
Write-Host "[ALL PASS] ReFix LAN identity modes verified" -ForegroundColor Green
exit 0
