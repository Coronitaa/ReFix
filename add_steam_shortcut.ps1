param(
    [string]$GameName,
    [string]$ExePath,
    [string]$TargetDir
)

function Get-SteamPath {
    try {
        $reg = Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -ErrorAction Stop
        if ($reg.SteamPath -and (Test-Path -LiteralPath $reg.SteamPath)) { return $reg.SteamPath }
    } catch {}
    try {
        $regLM = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop
        if ($regLM.InstallPath -and (Test-Path -LiteralPath $regLM.InstallPath)) { return $regLM.InstallPath }
    } catch {}
    try {
        $regLM64 = Get-ItemProperty -Path "HKLM:\SOFTWARE\Valve\Steam" -ErrorAction Stop
        if ($regLM64.InstallPath -and (Test-Path -LiteralPath $regLM64.InstallPath)) { return $regLM64.InstallPath }
    } catch {}
    $common = @(
        "${env:ProgramFiles(x86)}\Steam",
        "${env:ProgramFiles}\Steam",
        "C:\Steam", "D:\Steam", "E:\Steam", "F:\Steam", "G:\Steam"
    )
    foreach ($p in $common) {
        if ($p -and (Test-Path -LiteralPath $p)) { return $p }
    }
    return $null
}

function Get-Crc32([string]$str) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($str)
    $crc = 0xffffffff
    foreach ($b in $bytes) {
        $crc = $crc -bxor $b
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -eq 1) {
                $crc = ($crc -shr 1) -bxor 0xedb88320
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return ($crc -bxor 0xffffffff)
}

function Build-VdfEntry([int]$index, [string]$appName, [string]$exePath, [string]$startDir) {
    $crc = Get-Crc32 ($exePath + $appName)
    $appId = [uint32](([int64]$crc -bor 0x80000000L) -band 0xFFFFFFFFL)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms

    # Entry index header
    $bw.Write([byte]0)
    $bw.Write([System.Text.Encoding]::UTF8.GetBytes($index.ToString()))
    $bw.Write([byte]0)

    # Helper functions for VDF fields
    $writeString = {
        param($key, $val)
        $bw.Write([byte]1)
        $bw.Write([System.Text.Encoding]::UTF8.GetBytes($key))
        $bw.Write([byte]0)
        $bw.Write([System.Text.Encoding]::UTF8.GetBytes($val))
        $bw.Write([byte]0)
    }

    $writeInt = {
        param($key, [uint32]$val)
        $bw.Write([byte]2)
        $bw.Write([System.Text.Encoding]::UTF8.GetBytes($key))
        $bw.Write([byte]0)
        $bw.Write([uint32]$val)
    }

    &$writeInt "appid" $appId
    &$writeString "AppName" $appName
    &$writeString "Exe" "`"$exePath`""
    &$writeString "StartDir" "`"$startDir`""
    &$writeString "icon" ""
    &$writeString "ShortcutPath" ""
    &$writeString "LaunchOptions" ""
    &$writeInt "IsHidden" 0
    &$writeInt "AllowDesktopConfig" 1
    &$writeInt "AllowOverlay" 1
    &$writeInt "OpenVR" 0
    &$writeInt "Devkit" 0
    &$writeString "DevkitGameID" ""
    &$writeInt "DevkitOverrideAppID" 0
    &$writeInt "LastPlayTime" 0
    &$writeString "FlatpakAppID" ""

    # Tags sub-section (empty)
    $bw.Write([byte]0)
    $bw.Write([System.Text.Encoding]::UTF8.GetBytes("tags"))
    $bw.Write([byte]0)
    $bw.Write([byte]8)  # close tags sub-section

    # Close this entry
    $bw.Write([byte]8)

    $bytes = $ms.ToArray()
    $bw.Close()
    $ms.Close()
    return $bytes
}

