# test_steam_adapter_multiprocess.ps1 - Automated Dual-Process Steamworks LAN Integration Harness
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$BinPath = "",
    [Parameter(Mandatory = $false)]
    [int]$TimeoutSeconds = 30
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
Write-Host "[*] Target Binary: $resolvedBin"
Write-Host "[*] Timeout:       $TimeoutSeconds seconds"

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
    $psi.Arguments = "/c `"`"$resolvedBin`" --mode $Mode > `"$LogOut`" 2> `"$LogErr`"`""
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

if ($hostSuccess -and $clientSuccess) {
    Write-Host "`n[PASS] E2E DUAL-PROCESS STEAMWORKS LAN INTEGRATION VERIFIED!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n[FAIL] Steamworks multi-process verification failed!" -ForegroundColor Red
    exit 1
}
