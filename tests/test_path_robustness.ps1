# =============================================================================
# ReFix Automated Path Robustness and Transport Pipeline Test Harness
# Tests 100% path safety for special characters: '!', '&', '()', '[]', '^', '%', spaces, Unicode
# =============================================================================
[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [switch]$KeepArtifacts = $false
)

if (-not $RepoRoot) {
    if ($PSScriptRoot) {
        $RepoRoot = (Split-Path -Parent $PSScriptRoot)
    } else {
        $RepoRoot = (Get-Location).Path
    }
}
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path

$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "     ReFix Path Robustness & Transport Pipeline Test Suite     " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "Repository Root: $RepoRoot" -ForegroundColor Gray

$binDir = Join-Path $RepoRoot "bin"
$autoDeployBat = Join-Path $RepoRoot "AutoDeploy.bat"
$uninstallBat = Join-Path $RepoRoot "Uninstall_ReFix.bat"
$detectPs1 = Join-Path $binDir "detect_game.ps1"
$deployHelperPs1 = Join-Path $binDir "deploy_helper.ps1"

# Verify prerequisites
$prereqs = @($autoDeployBat, $detectPs1, $deployHelperPs1, (Join-Path $binDir "steam_api64.dll"), (Join-Path $binDir "winmm.dll"))
foreach ($p in $prereqs) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Error "Prerequisite missing: $p"
        exit 1
    }
}

$testRunId = [Guid]::NewGuid().ToString("N").Substring(0, 8)
$testTempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "refix_robustness_$testRunId"
[System.IO.Directory]::CreateDirectory($testTempRoot) | Out-Null
Write-Host "Test Workspace: $testTempRoot" -ForegroundColor Gray
Write-Host ""

$nihongo = [System.Text.Encoding]::UTF8.GetString([byte[]]@(0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E))
$unicodeGameName = "Test Game " + [char]::ConvertFromUtf32(0x1F3AE) + " " + $nihongo

$testCases = @(
    "Test Game !",
    "Test Game & Cool",
    "Test Game (2026)",
    "Test Game [v1.0]",
    "Test Game ^ 100%",
    $unicodeGameName
)

$testResults = @()

function New-MockGame {
    param([string]$FolderPath)
    if (Test-Path -LiteralPath $FolderPath) {
        Remove-Item -LiteralPath $FolderPath -Recurse -Force -ErrorAction SilentlyContinue
    }
    [System.IO.Directory]::CreateDirectory($FolderPath) | Out-Null

    # Create dummy 64-bit EXE
    $dummyExe = Join-Path $FolderPath "Game.exe"
    # Create simple PE header with 64-bit machine type 0x8664
    $peBytes = New-Object byte[] 1024
    $peBytes[0] = 0x4D # 'M'
    $peBytes[1] = 0x5A # 'Z'
    $peBytes[0x3C] = 0x80 # PE offset
    $peBytes[0x80] = 0x50 # 'P'
    $peBytes[0x81] = 0x45 # 'E'
    $peBytes[0x84] = 0x64 # 0x8664 x64
    $peBytes[0x85] = 0x86
    [System.IO.File]::WriteAllBytes($dummyExe, $peBytes)

    # Create mock original steam_api64.dll (distinct size from ReFix dll so backup triggers)
    $dummyDll = Join-Path $FolderPath "steam_api64.dll"
    $dllBytes = New-Object byte[] 2048
    $dllBytes[0] = 0x4D
    $dllBytes[1] = 0x5A
    $dllBytes[0x3C] = 0x80
    $dllBytes[0x80] = 0x50
    $dllBytes[0x81] = 0x45
    $dllBytes[0x84] = 0x64
    $dllBytes[0x85] = 0x86
    [System.IO.File]::WriteAllBytes($dummyDll, $dllBytes)
}

