# test_lan_multiprocess.ps1 - Automated Multiprocess LAN/Loopback Adversarial Test for ReFix
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$BinPath = "",
    [Parameter(Mandatory = $false)]
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BinPath)) {
    $BinPath = Join-Path -Path $PSScriptRoot -ChildPath "..\bin\refix_net_test.exe"
}

$resolvedBin = (Resolve-Path -LiteralPath $BinPath).Path
if (-not (Test-Path -LiteralPath $resolvedBin)) {
    Write-Error "Binary not found at LiteralPath: $resolvedBin"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   REFIX MULTIPROCESS ADVERSARIAL LAN HARNESS (LOCALHOST)  " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "[*] Target Binary: $resolvedBin"
Write-Host "[*] Timeout:       $TimeoutSeconds seconds"

$hostLogOut = Join-Path -Path $PSScriptRoot -ChildPath "multiprocess_host.log"
$hostLogErr = Join-Path -Path $PSScriptRoot -ChildPath "multiprocess_host_err.log"
$clientLogOut = Join-Path -Path $PSScriptRoot -ChildPath "multiprocess_client.log"
$clientLogErr = Join-Path -Path $PSScriptRoot -ChildPath "multiprocess_client_err.log"

Remove-Item -LiteralPath $hostLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $hostLogErr -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogErr -ErrorAction SilentlyContinue

function Start-AdversarialProcess {
    param([string]$Mode, [string]$LogOut, [string]$LogErr)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $psi.Arguments = "/c `"`"$resolvedBin`" $Mode > `"$LogOut`" 2> `"$LogErr`"`""
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    return [System.Diagnostics.Process]::Start($psi)
}

Write-Host "[*] Launching HOST process..." -ForegroundColor Yellow
$hostProc = Start-AdversarialProcess -Mode "host" -LogOut $hostLogOut -LogErr $hostLogErr

Start-Sleep -Milliseconds 800

if ($hostProc.HasExited) {
    Write-Error "[FAIL] Host process terminated prematurely with exit code: $($hostProc.ExitCode)"
    if (Test-Path -LiteralPath $hostLogErr) { Get-Content -LiteralPath $hostLogErr }
    exit 1
}

Write-Host "[*] Launching CLIENT process..." -ForegroundColor Yellow
$clientProc = Start-AdversarialProcess -Mode "client" -LogOut $clientLogOut -LogErr $clientLogErr

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$timeoutMs = $TimeoutSeconds * 1000

while (-not $clientProc.HasExited -and ($stopwatch.ElapsedMilliseconds -lt $timeoutMs)) {
    Start-Sleep -Milliseconds 500
}

if (-not $clientProc.HasExited) {
    Write-Warning "[TIMEOUT] Client did not finish within $TimeoutSeconds seconds. Terminating..."
    Stop-Process -Id $clientProc.Id -Force -ErrorAction SilentlyContinue
}

while (-not $hostProc.HasExited -and ($stopwatch.ElapsedMilliseconds -lt $timeoutMs)) {
    Start-Sleep -Milliseconds 500
}

if (-not $hostProc.HasExited) {
    Write-Warning "[TIMEOUT] Host did not finish within $TimeoutSeconds seconds. Terminating..."
    Stop-Process -Id $hostProc.Id -Force -ErrorAction SilentlyContinue
}

$hostProc.WaitForExit()
$clientProc.WaitForExit()

$hostExit = $hostProc.ExitCode
$clientExit = $clientProc.ExitCode

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "                TEST RESULTS & AUDIT                      " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

Write-Host "Host Exit Code:   $hostExit" -ForegroundColor $(if ($hostExit -eq 0) { "Green" } else { "Red" })
Write-Host "Client Exit Code: $clientExit" -ForegroundColor $(if ($clientExit -eq 0) { "Green" } else { "Red" })

$hostOutput = if (Test-Path -LiteralPath $hostLogOut) { Get-Content -LiteralPath $hostLogOut -Raw } else { "" }
$clientOutput = if (Test-Path -LiteralPath $clientLogOut) { Get-Content -LiteralPath $clientLogOut -Raw } else { "" }

$hostSuccess = $hostOutput.Contains("All 500 interleaved reliable messages delivered in exact sequence!")
$clientSuccess = $clientOutput.Contains("Client Adversarial Run Succeeded.")

if ($hostSuccess) {
    Write-Host "[PASS] Host verified 100% in-order, loss-free reliable message delivery!" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Host did not verify complete reliable message sequence." -ForegroundColor Red
}

if ($clientSuccess) {
    Write-Host "[PASS] Client confirmed successful handshake, connection, and transmission!" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Client did not report successful run." -ForegroundColor Red
}

Remove-Item -LiteralPath $hostLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $hostLogErr -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogErr -ErrorAction SilentlyContinue

if ($hostExit -eq 0 -and $clientExit -eq 0 -and $hostSuccess -and $clientSuccess) {
    Write-Host "`n[SUCCESS] MULTIPROCESS ADVERSARIAL HARNESS CERTIFIED!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n[FAILURE] MULTIPROCESS HARNESS FAILED!" -ForegroundColor Red
    exit 1
}
