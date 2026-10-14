# test_steam_sockets_multiprocess.ps1 - Automated Dual-Process ISteamNetworkingSockets Integration Harness
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$BinPath = "",
    [Parameter(Mandatory = $false)]
    [int]$TimeoutSeconds = 30,
    [Parameter(Mandatory = $false)]
    [switch]$DisableFallbacks = $true
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BinPath)) {
    $BinPath = Join-Path -Path $PSScriptRoot -ChildPath "..\bin\test_steam_sockets_lancore.exe"
}

$resolvedBin = (Resolve-Path -LiteralPath $BinPath).Path
if (-not (Test-Path -LiteralPath $resolvedBin)) {
    Write-Error "Binary not found at: $resolvedBin"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   REFIX STEAM SOCKETS MULTIPROCESS HARNESS (CHANNEL 250) " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "[*] Target Binary:    $resolvedBin"
Write-Host "[*] Timeout:          $TimeoutSeconds seconds"
Write-Host "[*] Canonical Only:   $DisableFallbacks (REFIX_DISABLE_LEGACY_FALLBACKS)"

$hostLogOut = Join-Path -Path $PSScriptRoot -ChildPath "sockets_host.log"
$hostLogErr = Join-Path -Path $PSScriptRoot -ChildPath "sockets_host_err.log"
$clientLogOut = Join-Path -Path $PSScriptRoot -ChildPath "sockets_client.log"
$clientLogErr = Join-Path -Path $PSScriptRoot -ChildPath "sockets_client_err.log"

Remove-Item -LiteralPath $hostLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $hostLogErr -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogOut -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $clientLogErr -ErrorAction SilentlyContinue

function Start-SocketsProcess {
    param([string]$Mode, [int]$ListenPort, [string]$SteamId, [string]$LogOut, [string]$LogErr)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $envPrefix = "set REFIX_LISTEN_PORT=$ListenPort && set REFIX_STEAM_ID=$SteamId && set SteamAppId=480 && "
    if ($DisableFallbacks) { $envPrefix += "set REFIX_DISABLE_LEGACY_FALLBACKS=1 && " }
    $psi.Arguments = "/c `"$envPrefix`"$resolvedBin`" $Mode > `"$LogOut`" 2> `"$LogErr`"`""
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    return [System.Diagnostics.Process]::Start($psi)
}

Write-Host "[*] Launching HOST process on port 47584..." -ForegroundColor Yellow
$hostProc = Start-SocketsProcess -Mode "--host" -ListenPort 47584 -SteamId "76561198000000001" -LogOut $hostLogOut -LogErr $hostLogErr

# Allow Host 1.5s to bind, start transport, and open listen socket
Start-Sleep -Milliseconds 1500

if ($hostProc.HasExited) {
    Write-Error "[FAIL] Host process terminated prematurely with exit code: $($hostProc.ExitCode)"
    if (Test-Path -LiteralPath $hostLogErr) { Get-Content -LiteralPath $hostLogErr }
    exit 1
}

Write-Host "[*] Launching CLIENT process on port 47586..." -ForegroundColor Yellow
$clientProc = Start-SocketsProcess -Mode "--client" -ListenPort 47586 -SteamId "76561198000000002" -LogOut $clientLogOut -LogErr $clientLogErr

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
Write-Host "Host Exit Code:   $hostExit"
Write-Host "Client Exit Code: $clientExit"

$failed = $false

if ($hostExit -ne 0) {
    Write-Host "[FAIL] Host exit code is non-zero: $hostExit" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "[PASS] Host exited cleanly with code 0" -ForegroundColor Green
}

if ($clientExit -ne 0) {
    Write-Host "[FAIL] Client exit code is non-zero: $clientExit" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "[PASS] Client exited cleanly with code 0" -ForegroundColor Green
}

if ($hostOutput -match "\[HOST_ACCEPTED\]") {
    Write-Host "[PASS] Host accepted incoming client connection via ISteamNetworkingSockets" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [HOST_ACCEPTED] in host output" -ForegroundColor Red
    $failed = $true
}

if ($hostOutput -match "\[HOST_RELIABLE_VERIFIED\]") {
    Write-Host "[PASS] Host received and verified reliable message over Channel 250" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [HOST_RELIABLE_VERIFIED] in host output" -ForegroundColor Red
    $failed = $true
}

if ($hostOutput -match "\[HOST_FRAGMENT_VERIFIED\]") {
    Write-Host "[PASS] Host received and reassembled 4096-byte fragmented message over Channel 250" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [HOST_FRAGMENT_VERIFIED] in host output" -ForegroundColor Red
    $failed = $true
}

if ($clientOutput -match "\[CLIENT_CONNECTED\]") {
    Write-Host "[PASS] Client reached Connected state after RfxsConnectAccept" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [CLIENT_CONNECTED] in client output" -ForegroundColor Red
    $failed = $true
}

if ($clientOutput -match "\[CLIENT_PONG_VERIFIED\]") {
    Write-Host "[PASS] Client received and verified bidirectional reliable pong from Host" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [CLIENT_PONG_VERIFIED] in client output" -ForegroundColor Red
    $failed = $true
}

if ($clientOutput -match "\[CLIENT_FRAGMENT_VERIFIED\]") {
    Write-Host "[PASS] Client received and reassembled 4096-byte fragmented message from Host" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Missing [CLIENT_FRAGMENT_VERIFIED] in client output" -ForegroundColor Red
    $failed = $true
}

if ($failed) {
    Write-Host "`n[FAIL] DUAL-PROCESS STEAM SOCKETS INTEGRATION HARNESS FAILED!" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n[PASS] ALL E2E DUAL-PROCESS STEAM SOCKETS INTEGRATION TESTS PASSED!" -ForegroundColor Green
    exit 0
}
