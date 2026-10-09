# test_steam_adapter_multiprocess.ps1 - Automated Dual-Process Steamworks LAN Integration Harness
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$BinPath = "",
    [Parameter(Mandatory = $false)]
    [int]$TimeoutSeconds = 30,
    [Parameter(Mandatory = $false)]
    [switch]$DisableFallbacks = $false
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BinPath)) {
    $BinPath = Join-Path -Path $PSScriptRoot -ChildPath "..\bin\test_steam_lancore_adapter.exe"
}

$resolvedBin = (Resolve-Path -LiteralPath $BinPath).Path
if (-not (Test-Path -LiteralPath $resolvedBin)) {
    Write-Error "Binary not found at: $resolvedBin"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   REFIX STEAMWORKS ADAPTER MULTIPROCESS HARNESS (LAN)    " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "[*] Target Binary:    $resolvedBin"
Write-Host "[*] Timeout:          $TimeoutSeconds seconds"
Write-Host "[*] Canonical Only:   $DisableFallbacks (REFIX_DISABLE_LEGACY_FALLBACKS)"

$hostLogOut = Join-Path -Path $PSScriptRoot -ChildPath "adapter_host.log"
$hostLogErr = Join-Path -Path $PSScriptRoot -ChildPath "adapter_host_err.log"
$clientLogOut = Join-Path -Path $PSScriptRoot -ChildPath "adapter_client.log"
$clientLogErr = Join-Path -Path $PSScriptRoot -ChildPath "adapter_client_err.log"

Remove-Item -LiteralPath $hostLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $hostLogErr -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogErr -ErrorAction SilentlyContinue

function Start-AdapterProcess {
    param([string]$Mode, [string]$LogOut, [string]$LogErr)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $envPrefix = if ($DisableFallbacks) { "set REFIX_DISABLE_LEGACY_FALLBACKS=1 && " } else { "" }
    $psi.Arguments = "/c `"$envPrefix`"$resolvedBin`" --mode $Mode > `"$LogOut`" 2> `"$LogErr`"`""
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    return [System.Diagnostics.Process]::Start($psi)
}

Write-Host "[*] Launching HOST process..." -ForegroundColor Yellow
$hostProc = Start-AdapterProcess -Mode "host" -LogOut $hostLogOut -LogErr $hostLogErr

# Allow Host 1.5s to bind, start transport, and advertise lobby
Start-Sleep -Milliseconds 1500

if ($hostProc.HasExited) {
    Write-Error "[FAIL] Host process terminated prematurely with exit code: $($hostProc.ExitCode)"
    if (Test-Path -LiteralPath $hostLogErr) { Get-Content -LiteralPath $hostLogErr }
    exit 1
}

Write-Host "[*] Launching CLIENT process..." -ForegroundColor Yellow
$clientProc = Start-AdapterProcess -Mode "client" -LogOut $clientLogOut -LogErr $clientLogErr

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$timeoutMs = $TimeoutSeconds * 1000

while ((-not $clientProc.HasExited -or -not $hostProc.HasExited) -and ($stopwatch.ElapsedMilliseconds -lt $timeoutMs)) {
    Start-Sleep -Milliseconds 500
}

if (-not $clientProc.HasExited) {
    Write-Warning "[TIMEOUT] Client did not finish within $TimeoutSeconds seconds. Terminating..."
    Stop-Process -Id $clientProc.Id -Force -ErrorAction SilentlyContinue
}

if (-not $hostProc.HasExited) {
    Write-Warning "[TIMEOUT] Host did not finish within $TimeoutSeconds seconds. Terminating..."
    Stop-Process -Id $hostProc.Id -Force -ErrorAction SilentlyContinue
}

$hostProc.WaitForExit()
$clientProc.WaitForExit()

$hostExit = $hostProc.ExitCode
$clientExit = $clientProc.ExitCode

$hostOutput = if (Test-Path -LiteralPath $hostLogOut) { Get-Content -LiteralPath $hostLogOut -Raw } else { "" }
$clientOutput = if (Test-Path -LiteralPath $clientLogOut) { Get-Content -LiteralPath $clientLogOut -Raw } else { "" }

Write-Host "`n--- HOST OUTPUT ---"
Write-Host $hostOutput
Write-Host "--- CLIENT OUTPUT ---"
Write-Host $clientOutput

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "                TEST AUDIT & VERIFICATION                 " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

Write-Host "Host Exit Code:   $hostExit" -ForegroundColor $(if ($hostExit -eq 0) { "Green" } else { "Red" })
Write-Host "Client Exit Code: $clientExit" -ForegroundColor $(if ($clientExit -eq 0) { "Green" } else { "Red" })

$hostLobbyId = ""
$clientLobbyId = ""
$lobbyIdValid = $false

if ($hostOutput -match 'Created Lobby ID:\s+(\d+)') {
    $hostLobbyId = $matches[1]
    [uint64]$hId = [uint64]$hostLobbyId
    $upper32 = ($hId -shr 32)
    if ($upper32 -eq 0x01840000) {
        $lobbyIdValid = $true
        Write-Host "[PASS] Verified canonical CSteamID format for Host Lobby ID: $hostLobbyId (Upper 32-bits = 0x01840000: Universe=Public, AccountType=Chat, Instance=Lobby)" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Non-canonical CSteamID format: Upper 32-bits is 0x$($upper32.ToString('X8')), expected 0x01840000!" -ForegroundColor Red
    }
}

if ($clientOutput -match 'Found Lobby ID:\s+(\d+)') {
    $clientLobbyId = $matches[1]
    if ($clientLobbyId -eq $hostLobbyId) {
        Write-Host "[PASS] Host and Client derived 100% IDENTICAL CSteamID: $clientLobbyId" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Lobby ID mismatch: Host=$hostLobbyId, Client=$clientLobbyId" -ForegroundColor Red
        $lobbyIdValid = $false
    }
}

$hostSuccess = ($hostExit -eq 0) -and
               $hostOutput.Contains("[HOST_READY]") -and
               $hostOutput.Contains("[HOST_CLIENT_JOINED]") -and
               $hostOutput.Contains("[HOST_PING_VERIFIED]") -and
               $hostOutput.Contains("[HOST_FRAGMENT_VERIFIED]") -and
               $hostOutput.Contains("[HOST_CLIENT_LEFT]") -and
               $hostOutput.Contains("[HOST_SUCCESS]")

$clientSuccess = ($clientExit -eq 0) -and
                 $clientOutput.Contains("[CLIENT_DISCOVERED_LOBBY]") -and
                 $clientOutput.Contains("[CLIENT_JOIN_SUCCESS]") -and
                 $clientOutput.Contains("[CLIENT_PONG_VERIFIED]") -and
                 $clientOutput.Contains("[CLIENT_FRAGMENT_VERIFIED]") -and
                 $clientOutput.Contains("[CLIENT_SUCCESS]")

if ($hostSuccess -and $clientSuccess -and $lobbyIdValid) {
    Write-Host "`n[PASS] E2E DUAL-PROCESS STEAMWORKS LAN INTEGRATION VERIFIED!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n[FAIL] Steamworks multi-process verification failed!" -ForegroundColor Red
    exit 1
}
