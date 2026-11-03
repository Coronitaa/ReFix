#!/usr/bin/env python3
"""Generate version-correct Steamworks interface adapters for the ReFix emulator.

For every versioned header (src/include/steam/isteam<family>NNN.h) the script
emits a C++ adapter class deriving from the versioned interface.  The compiler
therefore builds the vtable straight from the SDK header, so games built with an
older SDK call the right slot.  Each override forwards to the emulator's latest
interface (src/generated/steam_emu_latest.h accessors) when a method with the
same name, arity and identical parameter types exists, else returns a safe
default.  ISteamClientNNN adapters route GetISteamXxx(...) through the exact
version lookup so the whole interface tree stays version-correct.

Output: src/generated/iface_<family>.cpp and src/generated/iface_registry.cpp
Usage : python tools/gen_steam_iface_adapters.py [--root <repo>]
Stdlib only, deterministic.
"""
import os
import re
import sys

# family -> (header stem, version-string template, latest class, accessor,
#            versions served natively by the emulator (never wrapped))
FAMILIES = [
    ("client",        "isteamclient",        "SteamClient{v}",                        "ISteamClient",        "isteamclient.h",        "Client",        {"021"}),
    ("user",          "isteamuser",          "SteamUser{v}",                          "ISteamUser",          "isteamuser.h",          "User",          {"021"}),
    ("friends",       "isteamfriends",       "SteamFriends{v}",                       "ISteamFriends",       "isteamfriends.h",       "Friends",       {"017"}),
    ("utils",         "isteamutils",         "SteamUtils{v}",                         "ISteamUtils",         "isteamutils.h",         "Utils",         set()),
    ("matchmaking",   "isteammatchmaking",   "SteamMatchMaking{v}",                   "ISteamMatchmaking",   "isteammatchmaking.h",   "Matchmaking",   set()),
    ("networking",    "isteamnetworking",    "SteamNetworking{v}",                    "ISteamNetworking",    "isteamnetworking.h",    "Networking",    set()),
    ("userstats",     "isteamuserstats",     "STEAMUSERSTATS_INTERFACE_VERSION{v}",   "ISteamUserStats",     "isteamuserstats.h",     "UserStats",     set()),
    ("apps",          "isteamapps",          "STEAMAPPS_INTERFACE_VERSION{v}",        "ISteamApps",          "isteamapps.h",          "Apps",          set()),
    ("remotestorage", "isteamremotestorage", "STEAMREMOTESTORAGE_INTERFACE_VERSION{v}", "ISteamRemoteStorage", "isteamremotestorage.h", "RemoteStorage", set()),
    ("ugc",           "isteamugc",           "STEAMUGC_INTERFACE_VERSION{v}",         "ISteamUGC",           "isteamugc.h",           "UGC",           set()),
    ("http",          "isteamhttp",          "STEAMHTTP_INTERFACE_VERSION{v}",        "ISteamHTTP",          "isteamhttp.h",          "HTTP",          set()),
    ("screenshots",   "isteamscreenshots",   "STEAMSCREENSHOTS_INTERFACE_VERSION{v}", "ISteamScreenshots",   "isteamscreenshots.h",   "Screenshots",   set()),
    ("controller",    "isteamcontroller",    "SteamController{v}",                    "ISteamController",    "isteamcontroller.h",    "Controller",    {"008"}),
    ("gameserver",    "isteamgameserver",    "SteamGameServer{v}",                    "ISteamGameServer",    "isteamgameserver.h",    "GameServer",    {"012"}),
]

# (family, version) -> reason; filled from compile probing of the SDK headers.
SKIP = {
    ("controller", "002"): "header declares unknown_ret placeholders",
}

# Hand written bodies for versioned-only methods that have a clear equivalent.
SPECIAL_BODIES = {
    ("user", "GetHSteamUser"): "return UnrealSteamEmu::GetHSteamUser();",
}

