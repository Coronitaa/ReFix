import re

def main():
    # 1. Parse g_forwardNames from src/steam_proxy.cpp
    with open('src/steam_proxy.cpp', 'r', encoding='utf-8') as f:
        cpp_code = f.read()

    m = re.search(r'g_forwardNames\[STEAM_FORWARD_COUNT\]\s*=\s*\{([^}]+)\};', cpp_code, re.DOTALL)
    if not m:
        raise RuntimeError("Could not find g_forwardNames in src/steam_proxy.cpp")

    forward_names = []
    for line in m.group(1).splitlines():
        line = line.strip()
        if line.startswith('"'):
            name = line.split('"')[1]
            forward_names.append(name)

    print(f"Loaded {len(forward_names)} forward names from src/steam_proxy.cpp")

    # 2. Generate src/steam_fwd_x86.asm
    asm_lines = [
        "; =============================================================================",
        "; ReFix - steam_api.dll x86 Forwarding Trampolines (AUTO-GENERATED)",
        "; =============================================================================",
        f"; Forwards {len(forward_names)} exports via jump table (g_steamProcs).",
        "; =============================================================================",
        "",
        ".386",
        ".model flat, c",
        "",
        ".data",
        "EXTERN g_steamProcs:DWORD",
        "",
        ".code",
        ""
    ]

    for i, name in enumerate(forward_names):
        offset = i * 4
        asm_lines.append(f"{name}_proxy PROC")
        asm_lines.append(f"    jmp DWORD PTR [g_steamProcs + {offset}]")
        asm_lines.append(f"{name}_proxy ENDP")

    asm_lines.append("")
    asm_lines.append("PUBLIC SafeUnsupportedExportStub")
    asm_lines.append("SafeUnsupportedExportStub PROC")
    asm_lines.append("    xor eax, eax")
    asm_lines.append("    ret")
    asm_lines.append("SafeUnsupportedExportStub ENDP")
    asm_lines.append("")
    asm_lines.append("END")
    asm_lines.append("")

    with open('src/steam_fwd_x86.asm', 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(asm_lines))

    print(f"Generated src/steam_fwd_x86.asm with {len(forward_names)} trampolines.")

    # 3. Generate src/steam_api_x86.def from src/steam_api64.def
    with open('src/steam_api64.def', 'r', encoding='utf-8') as f:
        def64 = f.read()

    def_lines = []
    for line in def64.splitlines():
        trimmed = line.strip()
        if trimmed.startswith('LIBRARY'):
            def_lines.append('LIBRARY steam_api')
        else:
            def_lines.append(line)

    with open('src/steam_api_x86.def', 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(def_lines) + '\n')

    print("Generated src/steam_api_x86.def based on src/steam_api64.def.")

if __name__ == '__main__':
    main()
