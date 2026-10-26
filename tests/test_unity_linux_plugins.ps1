# =============================================================================
# ReFix deploy test: Unity game whose *_Data\Plugins\x86 / x86_64 hold only Linux plugins (.so)
# A 32-bit Windows Unity game loads *_Data\Plugins\steam_api.dll; deploy_helper must patch that folder and must
# not drop proxies into the Linux folders, where no genuine Valve DLL exists (post-deploy verification failed
# there for Overcooked! 2).
# =============================================================================
param([string]$RepoRoot = "")

if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$binDir = Join-Path $RepoRoot "bin"
$deployHelperPs1 = Join-Path $binDir "deploy_helper.ps1"

function New-PeStub([string]$Path, [int]$Size, [uint16]$Machine) {
    $bytes = New-Object byte[] $Size
    $bytes[0] = 0x4D; $bytes[1] = 0x5A; $bytes[0x3C] = 0x80
    $bytes[0x80] = 0x50; $bytes[0x81] = 0x45
    $bytes[0x84] = [byte]($Machine -band 0xFF); $bytes[0x85] = [byte]($Machine -shr 8)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

$gameDir = Join-Path $env:TEMP ("refix_unity_linux_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$plugins = Join-Path $gameDir "Game_Data\Plugins"
New-Item -ItemType Directory -Force -Path (Join-Path $plugins "x86"), (Join-Path $plugins "x86_64") | Out-Null
New-PeStub (Join-Path $gameDir "Game.exe") 4096 0x014C
New-PeStub (Join-Path $plugins "steam_api.dll") 2048 0x014C   # genuine 32-bit Steam API stand-in
foreach ($d in "x86", "x86_64") {
    Set-Content -LiteralPath (Join-Path $plugins "$d\libsteam_api.so") -Value "ELF" -Encoding ASCII
}

$output = & powershell -NoProfile -ExecutionPolicy Bypass -File $deployHelperPs1 -TargetDir $gameDir -BinDir $binDir `
    -ExeDir $gameDir -EngineType "Unity" -OnlineMode "valve" -GameName "Unity Linux Plugins" -UserName "TestPlayer" `
    -RealAppId "480" -MaskAppId "480" -Language "english" -DLCs "all" -DLCMode "all" -ListenPort "47584" 2>&1
$exitCode = $LASTEXITCODE

$proxy32Size = (Get-Item -LiteralPath (Join-Path $binDir "x86\steam_api.dll")).Length
$failures = @()
if ($exitCode -ne 0) { $failures += "deploy_helper exit code $exitCode" }
$deployed = Join-Path $plugins "steam_api.dll"
if (-not (Test-Path -LiteralPath $deployed) -or (Get-Item -LiteralPath $deployed).Length -ne $proxy32Size) {
    $failures += "ReFix x86 proxy not deployed to Game_Data\Plugins"
}
if (-not (Test-Path -LiteralPath (Join-Path $plugins "steam_api_o.dll"))) { $failures += "genuine x86 DLL not kept as steam_api_o.dll" }
foreach ($d in "x86", "x86_64") {
    $stray = Get-ChildItem -LiteralPath (Join-Path $plugins $d) -Filter "steam_api*" -File -ErrorAction SilentlyContinue
    if ($stray) { $failures += "Linux plugin folder $d received $($stray.Name -join ', ')" }
}
if (Test-Path -LiteralPath (Join-Path $gameDir "steam_api64.dll")) { $failures += "64-bit proxy deployed for a 32-bit-only game" }

Remove-Item -LiteralPath $gameDir -Recurse -Force -ErrorAction SilentlyContinue
if ($failures.Count -gt 0) {
    $output | Select-Object -Last 15 | ForEach-Object { Write-Host "    $_" }
    $failures | ForEach-Object { Write-Host "  [FAIL] $_" -ForegroundColor Red }
    exit 1
}
Write-Host "  [PASS] Unity Linux plugin folders left alone; 32-bit Plugins\steam_api.dll patched" -ForegroundColor Green
exit 0