BASIC_TYPE_WORDS = {"int", "char", "short", "long", "unsigned", "signed", "float",
                    "double", "bool", "void", "__int64", "size_t"}
QUALIFIERS = {"const", "volatile", "struct", "enum", "class", "union"}
SCALAR_RET = {"bool", "int", "int8", "uint8", "int16", "uint16", "int32", "uint32",
              "int64", "uint64", "float", "double", "unsignedint", "size_t"}


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


DEFINED_MACROS = {"STEAM_WIN32", "STEAM_API_NODLL", "_WIN32", "_MSC_VER", "WIN32"}


def eval_cond(expr):
    expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)", lambda m: "1" if m.group(1) in DEFINED_MACROS else "0", expr)
    expr = re.sub(r"defined\s+(\w+)", lambda m: "1" if m.group(1) in DEFINED_MACROS else "0", expr)
    expr = re.sub(r"[A-Za-z_]\w*", lambda m: "1" if m.group(0) in DEFINED_MACROS else "0", expr)
    expr = expr.replace("&&", " and ").replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    try:
        return bool(eval(expr, {"__builtins__": {}}, {}))
    except Exception:
        return True


def strip_preproc(text):
    """Evaluate #if/#ifdef blocks for a Windows, non-PS3 build and drop directives."""
    keep, stack, cont = [], [], False   # stack entries: [taken, active]
    for ln in text.split("\n"):
        s = ln.strip()
        active = all(e[1] for e in stack)
        if cont:
            cont = s.endswith("\\")
            continue
        if not s.startswith("#"):
            if active:
                keep.append(ln)
            continue
        cont = s.endswith("\\")
        m = re.match(r"#\s*(\w+)\s*(.*)$", s)
        d, arg = m.group(1), m.group(2).strip()
        if d in ("ifdef", "ifndef", "if"):
            if d == "if":
                c = eval_cond(arg)
            else:
                c = (arg in DEFINED_MACROS) == (d == "ifdef")
            stack.append([c, c and active])
        elif d == "elif" and stack:
            e = stack[-1]
            outer = all(x[1] for x in stack[:-1])
            c = (not e[0]) and eval_cond(arg)
            e[0] = e[0] or c
            e[1] = c and outer
        elif d == "else" and stack:
            e = stack[-1]
            outer = all(x[1] for x in stack[:-1])
            e[1] = (not e[0]) and outer
            e[0] = True
        elif d == "endif" and stack:
            stack.pop()
    return "\n".join(keep)


def expand_struct_return(text):
    """STEAMWORKS_STRUCT_RETURN_N(ret, name, T1, n1, ...) -> virtual ret name(T1 n1, ...) = 0;"""
    def rep(m):
        parts = [x.strip() for x in m.group(2).split(",")]
        ret, name, rest = parts[0], parts[1], parts[2:]
        args = ", ".join("%s %s" % (rest[i], rest[i + 1]) for i in range(0, len(rest), 2))
        return "virtual %s %s(%s) = 0;" % (ret, name, args)
    return re.sub(r"STEAMWORKS_STRUCT_RETURN_(\d)\s*\(([^()]*)\)", rep, text)


def strip_macros(s):
    # Annotation macros such as STEAM_OUT_ARRAY_COUNT( a, b ) / METHOD_DESC( ... )
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r"\b(?:STEAM_[A-Z_]+|METHOD_DESC|CALL_RESULT|CALL_BACK|OUT_[A-Z_]+|S_API|STEAM_CLANG_ATTR)\s*\((?:[^()]|\([^()]*\))*\)", " ", s)
    s = re.sub(r"\bSTEAM_[A-Z_]+\b(?!\s*\()", " ", s)
    return s


def unwrap_private_api(text):
    """STEAM_PRIVATE_API( decls ) is transparent under STEAM_API_NODLL: keep decls."""
    key = "STEAM_PRIVATE_API"
    while True:
        i = text.find(key)
        if i < 0:
            return text
        j = text.find("(", i)
        depth, k = 0, j
        while k < len(text):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        text = text[:i] + " " + text[j + 1:k] + " " + text[k + 1:]


