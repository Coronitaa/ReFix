# test_multi_instance_mesh.ps1 - Automated 3-Instance Mesh Verification Harness
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$BinPath = "",
    [Parameter(Mandatory = $false)]
    [int]$TimeoutSeconds = 25
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BinPath)) {
    $BinPath = Join-Path -Path $PSScriptRoot -ChildPath "..\bin\test_lan_concurrency_contract.exe"
}

$resolvedBin = (Resolve-Path -LiteralPath $BinPath).Path
if (-not (Test-Path -LiteralPath $resolvedBin)) {
    Write-Error "Binary not found at: $resolvedBin"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   REFIX 3-INSTANCE MESH TEST HARNESS (PORTS 47584-47586) " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "[*] Target Binary:    $resolvedBin"
Write-Host "[*] Timeout:          $TimeoutSeconds seconds"
Write-Host "[*] Target Nodes:     3 Instances (Localhost Mesh)"

$logNode1Out = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node1.log"
$logNode1Err = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node1_err.log"
$logNode2Out = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node2.log"
$logNode2Err = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node2_err.log"
$logNode3Out = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node3.log"
$logNode3Err = Join-Path -Path $PSScriptRoot -ChildPath "mesh_node3_err.log"

Remove-Item -LiteralPath $logNode1Out -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $logNode1Err -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $logNode2Out -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $logNode2Err -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $logNode3Out -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $logNode3Err -ErrorAction SilentlyContinue

function Start-MeshNodeProcess {
    param([int]$NodeId, [int]$ListenPort, [string]$SteamId, [string]$LogOut, [string]$LogErr)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $envPrefix = "set REFIX_LISTEN_PORT=$ListenPort && set REFIX_STEAM_ID=$SteamId && "
    $args = "--mesh-node $NodeId --port $ListenPort --steam-id $SteamId"
    $psi.Arguments = "/c `"$envPrefix`"$resolvedBin`" $args > `"$LogOut`" 2> `"$LogErr`"`""
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    return [System.Diagnostics.Process]::Start($psi)
}

Write-Host "[*] Launching Node 1 on port 47584 (SteamID: 76561198000000001)..." -ForegroundColor Yellow
$procNode1 = Start-MeshNodeProcess -NodeId 1 -ListenPort 47584 -SteamId "76561198000000001" -LogOut $logNode1Out -LogErr $logNode1Err

Start-Sleep -Milliseconds 400

Write-Host "[*] Launching Node 2 on port 47585 (SteamID: 76561198000000002)..." -ForegroundColor Yellow
$procNode2 = Start-MeshNodeProcess -NodeId 2 -ListenPort 47585 -SteamId "76561198000000002" -LogOut $logNode2Out -LogErr $logNode2Err

Start-Sleep -Milliseconds 400

Write-Host "[*] Launching Node 3 on port 47586 (SteamID: 76561198000000003)..." -ForegroundColor Yellow
$procNode3 = Start-MeshNodeProcess -NodeId 3 -ListenPort 47586 -SteamId "76561198000000003" -LogOut $logNode3Out -LogErr $logNode3Err

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$timeoutMs = $TimeoutSeconds * 1000

while ((-not $procNode1.HasExited -or -not $procNode2.HasExited -or -not $procNode3.HasExited) -and ($stopwatch.ElapsedMilliseconds -lt $timeoutMs)) {
    Start-Sleep -Milliseconds 500
}

if (-not $procNode1.HasExited) {
    Write-Warning "[TIMEOUT] Node 1 did not complete within $TimeoutSeconds seconds. Killing..."
    Stop-Process -Id $procNode1.Id -Force -ErrorAction SilentlyContinue
}
if (-not $procNode2.HasExited) {
    Write-Warning "[TIMEOUT] Node 2 did not complete within $TimeoutSeconds seconds. Killing..."
    Stop-Process -Id $procNode2.Id -Force -ErrorAction SilentlyContinue
}
if (-not $procNode3.HasExited) {
    Write-Warning "[TIMEOUT] Node 3 did not complete within $TimeoutSeconds seconds. Killing..."
    Stop-Process -Id $procNode3.Id -Force -ErrorAction SilentlyContinue
}

$procNode1.WaitForExit()
$procNode2.WaitForExit()
$procNode3.WaitForExit()

$exit1 = $procNode1.ExitCode
$exit2 = $procNode2.ExitCode
$exit3 = $procNode3.ExitCode

$out1 = if (Test-Path -LiteralPath $logNode1Out) { Get-Content -LiteralPath $logNode1Out -Raw } else { "" }
$out2 = if (Test-Path -LiteralPath $logNode2Out) { Get-Content -LiteralPath $logNode2Out -Raw } else { "" }
$out3 = if (Test-Path -LiteralPath $logNode3Out) { Get-Content -LiteralPath $logNode3Out -Raw } else { "" }

Write-Host "`n--- NODE 1 OUTPUT ---"
Write-Host $out1
Write-Host "--- NODE 2 OUTPUT ---"
Write-Host $out2
Write-Host "--- NODE 3 OUTPUT ---"
Write-Host $out3

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "           3-INSTANCE MESH AUDIT & VERIFICATION           " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$success1 = $out1.Contains("[MESH_NODE_OK] Node 1 successfully participated in 3-instance mesh without collisions!")
$success2 = $out2.Contains("[MESH_NODE_OK] Node 2 successfully participated in 3-instance mesh without collisions!")
$success3 = $out3.Contains("[MESH_NODE_OK] Node 3 successfully participated in 3-instance mesh without collisions!")

$noCollisions = (-not $out1.Contains("WSAEADDRINUSE")) -and (-not $out2.Contains("WSAEADDRINUSE")) -and (-not $out3.Contains("WSAEADDRINUSE"))

Write-Host "Node 1 Exit Code: $exit1 (Success marker: $success1)" -ForegroundColor $(if ($exit1 -eq 0 -and $success1) { "Green" } else { "Red" })
Write-Host "Node 2 Exit Code: $exit2 (Success marker: $success2)" -ForegroundColor $(if ($exit2 -eq 0 -and $success2) { "Green" } else { "Red" })
Write-Host "Node 3 Exit Code: $exit3 (Success marker: $success3)" -ForegroundColor $(if ($exit3 -eq 0 -and $success3) { "Green" } else { "Red" })
Write-Host "Zero Port Collisions (WSAEADDRINUSE): $noCollisions" -ForegroundColor $(if ($noCollisions) { "Green" } else { "Red" })

if ($exit1 -eq 0 -and $exit2 -eq 0 -and $exit3 -eq 0 -and $success1 -and $success2 -and $success3 -and $noCollisions) {
    Write-Host "`n[OVERALL RESULT] 3-INSTANCE MESH TEST PASSED (Exit Code 0)" -ForegroundColor Green
    exit 0
} else {
    Write-Error "`n[OVERALL RESULT] 3-INSTANCE MESH TEST FAILED (Exit Code 1)"
    exit 1
}
