#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT/build/test_linux_deploy"

echo "=== Testing Linux AutoDeploy.sh ==="

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# 1. Setup mock 64-bit Unity game
cat << 'EOF' > "$TEST_DIR/Game.exe"
MZ_MOCK_GAME_EXE
EOF
cat << 'EOF' > "$TEST_DIR/steam_api64.dll"
ORIGINAL_VALVE_STEAM_API64
EOF

echo "[Test 1] Deploy Option 1: ReFix Online + LAN (lan)"
bash "$ROOT/AutoDeploy.sh" "$TEST_DIR" lan 480 refix

if [ ! -f "$TEST_DIR/steam_api64_original.dll" ]; then
    echo "FAILED: steam_api64_original.dll backup not found!"
    exit 1
fi
if [ ! -f "$TEST_DIR/.refix_launch_options.txt" ]; then
    echo "FAILED: .refix_launch_options.txt not found!"
    exit 1
fi
grep -q "WINEDLLOVERRIDES" "$TEST_DIR/.refix_launch_options.txt" || {
    echo "FAILED: WINEDLLOVERRIDES missing from launch options!"
    exit 1
}
grep -q "RealAppId = 480" "$TEST_DIR/ReFix.ini" || {
    echo "FAILED: RealAppId not written to ReFix.ini!"
    exit 1
}
echo "Test 1 PASSED: Option 1 (lan) deployed successfully."

echo "[Test 2] Deploy Option 2: ReFix Offline LAN (offline)"
bash "$ROOT/AutoDeploy.sh" "$TEST_DIR" offline 480 refix
grep -q "Enabled = 0" "$TEST_DIR/ReFix.ini" || {
    echo "FAILED: SteamLink not disabled for offline LAN!"
    exit 1
}
echo "Test 2 PASSED: Option 2 (offline) deployed successfully."

echo "[Test 3] Deploy Option 3: ReFix Online Legacy (online)"
bash "$ROOT/AutoDeploy.sh" "$TEST_DIR" online 480 refix
if [ ! -f "$TEST_DIR/winmm.dll" ]; then
    echo "FAILED: winmm.dll not deployed for online legacy!"
    exit 1
fi
grep -q "winmm" "$TEST_DIR/.refix_launch_options.txt" || {
    echo "FAILED: winmm override missing in launch options!"
    exit 1
}
echo "Test 3 PASSED: Option 3 (online) deployed successfully."

echo "[Test 4] Uninstall"
bash "$ROOT/AutoDeploy.sh" --uninstall "$TEST_DIR"
if [ ! -f "$TEST_DIR/steam_api64.dll" ]; then
    echo "FAILED: steam_api64.dll not restored on uninstall!"
    exit 1
fi
if grep -q "ReFix" "$TEST_DIR/steam_api64.dll"; then
    echo "FAILED: Restored steam_api64.dll is still ReFix!"
    exit 1
fi
if [ -f "$TEST_DIR/steam_api64_original.dll" ]; then
    echo "FAILED: Backup not removed on uninstall!"
    exit 1
fi
for left in winmm.dll .refix_launch_options.txt ReFix.ini steam_appid.txt; do
    if [ -e "$TEST_DIR/$left" ]; then
        echo "FAILED: $left left behind after uninstall!"
        exit 1
    fi
done
echo "Test 4 PASSED: Uninstall cleaned up and restored original game files."

echo "[Test 5] Game's own winmm.dll / winhttp.dll survive deploy + uninstall (EAC game)"
rm -rf "$TEST_DIR"; mkdir -p "$TEST_DIR/EasyAntiCheat"
echo MZ_MOCK_GAME_EXE > "$TEST_DIR/Game.exe"
echo ORIGINAL_VALVE_STEAM_API64 > "$TEST_DIR/steam_api64.dll"
echo GAME_OWN_WINMM > "$TEST_DIR/winmm.dll"
echo GAME_OWN_WINHTTP > "$TEST_DIR/winhttp.dll"
echo EAC > "$TEST_DIR/EasyAntiCheat/EasyAntiCheat_x64.dll"
bash "$ROOT/AutoDeploy.sh" "$TEST_DIR" online 480 refix
grep -q ReFix "$TEST_DIR/winmm.dll" || { echo "FAILED: ReFix winmm.dll not deployed!"; exit 1; }
grep -q ReFix "$TEST_DIR/winhttp.dll" || { echo "FAILED: ReFix winhttp.dll not deployed!"; exit 1; }
bash "$ROOT/AutoDeploy.sh" --uninstall "$TEST_DIR"
grep -qx GAME_OWN_WINMM "$TEST_DIR/winmm.dll" || { echo "FAILED: game's winmm.dll not restored!"; exit 1; }
grep -qx GAME_OWN_WINHTTP "$TEST_DIR/winhttp.dll" || { echo "FAILED: game's winhttp.dll not restored!"; exit 1; }
grep -qx ORIGINAL_VALVE_STEAM_API64 "$TEST_DIR/steam_api64.dll" || { echo "FAILED: steam_api64.dll not the original!"; exit 1; }
echo "Test 5 PASSED: game's own proxies restored, ReFix ones removed."

echo "=== ALL LINUX AUTODEPLOY TESTS PASSED 100% ==="