def find_class_body(text, cls):
    m = re.search(r"\bclass\s+" + re.escape(cls) + r"\b\s*(?::[^{;]*)?\{", text)
    if not m:
        return None
    i, depth = m.end(), 1
    start = i
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return unwrap_private_api(text[start:i - 1])


def split_statements(body):
    stmts, cur, depth_b, depth_p = [], [], 0, 0
    for c in body:
        if c == "{":
            depth_b += 1
            if depth_b == 1:
                cur.append(" ")
            continue
        if c == "}":
            depth_b -= 1
            if depth_b == 0:
                stmts.append("".join(cur))
                cur = []
            continue
        if depth_b:
            continue
        if c == "(":
            depth_p += 1
        elif c == ")":
            depth_p -= 1
        if c == ";" and depth_p == 0:
            stmts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    return stmts


def split_params(s):
    parts, cur, d = [], [], 0
    for c in s:
        if c in "(<[":
            d += 1
        elif c in ")>]":
            d -= 1
        if c == "," and d == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    last = "".join(cur)
    if last.strip() or parts:
        parts.append(last)
    return [p.strip() for p in parts]


def strip_default(p):
    d, i = 0, 0
    for i, c in enumerate(p):
        if c in "([<":
            d += 1
        elif c in ")]>":
            d -= 1
        elif c == "=" and d == 0:
            return p[:i].strip()
    return p.strip()


def parse_param(p):
    """Return (type_text_with_NAME_slot, normalized_type). '@' marks the name slot."""
    p = strip_default(re.sub(r"\s+", " ", p))
    if p in ("", "void"):
        return None
    fp = re.search(r"\(\s*\*\s*(\w*)\s*\)", p)
    if fp:  # function pointer
        slot = p[:fp.start(1)] + "@" + p[fp.end(1):]
        return slot, re.sub(r"\s+", "", slot)
    arr = re.search(r"(\[[^\]]*\])\s*$", p)
    core = p[:arr.start()].strip() if arr else p
    suffix = arr.group(1) if arr else ""
    idents = [t for t in re.findall(r"[A-Za-z_]\w*", core) if t not in QUALIFIERS]
    named = len(idents) >= 2 and idents[-1] not in BASIC_TYPE_WORDS and re.search(r"\w$", core)
    if named:
        m = re.search(r"(\w+)$", core)
        type_text = core[:m.start()].rstrip()
    else:
        type_text = core
    if arr:
        type_text += "@" + suffix if False else ""
        slot = type_text + " @" + suffix
        return slot, re.sub(r"\s+", "", type_text + suffix)
    return type_text + " @", re.sub(r"\s+", "", type_text)


def parse_methods(cls_body):
    methods = []
    for st in split_statements(cls_body):
        st = strip_macros(re.sub(r"\s+", " ", st)).strip()
        if "virtual" not in st or "(" not in st:
            continue
        if not re.search(r"=\s*0\s*$", st):
            continue
        st = re.sub(r"=\s*0\s*$", "", st).strip()
        st = re.sub(r"^.*?\bvirtual\b", "", st).strip()
        m = re.match(r"(.*?)\b(\w+)\s*\((.*)\)\s*(const)?\s*$", st)
        if not m:
            raise ValueError("cannot parse: " + st)
        ret, name, params, const = m.group(1).strip(), m.group(2), m.group(3), m.group(4)
        plist = []
        for p in split_params(params):
            pp = parse_param(p)
            if pp:
                plist.append(pp)
        methods.append({"ret": ret, "name": name, "params": plist, "const": bool(const)})
    return methods


def default_return(ret):
    r = re.sub(r"\s+", " ", ret).strip()
    if r == "void":
        return ""
    if re.fullmatch(r"const char \*", r) or re.fullmatch(r"const char\*", r):
        return 'return "";'
    return "return {};"