foreach ($tcName in $testCases) {
    Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow
    Write-Host "Running Test Case: '$tcName'" -ForegroundColor Yellow
    Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow

    $gameDir = Join-Path $testTempRoot $tcName
    New-MockGame -FolderPath $gameDir

    # --- Phase 1: detect_game.ps1 preservation check ---
    $detectOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $detectPs1 -TargetDir "$gameDir" 2>&1
    $detectDict = @{}
    foreach ($line in ($detectOutput -split "`r?`n")) {
        if ($line -match "^([^=]+)=(.*)$") {
            $detectDict[$matches[1].Trim()] = $matches[2].Trim()
        }
    }

    $detectedExeDir = $detectDict["EXE_DIR"]
    $detectedExePath = $detectDict["GAME_EXE_PATH"]

    $p1Success = ($detectedExeDir -eq $gameDir) -and ($detectedExePath -eq (Join-Path $gameDir "Game.exe"))
    if ($p1Success) {
        Write-Host "  [PASS] detect_game.ps1: 100% path preserved: '$detectedExeDir'" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] detect_game.ps1: Expected '$gameDir', got '$detectedExeDir'" -ForegroundColor Red
    }

    # --- Phase 2: deploy_helper.ps1 direct execution ---
    $deployHelpOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $deployHelperPs1 `
        -TargetDir "$gameDir" `
        -BinDir "$binDir" `
        -ExeDir "$gameDir" `
        -EngineType "Native" `
        -OnlineMode "valve" `
        -GameName "$tcName" `
        -UserName "TestPlayer" `
        -RealAppId "480" `
        -MaskAppId "480" `
        -Language "english" `
        -DLCs "all" `
        -DLCMode "all" `
        -ListenPort "47584" 2>&1
    $deployHelpCode = $LASTEXITCODE

    $p2Deployed = (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64.dll")) -and `
                  (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64_valve.dll")) -and `
                  (Test-Path -LiteralPath (Join-Path $gameDir "steam_appid.txt")) -and `
                  (Test-Path -LiteralPath (Join-Path $gameDir "ReFix.ini")) -and `
                  (Test-Path -LiteralPath (Join-Path $gameDir "winmm.dll")) -and `
                  ($deployHelpCode -eq 0)

    if ($p2Deployed) {
        Write-Host "  [PASS] deploy_helper.ps1: All components deployed and verified" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] deploy_helper.ps1: ExitCode=$deployHelpCode, Files missing" -ForegroundColor Red
    }

    # --- Phase 3: AutoDeploy.bat full end-to-end execution ---
    # Reset mock game folder
    New-MockGame -FolderPath $gameDir

    $env:REFIX_NON_INTERACTIVE = "1"
    $autoDeployOutput = & $autoDeployBat "$gameDir" 2>&1
    $autoDeployCode = $LASTEXITCODE

    $p3BatDeployed = (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64.dll")) -and `
                     (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64_valve.dll")) -and `
                     (Test-Path -LiteralPath (Join-Path $gameDir "steam_appid.txt")) -and `
                     (Test-Path -LiteralPath (Join-Path $gameDir "ReFix.ini")) -and `
                     (Test-Path -LiteralPath (Join-Path $gameDir "winmm.dll")) -and `
                     ($autoDeployCode -eq 0)

    # Check that ReFix.ini is non-empty and contains UNAE structure
    $iniValid = $false
    if (Test-Path -LiteralPath (Join-Path $gameDir "ReFix.ini")) {
        $iniText = [System.IO.File]::ReadAllText((Join-Path $gameDir "ReFix.ini"))
        if ($iniText.Contains("[UNAE]") -or $iniText.Contains("[Matchmaking]")) {
            $iniValid = $true
        }
    }

    if ($p3BatDeployed -and $iniValid) {
        Write-Host "  [PASS] AutoDeploy.bat: E2E deployment successful (ExitCode=0)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] AutoDeploy.bat: ExitCode=$autoDeployCode, p3BatDeployed=$p3BatDeployed, iniValid=$iniValid" -ForegroundColor Red
        Write-Host "  Output:" -ForegroundColor DarkGray
        Write-Host ($autoDeployOutput -join "`n") -ForegroundColor DarkGray
    }

    # --- Phase 4: Uninstall_ReFix.bat restore check ---
    $uninstOutput = & $uninstallBat "$gameDir" 2>&1
    $uninstCode = $LASTEXITCODE

    $p4Restored = (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64.dll")) -and `
                  (-not (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64_valve.dll"))) -and `
                  (-not (Test-Path -LiteralPath (Join-Path $gameDir "winmm.dll"))) -and `
                  (-not (Test-Path -LiteralPath (Join-Path $gameDir "ReFix.ini"))) -and `
                  (-not (Test-Path -LiteralPath (Join-Path $gameDir "steam_appid.txt"))) -and `
                  ($uninstCode -eq 0)

    if ($p4Restored) {
        Write-Host "  [PASS] Uninstall_ReFix.bat: Restored original files cleanly" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Uninstall_ReFix.bat: Restoration failed (ExitCode=$uninstCode)" -ForegroundColor Red
    }

    $caseSuccess = $p1Success -and $p2Deployed -and $p3BatDeployed -and $iniValid -and $p4Restored
    $testResults += [PSCustomObject]@{
        CaseName     = $tcName
        PathDetect   = if ($p1Success) { "PASS" } else { "FAIL" }
        HelperDeploy = if ($p2Deployed) { "PASS" } else { "FAIL" }
        BatchDeploy  = if ($p3BatDeployed -and $iniValid) { "PASS" } else { "FAIL" }
        Uninstall    = if ($p4Restored) { "PASS" } else { "FAIL" }
        Overall      = if ($caseSuccess) { "PASS" } else { "FAIL" }
    }
}

# --- Phase 5: Failure Mode & Exit Code Enforcement Tests (FASE 5) ---
Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow
Write-Host "Running FASE 5: Failure Handling & Non-Zero Exit Code Tests" -ForegroundColor Yellow
Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow

# Test 5A: AutoDeploy with non-existent directory must return exit code 1 and no SUCCESS
$nonExistentDir = Join-Path $testTempRoot "DoesNotExist_XYZ_123"
$failBatOutput = & $autoDeployBat "$nonExistentDir" 2>&1
$failBatCode = $LASTEXITCODE
$noSuccessInFail = -not (($failBatOutput -join " ") -match "\[SUCCESS\]|DEPLOY COMPLETE")
$test5APass = ($failBatCode -ne 0) -and $noSuccessInFail

if ($test5APass) {
    Write-Host "  [PASS] Failure Test 5A (Non-existent directory): ExitCode=$failBatCode, No [SUCCESS]" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Failure Test 5A: ExitCode=$failBatCode, Output contains false success!" -ForegroundColor Red
}

# Test 5B: deploy_helper with invalid bin dir must return exit code 1
$failHelpOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $deployHelperPs1 `
    -TargetDir "$testTempRoot" `
    -BinDir "C:\NonExistent_Bin_Refix_Dir" `
    -ExeDir "$testTempRoot" `
    -EngineType "Native" `
    -OnlineMode "valve" 2>&1
$failHelpCode = $LASTEXITCODE
$test5BPass = ($failHelpCode -ne 0)

if ($test5BPass) {
    Write-Host "  [PASS] Failure Test 5B (Missing binaries): ExitCode=$failHelpCode" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Failure Test 5B: Expected non-zero exit code, got $failHelpCode" -ForegroundColor Red
}

$testResults += [PSCustomObject]@{
    CaseName     = "FASE 5: Error Handling (Invalid Path)"
    PathDetect   = "N/A"
    HelperDeploy = "N/A"
    BatchDeploy  = if ($test5APass) { "PASS" } else { "FAIL" }
    Uninstall    = "N/A"
    Overall      = if ($test5APass -and $test5BPass) { "PASS" } else { "FAIL" }
}

# Clean up
if (-not $KeepArtifacts) {
    Remove-Item -LiteralPath $testTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "                      TEST RESULTS SUMMARY                       " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
$testResults | Format-Table -AutoSize

$allPassed = ($testResults | Where-Object { $_.Overall -ne "PASS" }).Count -eq 0
if ($allPassed) {
    Write-Host "ALL TESTS PASSED SUCCESSFULLY! (100% Path Robustness Verified)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "SOME TESTS FAILED! Check detailed logs above." -ForegroundColor Red
    exit 1
}
