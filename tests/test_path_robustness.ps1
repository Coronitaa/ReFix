# =============================================================================
# ReFix - Comprehensive Path Robustness & Failure Mode Test Suite
# Tests: Special characters (!, &, (), [], ^%, spaces, Unicode/Japanese/Emoji),
#        -LiteralPath compliance, array unwrapping, non-zero exit codes.
# =============================================================================
param(
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Continue"
$rootDir = (Get-Item -LiteralPath "$PSScriptRoot\..").FullName
$binDir = Join-Path $rootDir "bin"
$autoDeployBat = Join-Path $rootDir "AutoDeploy.bat"
$uninstallBat = Join-Path $rootDir "Uninstall_ReFix.bat"
$deployHelperPs1 = Join-Path $binDir "deploy_helper.ps1"
$detectPs1 = Join-Path $binDir "detect_game.ps1"

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "       ReFix Path Robustness & Deployment Test Harness           " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "Root Directory: $rootDir"
Write-Host "Bin Directory:  $binDir"

# Verification of base requirements
if (-not (Test-Path -LiteralPath $binDir)) {
    Write-Host "[FAIL] bin directory not found at $binDir" -ForegroundColor Red
    exit 1
}

$testTempRoot = Join-Path $env:TEMP "ReFix_PathTests_$([Guid]::NewGuid().ToString().Substring(0,8))"
[System.IO.Directory]::CreateDirectory($testTempRoot) | Out-Null
Write-Host "Using temporary test sandbox: $testTempRoot`n"

# Helper to create mock game folder
function New-MockGame {
    param(
        [string]$FolderPath,
        [string]$ExeName = "Game.exe"
    )
    if (Test-Path -LiteralPath $FolderPath) {
        Remove-Item -LiteralPath $FolderPath -Recurse -Force -ErrorAction SilentlyContinue
    }
    [System.IO.Directory]::CreateDirectory($FolderPath) | Out-Null
    
    # Create mock 64-bit dummy Game.exe (minimal valid PE header or dummy binary)
    $exePath = Join-Path $FolderPath $ExeName
    $dummyBytes = [byte[]](0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00) # MZ dummy
    [System.IO.File]::WriteAllBytes($exePath, $dummyBytes)

    # Copy genuine dummy valve dll to simulate realistic existing Steam game
    $valveDll = Join-Path $binDir "valve\steam_api64.dll"
    if (Test-Path -LiteralPath $valveDll) {
        Copy-Item -LiteralPath $valveDll -Destination (Join-Path $FolderPath "steam_api64.dll") -Force
    } else {
        [System.IO.File]::WriteAllBytes((Join-Path $FolderPath "steam_api64.dll"), $dummyBytes)
    }
}

# Test Cases Array: Name -> Folder
$testCases = @(
    "Test_PlainGame",
    "Test Game With Spaces",
    "Test Game ! Exclamation Mark",
    "Test Game [v1.0] Square Brackets",
    "Test Game (2026) Parentheses",
    "Test Game & Ampersand",
    "Test Game ^ Caret and % Percent",
    "Test Game 🎮 Unicode and 日本語"
)

$testResults = @()

foreach ($tcName in $testCases) {
    Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow
    Write-Host "RUNNING TEST CASE: $tcName" -ForegroundColor Yellow
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