def gen_method(m, latest_by_name, fam, counters, is_client):
    params = m["params"]
    names = ["a%d" % i for i in range(len(params))]
    decl_params = ", ".join(slot.replace("@", n) for (slot, _), n in zip(params, names))
    sig = "%s %s(%s)%s override" % (m["ret"], m["name"], decl_params, " const" if m["const"] else "")
    ret_void = m["ret"].strip() == "void"
    special = SPECIAL_BODIES.get((fam, m["name"]))
    body_lines = []
    # Client: GetISteamXxx(..., const char *pchVersion) -> exact version lookup.
    ver_idx = None
    if is_client and re.match(r"(DEPRECATED_)?GetISteam", m["name"]) and params:
        for i, (_, norm) in enumerate(params):
            if norm == "constchar*":
                ver_idx = i
    if ver_idx is not None:
        body_lines.append("if (void* v = UnrealSteamEmu::GetGenericInterface(%s)) return static_cast<%s>(v);" % (names[ver_idx], m["ret"]))
    if special and ver_idx is None:
        counters["forwarded"] += 1
        body_lines.append(special)
        return "    virtual %s { %s }" % (sig, " ".join(body_lines))
    target = None
    for cand in latest_by_name.get(m["name"], []):
        if len(cand["params"]) != len(params):
            continue
        if any(a[1] != b[1] for a, b in zip(cand["params"], params)):
            continue
        rr, cr = re.sub(r"\s+", "", cand["ret"]), re.sub(r"\s+", "", m["ret"])
        if rr == cr or (rr in SCALAR_RET and cr in SCALAR_RET):
            target = cand
            break
    if target is not None and not ret_void and re.sub(r"\s+", "", target["ret"]) != re.sub(r"\s+", "", m["ret"]):
        call_cast = "static_cast<%s>" % m["ret"]
    else:
        call_cast = ""
    if target is not None:
        counters["forwarded"] += 1
        call = "UnrealSteamEmu::Latest%s()->%s(%s)" % (counters["acc"], m["name"], ", ".join(names))
        if ret_void:
            body_lines.append("%s;" % call)
        else:
            body_lines.append("return %s(%s);" % (call_cast, call))
    else:
        counters["defaulted"] += 1
        counters["defaulted_names"].append(m["name"])
        body_lines.append(default_return(m["ret"]))
    return "    virtual %s { %s }" % (sig, " ".join(b for b in body_lines if b))


PREAMBLE = """#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstring>
#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
class ISteamPS3OverlayRender;  // PS3-only, opaque here
"""


