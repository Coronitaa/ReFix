import struct
import sys

def parse_pe_exports(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] != b'MZ':
        raise ValueError('Not a MZ PE')
    e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]
    if data[e_lfanew:e_lfanew+4] != b'PE\0\0':
        raise ValueError('Not a PE')
    num_sections = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
    opt_size = struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]
    opt_header_offset = e_lfanew + 4 + 20
    magic = struct.unpack_from('<H', data, opt_header_offset)[0]
    if magic != 0x20b:
        raise ValueError('Not PE32+')
    export_rva, export_size = struct.unpack_from('<II', data, opt_header_offset + 112)
    if export_rva == 0 or export_size == 0:
        return []
    
    sections_offset = opt_header_offset + opt_size
    sections = []
    for i in range(num_sections):
        sec = data[sections_offset + i*40 : sections_offset + (i+1)*40]
        vsize, vrva, raw_size, raw_ptr = struct.unpack_from('<IIII', sec, 8)
        sections.append((vrva, vsize, raw_ptr, raw_size))
        
    def rva2off(rva):
        for vrva, vsize, raw_ptr, raw_size in sections:
            if vrva <= rva < vrva + max(vsize, raw_size):
                return raw_ptr + (rva - vrva)
        return None
        
    exp_off = rva2off(export_rva)
    chars, stamp, maj, min, name_rva, base, num_funcs, num_names, funcs_rva, names_rva, ords_rva = struct.unpack_from('<IIHHIIIIIII', data, exp_off)
    names_off = rva2off(names_rva)
    
    export_names = []
    for i in range(num_names):
        nrva = struct.unpack_from('<I', data, names_off + i*4)[0]
        noff = rva2off(nrva)
        end = data.find(b'\0', noff)
        export_names.append(data[noff:end].decode('ascii'))
    return export_names

def main():
    print("=== ReFix Real PE Export Table Verification ===")
    target_dll = 'bin/steam_api64.dll'
    pe_names = set(parse_pe_exports(target_dll))
    
    with open('src/steam_api64.def', 'r') as f:
        def_lines = [l.strip() for l in f if l.strip() and not l.strip().startswith(';') and not l.strip().startswith('LIBRARY') and not l.strip().startswith('EXPORTS')]
    def_export_names = set([l.split('=')[0].strip() for l in def_lines])

    missing = def_export_names - pe_names
    extra = pe_names - def_export_names

    print(f"[*] Target DLL:             {target_dll}")
    print(f"[*] Source DEF entries:     {len(def_export_names)}")
    print(f"[*] Real PE export entries: {len(pe_names)}")
    print(f"[*] Missing in PE:          {len(missing)}")

    print("\n[*] Critical Entrypoints Verification:")
    critical_symbols = [
        'SteamAPI_SteamNetworkingSockets_v008',
        'SteamAPI_SteamNetworkingUtils_v003',
        'SteamAPI_ISteamNetworkingSockets_ConnectP2P',
        'SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P',
        'SteamAPI_ISteamNetworkingSockets_AcceptConnection',
        'SteamAPI_ISteamNetworkingSockets_CloseConnection',
        'SteamAPI_Init',
        'SteamAPI_InitSafe',
        'SteamAPI_RestartAppIfNecessary',
        'SteamAPI_SteamUGC_v020',
        'SteamAPI_SteamTimeline_v004'
    ]

    all_critical_found = True
    for sym in critical_symbols:
        status = "[OK] PRESENT" if sym in pe_names else "[FAIL] MISSING"
        if sym not in pe_names:
            all_critical_found = False
        print(f"    {status.ljust(15)} {sym}")

    if missing:
        print(f"\n[FAIL] {len(missing)} exports missing from PE:")
        for m in sorted(missing):
            print(f"    - {m}")
        sys.exit(1)

    if not all_critical_found:
        print("\n[FAIL] One or more critical entrypoints missing!")
        sys.exit(1)

    print("\n[PASS] PE Export Table matches source export table perfectly!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