if ($TargetDir) {
    $TargetDir = $TargetDir.Trim('"').Trim("'").TrimEnd('\')
}

if ($TargetDir -and (Test-Path -LiteralPath $TargetDir)) {
    $currDir = (Get-Item -LiteralPath $TargetDir).FullName
} else {
    if ($PSScriptRoot -and (Test-Path -LiteralPath $PSScriptRoot)) {
        $currDir = (Get-Item -LiteralPath $PSScriptRoot).FullName
    } else {
        $currDir = (Get-Location).Path
    }
}

$iniPath = Join-Path $currDir "ReFix.ini"

if (-not $GameName) {
    if (Test-Path -LiteralPath $iniPath) {
        $line = Get-Content -LiteralPath $iniPath | Where-Object { $_ -match "^GameName=" } | Select-Object -First 1
        if ($line) { $GameName = ($line -split "=", 2)[1].Trim() }
    }
}
if (-not $GameName) { $GameName = (Get-Item -LiteralPath $currDir).Name }
if (-not $GameName.EndsWith("[ReFix]") -and -not $GameName.EndsWith("[ʀᴇꜰɪx]")) { $displayName = "$GameName [ReFix]" } else { $displayName = $GameName }

# Strict exclusion pattern for system / helper / tool executables
$excludePattern = "(?i)^(powershell|pwsh|cmd|python|pythonw|git|dotnet|createdump|UnityCrash|CrashReport|Install|unins|setup|updater|select_folder|depot|steam-manifest|vcredist|dxsetup|prereq|UE4Prereq)"

if ($ExePath) {
    $ExePath = $ExePath.Trim('"').Trim("'")
}

if (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath)) {
    $ExePath = $null

    # 1. Search top-level files in current target directory using -LiteralPath
    $topExes = Get-ChildItem -LiteralPath $currDir -File -ErrorAction SilentlyContinue | 
        Where-Object { $_.Extension -eq ".exe" -and $_.Name -notmatch $excludePattern }

    if ($topExes) {
        $matched = $topExes | Where-Object { $GameName -and $_.BaseName -like "*$GameName*" } | Select-Object -First 1
        if ($matched) {
            $ExePath = $matched.FullName
        } else {
            $ExePath = ($topExes | Sort-Object Length -Descending | Select-Object -First 1).FullName
        }
    }

    # 2. Check parent directory if inside a Binaries\Win64 or subfolder
    if (-not $ExePath) {
        $parentItem = (Get-Item -LiteralPath $currDir).Parent
        if ($parentItem) {
            $parentExes = Get-ChildItem -LiteralPath $parentItem.FullName -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Extension -eq ".exe" -and $_.Name -notmatch $excludePattern }
            if ($parentExes) {
                $ExePath = ($parentExes | Sort-Object Length -Descending | Select-Object -First 1).FullName
            }
        }
    }

    # 3. Look for Unreal Engine Shipping executable (*Win64-Shipping.exe)
    if (-not $ExePath) {
        $shippingExes = Get-ChildItem -LiteralPath $currDir -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -eq ".exe" -and $_.Name -match "Shipping\.exe$" -and $_.Name -notmatch $excludePattern }
        if ($shippingExes) {
            $ExePath = ($shippingExes | Sort-Object Length -Descending | Select-Object -First 1).FullName
        }
    }

    # 4. Fallback recursive search sorted by largest file size
    if (-not $ExePath) {
    $exeCandidates = Get-ChildItem -Path $currDir -Filter "*.exe" -File -Recurse -ErrorAction SilentlyContinue | 
        Where-Object { $_.Name -notmatch "(?i)(UnityCrash|crashpad|crash_handler|CrashReport|Install|unins|Steamless|generate_interfaces|Configurar_Firewall|helper|updater)" } |
        Sort-Object Length -Descending
    if ($exeCandidates) { $ExePath = $exeCandidates[0].FullName }
    }
}

if (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath)) {
    Write-Host "[ERROR] Could not find valid game executable in $currDir." -ForegroundColor Red
    exit 1
}

$startDir = [System.IO.Path]::GetDirectoryName($ExePath) + "\"

Write-Host "[+] Target Game Name : $displayName" -ForegroundColor Cyan
Write-Host "[+] Executable Path  : $ExePath" -ForegroundColor Cyan
Write-Host "[+] Working Directory: $startDir" -ForegroundColor Cyan

$steamDir = Get-SteamPath

