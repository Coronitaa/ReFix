#!/usr/bin/env python3
"""Generate the generic flat-API dispatcher for the ReFix LAN emulator.

In LAN mode every steam_api export is resolved by LookupLanExport().  Exports
without a hand-written wrapper used to land on SafeUnsupportedExportStub and
silently return 0, so any game calling them through the flat API (Steamworks.NET,
Facepunch, GodotSteam, ...) saw failures for features the emulator implements
(cloud-save streams, async reads, friends, stats, ...).

For every SteamAPI_ISteamXxx_Method( ISteamXxx* self, ... ) declared in
src/include/steam/steam_api_flat.h this script emits a thunk that calls
Method(...) on the emulator object, exactly like Valve's own steam_api does.
A thunk is emitted only when the latest SDK class ISteamXxx declares a method
with the same name and arity, so the call is checked by the compiler.
Versioned accessors (SteamAPI_SteamXxx_vNNN) resolve through
UnrealSteamEmu::GetGenericInterface with the matching version string, which
already returns the version-correct adapter.

Output: src/generated/flat_dispatch.cpp
Usage : python tools/gen_steam_flat_dispatch.py [--root <repo>]
Stdlib only, deterministic.
"""
import os
import re
import sys

# Interfaces the emulator implements (ReFixLatestInterface knows them).
INTERFACES = [
    "ISteamClient", "ISteamUser", "ISteamFriends", "ISteamUtils", "ISteamMatchmaking",
    "ISteamMatchmakingServers", "ISteamUserStats", "ISteamApps", "ISteamNetworking",
    "ISteamNetworkingSockets", "ISteamNetworkingUtils", "ISteamNetworkingMessages",
    "ISteamRemoteStorage", "ISteamUGC", "ISteamGameServer", "ISteamGameServerStats",
    "ISteamHTTP", "ISteamInput", "ISteamInventory", "ISteamScreenshots", "ISteamTimeline",
    "ISteamGameSearch", "ISteamAppList", "ISteamMusic", "ISteamMusicRemote",
    "ISteamHTMLSurface", "ISteamVideo", "ISteamParentalSettings", "ISteamParties",
    "ISteamRemotePlay",
]

# Flat functions whose flat signature differs from the C++ method in a way the
# thunk cannot express; they keep the safe stub unless hand-written.
EXCLUDE = set()

FLAT_RE = re.compile(r"^STEAMAPI_API\s+(.+?)\s*\b(SteamAPI_(\w+?)_(\w+))\s*\((.*)\);\s*(//.*)?$")
ACCESSOR_RE = re.compile(r"^STEAMAPI_API\s+(\w+)\s*\*\s*(SteamAPI_Steam(\w+?)_v(\d+))\s*\(\s*\)\s*;")
METHOD_RE = re.compile(r"virtual\s+[^;{}()]*?\b(\w+)\s*\(([^;{}]*?)\)\s*(?:const\s*)?=\s*0\s*;", re.S)
OVERLOAD_SUFFIX_RE = re.compile(r"^(\w+?)(Int32|Int64|Float|Double|String|Bool|Ptr)$")
VERSION_RE = re.compile(r'#define\s+\w+_INTERFACE_VERSION\w*\s+"([^"]+)"')


def split_params(text):
    parts, depth, cur = [], 0, ""
    for ch in text:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return [p for p in parts if p and p != "void"]


