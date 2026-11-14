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
"C:\Program Files\Git\bin\bash.exe" "$ROOT/AutoDeploy.sh" "$TEST_DIR" lan 480 refix

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
"C:\Program Files\Git\bin\bash.exe" "$ROOT/AutoDeploy.sh" "$TEST_DIR" offline 480 refix
grep -q "Enabled = 0" "$TEST_DIR/ReFix.ini" || {
    echo "FAILED: SteamLink not disabled for offline LAN!"
    exit 1
}
echo "Test 2 PASSED: Option 2 (offline) deployed successfully."

echo "[Test 3] Deploy Option 3: ReFix Online Legacy (online)"
"C:\Program Files\Git\bin\bash.exe" "$ROOT/AutoDeploy.sh" "$TEST_DIR" online 480 refix
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
"C:\Program Files\Git\bin\bash.exe" "$ROOT/AutoDeploy.sh" --uninstall "$TEST_DIR"
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
echo "Test 4 PASSED: Uninstall cleaned up and restored original game files."

echo "=== ALL LINUX AUTODEPLOY TESTS PASSED 100% ==="