if (-not $steamDir) {
    Write-Host "[ERROR] Could not locate Steam installation." -ForegroundColor Red
    exit 1
}

$userData = Join-Path $steamDir "userdata"

if (-not (Test-Path -LiteralPath $userData)) {
    Write-Host "[ERROR] Steam userdata directory not found at $userData" -ForegroundColor Red
    exit 1
}

$userFolders = Get-ChildItem -LiteralPath $userData | Where-Object { $_.PSIsContainer -and $_.Name -match "^\d+$" }

foreach ($user in $userFolders) {
    $vdfPath = Join-Path $user.FullName "config\shortcuts.vdf"
    $configDir = Join-Path $user.FullName "config"
    if (-not (Test-Path -LiteralPath $configDir)) { New-Item -ItemType Directory -Path $configDir -Force | Out-Null }

    if (Test-Path -LiteralPath $vdfPath) {
        $raw = [System.IO.File]::ReadAllBytes($vdfPath)
        $backupPath = "$vdfPath.refix_backup"
        if (-not (Test-Path -LiteralPath $backupPath)) {
            [System.IO.File]::Copy($vdfPath, $backupPath)
            Write-Host "[+] Created backup: shortcuts.vdf.refix_backup" -ForegroundColor DarkGray
        }
    } else {
        $raw = [byte[]](0x00, 0x73, 0x68, 0x6f, 0x72, 0x74, 0x63, 0x75, 0x74, 0x73, 0x00, 0x08, 0x08)
    }

    $displayBytes = [System.Text.Encoding]::UTF8.GetBytes($displayName)
    $exeBytes = [System.Text.Encoding]::UTF8.GetBytes($ExePath)

    # Check if entry with this display name exists AND points to exact target exe
    $alreadyAdded = $false
    for ($i = 0; $i -lt ($raw.Length - $displayBytes.Length); $i++) {
        $match = $true
        for ($j = 0; $j -lt $displayBytes.Length; $j++) {
            if ($raw[$i + $j] -ne $displayBytes[$j]) { $match = $false; break }
        }
        if ($match) {
            $windowEnd = [Math]::Min($raw.Length, $i + 256)
            for ($k = $i; $k -lt ($windowEnd - $exeBytes.Length); $k++) {
                $exeMatch = $true
                for ($l = 0; $l -lt $exeBytes.Length; $l++) {
                    if ($raw[$k + $l] -ne $exeBytes[$l]) { $exeMatch = $false; break }
                }
                if ($exeMatch) { $alreadyAdded = $true; break }
            }
            break
        }
    }

    if (-not $alreadyAdded) {
        $idx = 0
        $appNameHeader = [System.Text.Encoding]::UTF8.GetBytes([char]1 + "AppName" + [char]0)
        for ($i = 0; $i -lt ($raw.Length - $appNameHeader.Length); $i++) {
            $match = $true
            for ($j = 0; $j -lt $appNameHeader.Length; $j++) {
                if ($raw[$i + $j] -ne $appNameHeader[$j]) { $match = $false; break }
            }
            if ($match) { $idx++ }
        }

        $entryBytes = Build-VdfEntry $idx $displayName $ExePath $startDir

        $endPos = $raw.Length
        if ($endPos -ge 2 -and $raw[$endPos - 1] -eq 0x08 -and $raw[$endPos - 2] -eq 0x08) {
            $endPos -= 2
        }

        $newRaw = New-Object byte[] ($endPos + $entryBytes.Length + 2)
        [Array]::Copy($raw, 0, $newRaw, 0, $endPos)
        [Array]::Copy($entryBytes, 0, $newRaw, $endPos, $entryBytes.Length)
        $newRaw[$newRaw.Length - 2] = 0x08
        $newRaw[$newRaw.Length - 1] = 0x08

        [System.IO.File]::WriteAllBytes($vdfPath, $newRaw)
        Write-Host "[+] Updated Steam shortcuts for user profile: $($user.Name)" -ForegroundColor Green
    } else {
        Write-Host "[+] Shortcut '$displayName' is up to date for profile $($user.Name)" -ForegroundColor Yellow
    }
}
