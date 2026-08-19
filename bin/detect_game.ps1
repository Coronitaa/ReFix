param(
    [Parameter(Mandatory=$true)]
    [string]$TargetDir
)

$target = $TargetDir.TrimEnd('\').Trim('"')
if (-not (Test-Path $target)) {
    Write-Output "ERROR: Directory not found"
    exit 1
}

$ignorePatterns = @(
    "*crashpad*", "*UnityCrashHandler*", "*unins*", "*setup*", "*updater*",
    "*vcredist*", "*dxsetup*", "*dotnet*", "*prereq*", "*F10.exe*",
    "*.unpacked.exe*", "*.steamstub.exe*", "*.original.exe*", "*redist*"
)

# 1. Detect Engine Type
$engineType = "Unity"
if (Test-Path (Join-Path $target "Engine")) {
    $engineType = "Unreal"
}
$pck = Get-ChildItem -Path $target -Filter "*.pck" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($pck) { $engineType = "Godot" }
$godotDll = Get-ChildItem -Path $target -Filter "libgodot*.dll" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($godotDll) { $engineType = "Godot" }

# 2. Find all steam_api64.dll or steam_api.dll locations
$steamDlls = Get-ChildItem -Path $target -Filter "steam_api64*.dll" -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notlike "*valve*" -and $_.Name -notlike "*goldberg*" -and $_.Name -notlike "*_o.dll*" }

$candidateExes = @()

# First priority: executables beside steam_api64.dll
foreach ($dll in $steamDlls) {
    $dllDir = $dll.DirectoryName
    $exesInDir = Get-ChildItem -Path $dllDir -Filter "*.exe" -File -ErrorAction SilentlyContinue
    foreach ($e in $exesInDir) {
        $isIgnored = $false
        foreach ($pat in $ignorePatterns) {
            if ($e.Name -like $pat) { $isIgnored = $true; break }
        }
        if (-not $isIgnored -and ($candidateExes.FullName -notcontains $e.FullName)) {
            $candidateExes += $e
        }
    }
}

# Second priority: Unreal Shipping executables
$unrealExes = Get-ChildItem -Path $target -Filter "*-Win64-Shipping.exe" -Recurse -File -ErrorAction SilentlyContinue
foreach ($e in $unrealExes) {
    if ($candidateExes.FullName -notcontains $e.FullName) {
        $candidateExes += $e
    }
}

# Third priority: all other executables in tree
$allExes = Get-ChildItem -Path $target -Filter "*.exe" -Recurse -File -ErrorAction SilentlyContinue
foreach ($e in $allExes) {
    $isIgnored = $false
    foreach ($pat in $ignorePatterns) {
        if ($e.Name -like $pat) { $isIgnored = $true; break }
    }
    if (-not $isIgnored -and ($candidateExes.FullName -notcontains $e.FullName)) {
        $candidateExes += $e
    }
}

# Select best executable:
# Priority: Ship > Shipping > Win64 > beside steam_api64.dll > root exe > first found
$chosenExe = $null

foreach ($c in $candidateExes) {
    if ($c.DirectoryName -like "*Ship*" -or $c.DirectoryName -like "*Shipping*") {
        $chosenExe = $c
        break
    }
}

if (-not $chosenExe) {
    foreach ($c in $candidateExes) {
        if ($c.DirectoryName -like "*Binaries\Win64*" -or $c.DirectoryName -like "*bin*") {
            $chosenExe = $c
            break
        }
    }
}

if (-not $chosenExe -and $candidateExes.Count -gt 0) {
    # Check if any exe is in target root
    foreach ($c in $candidateExes) {
        if ($c.DirectoryName -eq $target) {
            $chosenExe = $c
            break
        }
    }
}

if (-not $chosenExe -and $candidateExes.Count -gt 0) {
    $chosenExe = $candidateExes[0]
}

# Determine EXE_DIR and GAME_NAME
$exeDir = $target
$gameExePath = ""
$gameName = (Get-Item $target).Name