def param_name(param):
    m = re.search(r"(\w+)\s*(\[[^\]]*\])?\s*$", param)
    return m.group(1)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def latest_classes(sdk_dir):
    """class name -> (method arity map, version string) from the unversioned headers."""
    classes = {}
    for fn in sorted(os.listdir(sdk_dir)):
        if not fn.endswith(".h") or re.search(r"\d{3}\.h$", fn):
            continue
        raw = open(os.path.join(sdk_dir, fn), encoding="utf-8", errors="ignore").read()
        text = strip_comments(raw)
        for m in re.finditer(r"\bclass\s+(ISteam\w+)\s*(?::[^{;]*)?\{", text):
            name = m.group(1)
            if name not in INTERFACES or name in classes:
                continue
            depth, i = 0, m.end() - 1
            while i < len(text):
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            body = text[m.end():i]
            methods = {}
            for mm in METHOD_RE.finditer(body):
                methods.setdefault(mm.group(1), set()).add(len(split_params(mm.group(2))))
            vm = VERSION_RE.search(raw[m.end():])
            # Non-virtual members (inline helpers such as SetGlobalConfigValueInt32, defined in or
            # after the class) are callable too; the compiler checks every generated call.
            for mm in re.finditer(r"\b(\w+)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(?:const\s*)?[{;]", body):
                methods.setdefault(mm.group(1), set()).add(len(split_params(mm.group(2))))
            classes[name] = (methods, vm.group(1) if vm else None)
    return classes


