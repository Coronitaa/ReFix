param(
    [Parameter(Mandatory=$true)]
    [string]$TargetDir,

    [string]$BinDir = "",
    [string]$Action = "install",   # "install" | "uninstall" | "status"
    [string]$AppId = "",
    [string]$GameName = ""
)

$target = $TargetDir.TrimEnd('\').Trim('"')
if (-not (Test-Path $target)) {
    Write-Host "[ERROR] Target directory does not exist: $target" -ForegroundColor Red
    exit 1
}

if (-not $BinDir) {
    $BinDir = Join-Path $PSScriptRoot "..\bin"
}
$smokeToolsDir = Join-Path $BinDir "tools\SmokeAPI"
if (-not (Test-Path $smokeToolsDir)) {
    $smokeToolsDir = Join-Path $PSScriptRoot "tools\SmokeAPI"
}

$smoke64 = Join-Path $smokeToolsDir "smoke_api64.dll"
$smoke32 = Join-Path $smokeToolsDir "smoke_api32.dll"

# --- 1. Locate all directories with steam_api*.dll ---
$targetDlls = Get-ChildItem -Path $target -Filter "steam_api*.dll" -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notlike "*_o.dll*" -and $_.Name -notlike "*_valve.dll*" -and $_.Name -notlike "*_goldberg.dll*" }

$targetDirs = @()
foreach ($dll in $targetDlls) {
    if ($targetDirs -notcontains $dll.DirectoryName) {
        $targetDirs += $dll.DirectoryName
    }
}

if ($targetDirs.Count -eq 0) {
    $targetDirs += $target
}

# --- Action: STATUS ---
if ($Action -eq "status") {
    Write-Host "=== DLC Unlocker Status for $target ===" -ForegroundColor Cyan
    foreach ($dir in $targetDirs) {
        $has64Bak = Test-Path (Join-Path $dir "steam_api64_o.dll")
        $has32Bak = Test-Path (Join-Path $dir "steam_api_o.dll")
        $hasCreamIni = Test-Path (Join-Path $dir "cream_api.ini")
        $hasSmokeJson = Test-Path (Join-Path $dir "SmokeAPI.config.json")
        $isInstalled = ($has64Bak -or $has32Bak) -and ($hasCreamIni -or $hasSmokeJson)
        Write-Host "Directory: $dir"
        Write-Host "  - Installed: $isInstalled"
        Write-Host "  - Backup x64: $has64Bak | Backup x86: $has32Bak"
        Write-Host "  - cream_api.ini: $hasCreamIni | SmokeAPI.config.json: $hasSmokeJson"
    }
    exit 0
}

# --- Action: UNINSTALL ---
if ($Action -eq "uninstall") {
    Write-Host "=== Desinstalando DLC Unlocker en: $target ===" -ForegroundColor Yellow
    $restoredCount = 0
    foreach ($dir in $targetDirs) {
        $bak64 = Join-Path $dir "steam_api64_o.dll"
        $api64 = Join-Path $dir "steam_api64.dll"
        if (Test-Path $bak64) {
            if (Test-Path $api64) { Remove-Item -Path $api64 -Force }
            Move-Item -Path $bak64 -Destination $api64 -Force
            Write-Host "  [OK] Restaurado original steam_api64.dll en $dir" -ForegroundColor Green
            $restoredCount++
        }

        $bak32 = Join-Path $dir "steam_api_o.dll"
        $api32 = Join-Path $dir "steam_api.dll"
        if (Test-Path $bak32) {
            if (Test-Path $api32) { Remove-Item -Path $api32 -Force }
            Move-Item -Path $bak32 -Destination $api32 -Force
            Write-Host "  [OK] Restaurado original steam_api.dll en $dir" -ForegroundColor Green
            $restoredCount++
        }

        foreach ($cfg in @("cream_api.ini", "SmokeAPI.config.json", "SmokeAPI.json", "SmokeAPI.log", "SmokeAPI.cache.json")) {
            $cfgPath = Join-Path $dir $cfg
            if (Test-Path $cfgPath) {
                Remove-Item -Path $cfgPath -Force -ErrorAction SilentlyContinue
                Write-Host "  [OK] Eliminado archivo de configuracion: $cfg" -ForegroundColor Green
            }
        }
    }

    if ($restoredCount -gt 0) {
        Write-Host "`n[EXITO] DLC Unlocker desinstalado y DLLs originales de Steam restauradas." -ForegroundColor Green
    } else {
        Write-Host "`n[AVISO] No se encontraron respaldos de DLC Unlocker para restaurar." -ForegroundColor Yellow
    }
    exit 0
}

# --- Action: INSTALL ---
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " [ReFix DLC Unlocker] Target: $target" -ForegroundColor Cyan
Write-Host " [ReFix DLC Unlocker] Engine: BLUESTAR SmokeAPI Backend" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Resolve AppId if missing
$finalAppId = $AppId
if (-not $finalAppId -or $finalAppId -eq "0" -or $finalAppId -eq "480") {
    $appIdFile = Get-ChildItem -Path $target -Filter "steam_appid.txt" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($appIdFile) {
        $content = Get-Content $appIdFile.FullName -Raw -ErrorAction SilentlyContinue
        if ($content -match "([0-9]{3,9})") { $finalAppId = $Matches[1].Trim() }
    }
}
if (-not $finalAppId -or $finalAppId -eq "0") {
    $finalAppId = "480"
}