if ($chosenExe) {
    $gameExePath = $chosenExe.FullName
    $exeDir = $chosenExe.DirectoryName
    
    $cleanName = $chosenExe.BaseName
    if ($cleanName -like "*-Win64-Shipping") {
        $cleanName = $cleanName.Replace("-Win64-Shipping", "")
    }
    if ($cleanName -ne "Game" -and $cleanName -ne "Launch" -and $cleanName -ne "Main" -and $cleanName -ne "CrashReportClient") {
        $gameName = $cleanName
    }
}

if ($gameName -like "*_Windows") {
    $gameName = $gameName.Substring(0, $gameName.Length - 8)
} elseif ($gameName -like "*_Win64") {
    $gameName = $gameName.Substring(0, $gameName.Length - 6)
}

# 3. Detect Original AppID across all potential files
$detectedAppId = ""

# A. Scan for steam_appid.txt
$appIdFiles = Get-ChildItem -Path $target -Filter "steam_appid.txt" -Recurse -File -ErrorAction SilentlyContinue
foreach ($f in $appIdFiles) {
    $content = (Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue)
    if ($content -match "([0-9]{3,9})") {
        $found = $Matches[1].Trim()
        if ($found -ne "480" -or -not $detectedAppId) {
            $detectedAppId = $found
            if ($found -ne "480") { break }
        }
    }
}

# B. Scan for ReFix.ini RealAppId
if (-not $detectedAppId -or $detectedAppId -eq "480") {
    $reFixInis = Get-ChildItem -Path $target -Filter "ReFix.ini" -Recurse -File -ErrorAction SilentlyContinue
    foreach ($ini in $reFixInis) {
        $lines = Get-Content $ini.FullName -ErrorAction SilentlyContinue
        foreach ($line in $lines) {
            if ($line -match "^\s*RealAppId\s*=\s*([0-9]+)") {
                $found = $Matches[1].Trim()
                if ($found -and $found -ne "0" -and $found -ne "480") {
                    $detectedAppId = $found
                    break
                }
            }
        }
        if ($detectedAppId -and $detectedAppId -ne "480") { break }
    }
}

# C. Scan for steam_settings\configs.app.ini
if (-not $detectedAppId -or $detectedAppId -eq "480") {
    $appInis = Get-ChildItem -Path $target -Filter "configs.app.ini" -Recurse -File -ErrorAction SilentlyContinue
    foreach ($ini in $appInis) {
        $lines = Get-Content $ini.FullName -ErrorAction SilentlyContinue
        foreach ($line in $lines) {
            if ($line -match "^\s*appid\s*=\s*([0-9]+)") {
                $found = $Matches[1].Trim()
                if ($found -and $found -ne "0" -and $found -ne "480") {
                    $detectedAppId = $found
                    break
                }
            }
        }
        if ($detectedAppId -and $detectedAppId -ne "480") { break }
    }
}

# 4. Check for presence of Steam and EOS
$hasSteam = $false
if ($steamDlls.Count -gt 0) { $hasSteam = $true }

$hasEos = $false
$eosFiles = Get-ChildItem -Path $target -Filter "*EOSSDK*" -Recurse -File -ErrorAction SilentlyContinue
if ($eosFiles.Count -gt 0) { $hasEos = $true }
$redpointFolders = Get-ChildItem -Path $target -Filter "*Redpoint*" -Recurse -Directory -ErrorAction SilentlyContinue
if ($redpointFolders.Count -gt 0) { $hasEos = $true }

# Output key=value pairs for easy parsing in CMD
Write-Output "ENGINE_TYPE=$engineType"
Write-Output "EXE_DIR=$exeDir"
Write-Output "GAME_EXE_PATH=$gameExePath"
Write-Output "GAME_NAME=$gameName"
Write-Output "DETECTED_APPID=$detectedAppId"
Write-Output "HAS_STEAM=$hasSteam"
Write-Output "HAS_EOS=$hasEos"