def thunk(ret, flat, iface, method, params):
    args = []
    for p in params:
        n = param_name(p)
        if re.match(r"^(const\s+)?uint64_steamid\b", p):
            args.append("ReFixFlatSteamID{%s}" % n)
        elif re.match(r"^(const\s+)?uint64_gameid\b", p):
            args.append("ReFixFlatGameID{%s}" % n)
        else:
            args.append(n)
    call = "s->%s(%s)" % (method, ", ".join(args))
    sig = ", ".join(["%s* self" % iface] + params)
    lines = ["static %s REFIX_FLAT_CC ReFixFlat_%s(%s) {" % (ret, flat, sig),
             "    %s* s = ReFixFlatSelf<%s>(self, \"%s\");" % (iface, iface, iface)]
    if ret == "void":
        lines += ["    if (s) %s;" % call, "}"]
    else:
        if ret in ("uint64_steamid", "uint64_gameid"):
            expr = "ReFixFlatU64(%s)" % call
        else:
            expr = "static_cast<%s>(%s)" % (ret, call)
        lines += ["    if (!s) return {};", "    return %s;" % expr, "}"]
    return "\n".join(lines)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if "--root" in sys.argv:
        root = sys.argv[sys.argv.index("--root") + 1]
    sdk_dir = os.path.join(root, "src", "include", "steam")
    classes = latest_classes(sdk_dir)
    missing = [c for c in INTERFACES if c not in classes]
    if missing:
        sys.exit("latest SDK class not found: " + ", ".join(missing))

    thunks, table, skipped = [], [], []
    seen = set()
    flat_lines = open(os.path.join(sdk_dir, "steam_api_flat.h"), encoding="utf-8", errors="ignore").read().splitlines()
    for line in flat_lines:
        line = line.strip()
        am = ACCESSOR_RE.match(line)
        if am:
            iface, flat, family, ver = am.groups()
            if iface not in classes or flat in seen:
                continue
            latest_ver = classes[iface][1]
            if not latest_ver or not re.search(r"\d+$", latest_ver):
                skipped.append("%s (no version string)" % flat)
                continue
            version = re.sub(r"\d+$", lambda d: ver.zfill(len(d.group(0))), latest_ver)
            seen.add(flat)
            thunks.append("static %s* REFIX_FLAT_CC ReFixFlat_%s() {\n"
                          "    return static_cast<%s*>(UnrealSteamEmu::GetGenericInterface(\"%s\"));\n}" % (iface, flat, iface, version))
            table.append(flat)
            continue
        fm = FLAT_RE.match(line)
        if not fm:
            continue
        ret, flat, iface, method, param_text = fm.groups()[:5]
        params = split_params(param_text)
        if not params or not re.match(r"^%s\s*\*\s*self$" % re.escape(iface), params[0]):
            continue
        if iface not in classes or flat in seen:
            continue
        params = params[1:]
        arities = classes[iface][0].get(method)
        target = method
        if not arities:
            # Overloads are named with a type suffix in the flat API (GetStatInt32 -> GetStat(int32*)).
            sm = OVERLOAD_SUFFIX_RE.match(method)
            if sm and sm.group(1) in classes[iface][0]:
                target = sm.group(1)
                arities = classes[iface][0][target]
        if flat in EXCLUDE:
            skipped.append("%s (excluded)" % flat)
            continue
        if not arities or len(params) not in arities:
            skipped.append("%s (not in latest %s)" % (flat, iface))
            continue
        seen.add(flat)
        thunks.append(thunk(ret, flat, iface, target, params))
        table.append(flat)

    out = [
        "// GENERATED by tools/gen_steam_flat_dispatch.py - do not edit.",
        "// Flat thunks: %d  skipped: %d" % (len(table), len(skipped)),
        "// Skipped:",
    ] + ["//   %s" % s for s in skipped] + [
        "#define WIN32_LEAN_AND_MEAN",
        "#include <windows.h>",
        "#include <cstdint>",
        "#include <cstring>",
        "#include <unordered_map>",
        "#include <string>",
        "#define STEAM_WIN32 1",
        "#define STEAM_API_NODLL 1",
        '#include "include/steam/steam_api.h"',
        '#include "include/steam/isteamgameserver.h"',
        '#include "include/steam/isteamnetworkingsockets.h"',
        '#include "include/steam/isteamnetworkingutils.h"',
        '#include "include/steam/isteamnetworkingmessages.h"',
        '#include "include/steam/isteamgameserverstats.h"',
        '#include "generated/flat_dispatch.h"',
        "",
        "#if defined(_M_IX86)",
        "#define REFIX_FLAT_CC __cdecl",
        "#else",
        "#define REFIX_FLAT_CC",
        "#endif",
        "",
        "typedef uint64 uint64_steamid;  // as in steam_api_flat.h",
        "typedef uint64 uint64_gameid;",
        "",
        "namespace UnrealSteamEmu { void* GetGenericInterface(const char* pchVersion); }",
        "",
        "namespace {",
        "",
        "// flat uint64 <-> CSteamID / CGameID, whichever the C++ method takes.",
        "struct ReFixFlatSteamID {",
        "    uint64 v;",
        "    operator CSteamID() const { return CSteamID(v); }",
        "    operator uint64() const { return v; }",
        "};",
        "struct ReFixFlatGameID {",
        "    uint64 v;",
        "    operator CGameID() const { return CGameID(v); }",
        "    operator uint64() const { return v; }",
        "};",
        "inline uint64 ReFixFlatU64(CSteamID id) { return id.ConvertToUint64(); }",
        "inline uint64 ReFixFlatU64(CGameID id) { return id.ToUint64(); }",
        "inline uint64 ReFixFlatU64(uint64 v) { return v; }",
        "",
        "// The game's self pointer is used only when it is the emulator's latest-version",
        "// object (same vtable); versioned adapters and null fall back to that object.",
        "template <class T>",
        "T* ReFixFlatSelf(T* self, const char* iface) {",
        "    T* latest = static_cast<T*>(ReFixLatestInterface(iface));",
        "    if (!latest) return nullptr;",
        "    if (self && self != latest && *reinterpret_cast<void**>(self) == *reinterpret_cast<void**>(latest)) return self;",
        "    return latest;",
        "}",
        "",
    ] + thunks + [
        "",
        "} // namespace",
        "",
        "FARPROC ReFixFlatDispatchLookup(const char* name) {",
        "    static const std::unordered_map<std::string, FARPROC> s_table = {",
    ] + ['        { "%s", (FARPROC)ReFixFlat_%s },' % (f, f) for f in table] + [
        "    };",
        "    if (!name) return nullptr;",
        "    auto it = s_table.find(name);",
        "    return it == s_table.end() ? nullptr : it->second;",
        "}",
        "",
    ]
    path = os.path.join(root, "src", "generated", "flat_dispatch.cpp")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote %s: %d thunks, %d skipped" % (path, len(table), len(skipped)))


if __name__ == "__main__":
    main()