# 2. Fetch DLC list from Steam Store API
$dlcList = @{}
if ($finalAppId -and $finalAppId -ne "480") {
    Write-Host "[1/3] Consultando catalogo oficial de DLCs en Steam Store API (AppID: $finalAppId)..." -ForegroundColor Cyan
    try {
        $apiUrl = "https://store.steampowered.com/api/appdetails?appids=$finalAppId"
        $client = New-Object System.Net.Http.HttpClient
        $client.Timeout = [TimeSpan]::FromSeconds(5)
        $response = $client.GetStringAsync($apiUrl).GetAwaiter().GetResult()
        $json = ConvertFrom-Json $response
        $appData = $json.$finalAppId.data
        if ($appData -and $appData.dlc) {
            $dlcIds = $appData.dlc
            Write-Host "  [OK] Encontrados $($dlcIds.Count) DLC(s) oficiales para AppID $finalAppId." -ForegroundColor Green
            foreach ($dId in $dlcIds) {
                $dlcList["$dId"] = "DLC $dId"
            }
        }
    } catch {
        Write-Host "  [AVISO] No se pudo consultar Steam Store API (Modo offline / sin conexion). Se aplicara modo universal unlock_all." -ForegroundColor Yellow
    }
}

# 3. Deploy SmokeAPI and generate configs in all relevant directories
Write-Host "[2/3] Desplegando binarios de SmokeAPI y creando respaldos..." -ForegroundColor Cyan
$deployedCount = 0

foreach ($dir in $targetDirs) {
    $api64 = Join-Path $dir "steam_api64.dll"
    $bak64 = Join-Path $dir "steam_api64_o.dll"
    $has64 = (Test-Path $api64) -or (Test-Path $smoke64)

    if (Test-Path $api64) {
        if (-not (Test-Path $bak64)) {
            Copy-Item -Path $api64 -Destination $bak64 -Force
            Write-Host "  [OK] Creado respaldo: steam_api64.dll -> steam_api64_o.dll en $dir" -ForegroundColor Green
        }
    }
    if (Test-Path $smoke64) {
        Copy-Item -Path $smoke64 -Destination $api64 -Force
        Write-Host "  [OK] Desplegado SmokeAPI 64-bit -> $api64" -ForegroundColor Green
        $deployedCount++
    }

    $api32 = Join-Path $dir "steam_api.dll"
    $bak32 = Join-Path $dir "steam_api_o.dll"
    if (Test-Path $api32) {
        if (-not (Test-Path $bak32)) {
            Copy-Item -Path $api32 -Destination $bak32 -Force
            Write-Host "  [OK] Creado respaldo: steam_api.dll -> steam_api_o.dll en $dir" -ForegroundColor Green
        }
        if (Test-Path $smoke32) {
            Copy-Item -Path $smoke32 -Destination $api32 -Force
            Write-Host "  [OK] Desplegado SmokeAPI 32-bit -> $api32" -ForegroundColor Green
            $deployedCount++
        }
    }

    # 4. Generate cream_api.ini (BLUESTAR standard)
    Write-Host "[3/3] Generando cream_api.ini y SmokeAPI.config.json..." -ForegroundColor Cyan
    $creamIniLines = @(
        "; =============================================================================",
        "; Generated by ReFix DLC Unlocker (BLUESTAR Engine)",
        "; =============================================================================",
        "[steam]",
        "appid = $finalAppId",
        "unlockall = true",
        "orgapi = steam_api_o.dll",
        "orgapi64 = steam_api64_o.dll",
        "extraprotection = false",
        "forceoffline = false",
        "",
        "[steam_misc]",
        "disableuserinterface = false",
        "",
        "[dlc]"
    )
    foreach ($k in $dlcList.Keys) {
        $creamIniLines += "$k = $($dlcList[$k])"
    }
    $creamIniPath = Join-Path $dir "cream_api.ini"
    [System.IO.File]::WriteAllText($creamIniPath, ($creamIniLines -join "`r`n") + "`r`n", [System.Text.Encoding]::UTF8)
    Write-Host "  [OK] Generado $creamIniPath" -ForegroundColor Green

    # 5. Generate SmokeAPI.config.json
    $smokeJsonPath = Join-Path $dir "SmokeAPI.config.json"
    $smokeConfig = @{
        logging = $false
        unlock_all = $true
        dlcs = $dlcList
    }
    $jsonStr = ConvertTo-Json $smokeConfig -Depth 5
    [System.IO.File]::WriteAllText($smokeJsonPath, $jsonStr + "`r`n", [System.Text.Encoding]::UTF8)
    Write-Host "  [OK] Generado $smokeJsonPath" -ForegroundColor Green
}

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host " DLC UNLOCKER INSTALADO CON EXITO" -ForegroundColor Green
Write-Host " Juego: $GameName  |  AppID: $finalAppId" -ForegroundColor Green
Write-Host " Modo: Desbloqueo Universal Automatico (SmokeAPI / CreamAPI)" -ForegroundColor Green
if ($dlcList.Count -gt 0) {
    Write-Host " DLCs Oficiales Detectados: $($dlcList.Count)" -ForegroundColor Green
}
Write-Host "============================================================" -ForegroundColor Green