def main():
    root = os.getcwd()
    if "--root" in sys.argv:
        root = sys.argv[sys.argv.index("--root") + 1]
    steam_dir = os.path.join(root, "src", "include", "steam")
    out_dir = os.path.join(root, "src", "generated")
    os.makedirs(out_dir, exist_ok=True)

    totals = {"adapters": 0, "forwarded": 0, "defaulted": 0}
    registry = {}  # family -> list of (versionString, adapterVar)
    report = []
    for fam, stem, vtmpl, latest_cls, latest_hdr, acc, native in FAMILIES:
        with open(os.path.join(steam_dir, latest_hdr), encoding="utf-8", errors="replace") as f:
            ltxt = expand_struct_return(strip_preproc(strip_comments(f.read())))
        lbody = find_class_body(ltxt, latest_cls)
        if lbody is None:
            raise SystemExit("latest class %s not found" % latest_cls)
        latest_methods = parse_methods(lbody)
        latest_by_name = {}
        for m in latest_methods:
            latest_by_name.setdefault(m["name"], []).append(m)
        versions = sorted(re.match(re.escape(stem) + r"(\d{3})\.h$", fn).group(1)
                          for fn in os.listdir(steam_dir)
                          if re.match(re.escape(stem) + r"\d{3}\.h$", fn))
        skipped = []
        classes, entries = [], []
        fam_fwd = fam_def = 0
        for v in versions:
            if v in native:
                skipped.append((v, "served natively by the emulator"))
                continue
            if (fam, v) in SKIP:
                skipped.append((v, SKIP[(fam, v)]))
                continue
            cls = "%s%s" % (latest_cls, v)
            with open(os.path.join(steam_dir, "%s%s.h" % (stem, v)), encoding="utf-8", errors="replace") as f:
                htxt = expand_struct_return(strip_preproc(strip_comments(f.read())))
            body = find_class_body(htxt, cls)
            if body is None:
                skipped.append((v, "class %s not found in header" % cls))
                continue
            counters = {"forwarded": 0, "defaulted": 0, "acc": acc, "defaulted_names": []}
            methods = parse_methods(body)
            lines = [gen_method(m, latest_by_name, fam, counters, fam == "client") for m in methods]
            adapter = "ReFixAdapter_%s" % cls
            classes.append("class %s : public %s {\npublic:\n%s\n};\nstatic %s g_%s;\n" % (
                adapter, cls, "\n".join(lines), adapter, adapter))
            entries.append((vtmpl.format(v=v), "static_cast<%s*>(&g_%s)" % (cls, adapter)))
            totals["adapters"] += 1
            totals["forwarded"] += counters["forwarded"]
            totals["defaulted"] += counters["defaulted"]
            fam_fwd += counters["forwarded"]
            fam_def += counters["defaulted"]
        registry[fam] = entries
        hdr = ["// GENERATED by tools/gen_steam_iface_adapters.py - do not edit.",
               "// Family: %s  adapters: %d  forwarded methods: %d  defaulted methods: %d" % (fam, len(entries), fam_fwd, fam_def),
               "// Skipped versions:"]
        hdr += ["//   %s%s: %s" % (stem, v, why) for v, why in skipped] or ["//   (none)"]
        incs = "".join('#include "include/steam/%s%s.h"\n' % (stem, v) for v in versions if (fam, v) not in SKIP)
        text = "\n".join(hdr) + "\n" + PREAMBLE + incs + '#include "generated/steam_emu_latest.h"\n\n'
        text += "\n".join(classes)
        text += "\nextern const ReFixIfaceEntry g_ReFixIfaceTable_%s[] = {\n" % fam
        text += "".join('    { "%s", %s },\n' % (s, e) for s, e in sorted(entries))
        text += "    { nullptr, nullptr }\n};\n"
        with open(os.path.join(out_dir, "iface_%s.cpp" % fam), "w", newline="\n") as f:
            f.write(text)
        report.append("%-13s adapters=%-3d forwarded=%-4d defaulted=%-4d skipped=%s" % (
            fam, len(entries), fam_fwd, fam_def, ",".join(v for v, _ in skipped) or "-"))

    reg = ["// GENERATED by tools/gen_steam_iface_adapters.py - do not edit.",
           '#include <cstring>', '#include "generated/steam_emu_latest.h"', ""]
    for fam, *_ in FAMILIES:
        reg.append("extern const ReFixIfaceEntry g_ReFixIfaceTable_%s[];" % fam)
    reg += ["", "static const ReFixIfaceEntry* const kTables[] = {"]
    reg += ["    g_ReFixIfaceTable_%s," % fam for fam, *_ in FAMILIES]
    reg += ["};", "",
            "void* ReFixVersionedInterface(const char* pchVersion) {",
            "    if (!pchVersion) return nullptr;",
            "    for (const ReFixIfaceEntry* t : kTables)",
            "        for (const ReFixIfaceEntry* e = t; e->version; ++e)",
            "            if (std::strcmp(e->version, pchVersion) == 0) return e->iface;",
            "    return nullptr;", "}", ""]
    with open(os.path.join(out_dir, "iface_registry.cpp"), "w", newline="\n") as f:
        f.write("\n".join(reg))

    print("\n".join(report))
    print("TOTAL adapters=%(adapters)d forwarded=%(forwarded)d defaulted=%(defaulted)d" % totals)


if __name__ == "__main__":
    main()
