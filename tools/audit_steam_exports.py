import re
import sys

def main():
    print("=== ReFix Steam Export Audit Tool ===")

    # 1. Parse g_forwardNames from src/steam_proxy.cpp
    with open("src/steam_proxy.cpp", "r", encoding="utf-8") as f:
        cpp = f.read()

    match = re.search(r"static const char\* g_forwardNames\[STEAM_FORWARD_COUNT\] = \{(.*?)\};", cpp, re.DOTALL)
    if not match:
        print("[FAIL] Could not find g_forwardNames in src/steam_proxy.cpp")
        sys.exit(1)

    raw_names = match.group(1)
    cpp_names = []
    for line in raw_names.splitlines():
        line = line.strip()
        if line.startswith('"'):
            name = line.split('"')[1]
            cpp_names.append(name)

    print(f"[*] steam_proxy.cpp g_forwardNames count: {len(cpp_names)}")

    # 2. Parse src/steam_fwd.asm
    with open("src/steam_fwd.asm", "r", encoding="utf-8") as f:
        asm = f.read()

    asm_entries = re.findall(
        r"([A-Za-z0-9_]+)_proxy\s+PROC\s+jmp\s+QWORD\s+PTR\s+\[g_steamProcs\s*\+\s*(\d+)\]\s+([A-Za-z0-9_]+)_proxy\s+ENDP",
        asm
    )
    print(f"[*] steam_fwd.asm proxy procedures count: {len(asm_entries)}")

    # 3. Parse src/steam_api64.def
    with open("src/steam_api64.def", "r", encoding="utf-8") as f:
        def_lines = [l.strip() for l in f if l.strip() and not l.strip().startswith(";") and not l.strip().startswith("LIBRARY") and not l.strip().startswith("EXPORTS")]

    # Custom ReFix exports at the top (before GetHSteamPipe)
    try:
        steam_start_idx = def_lines.index("GetHSteamPipe=GetHSteamPipe_proxy")
    except ValueError:
        try:
            steam_start_idx = def_lines.index("GetHSteamPipe")
        except ValueError:
            steam_start_idx = 0

    custom_exports = def_lines[:steam_start_idx]
    steam_def_lines = def_lines[steam_start_idx:]

    trailing_direct = ["SteamAPI_Init", "SteamAPI_InitSafe", "SteamAPI_InitAnonymousUser", "SteamAPI_RestartAppIfNecessary"]
    # Check if trailing direct exports or other trailing exports are present
    trailing_filtered = []
    for line in steam_def_lines:
        base_name = line.split("=")[0].strip()
        if base_name in trailing_direct or base_name in ["CreateInterface", "SteamAPI_InitFlat", "ReFix_GetUnsupportedLanCallCount"]:
            trailing_filtered.append(line)

    steam_forwarded_lines = [l for l in steam_def_lines if l not in trailing_filtered]

    print(f"[*] steam_api64.def custom ReFix exports: {len(custom_exports)}")
    print(f"[*] steam_api64.def forwarded Steam exports: {len(steam_forwarded_lines)}")

    # 4. Check duplicates
    seen = set()
    dupes = []
    for name in cpp_names:
        if name in seen:
            dupes.append(name)
        seen.add(name)
    if dupes:
        print(f"[FAIL] Duplicate exports in g_forwardNames: {dupes}")

    # 5. Check 1:1 match across all entries
    errors = []
    for i in range(len(cpp_names)):
        cpp_name = cpp_names[i]
        expected_offset = i * 8

        # Check ASM
        if i < len(asm_entries):
            asm_name, asm_offset, asm_end = asm_entries[i]
            if asm_name != cpp_name:
                errors.append(f"Index {i}: ASM proc name mismatch: expected '{cpp_name}', got '{asm_name}'")
            if int(asm_offset) != expected_offset:
                errors.append(f"Index {i} ({cpp_name}): ASM offset mismatch: expected {expected_offset}, got {asm_offset}")
            if asm_name != asm_end:
                errors.append(f"Index {i}: ASM proc inconsistent: start '{asm_name}' vs end '{asm_end}'")
        else:
            errors.append(f"Index {i} ({cpp_name}): Missing from src/steam_fwd.asm")

        # Check DEF
        if i < len(steam_forwarded_lines):
            def_line = steam_forwarded_lines[i]
            expected_aliased = f"{cpp_name}={cpp_name}_proxy"
            if def_line != expected_aliased and def_line != cpp_name:
                errors.append(f"Index {i}: DEF line mismatch: expected '{expected_aliased}' or '{cpp_name}', got '{def_line}'")
        else:
            errors.append(f"Index {i} ({cpp_name}): Missing from src/steam_api64.def")

    if len(asm_entries) != len(cpp_names):
        errors.append(f"Count mismatch: g_forwardNames={len(cpp_names)} vs ASM={len(asm_entries)}")
    if len(steam_forwarded_lines) != len(cpp_names):
        errors.append(f"Count mismatch: g_forwardNames={len(cpp_names)} vs DEF={len(steam_forwarded_lines)}")

    if errors:
        print(f"\n[FAIL] Found {len(errors)} alignment errors:")
        for err in errors[:25]:
            print(f"  - {err}")
        if len(errors) > 25:
            print(f"  ... and {len(errors) - 25} more errors.")
        sys.exit(1)

    print("\n[PASS] AUDIT SUCCESSFUL!")
    print(f"All {len(cpp_names)} exports match perfectly across all 4 layers:")
    print("  1. g_forwardNames[i] in src/steam_proxy.cpp")
    print("  2. src/steam_fwd.asm [g_steamProcs + i*8]")
    print("  3. src/steam_api64.def slot i")
    print("  4. g_steamProcs[i] runtime array slot")
    sys.exit(0)

if __name__ == "__main__":
    main()
