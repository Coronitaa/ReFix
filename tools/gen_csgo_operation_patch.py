"""Builds src/games/csgo/operation_patch_data.h: the Operation Shattered Web missions that later CS:GO builds added
(weeks 5-16), for builds that still ship them as "locked".

Inputs: the installed build's items_game.txt and resource files, plus a later items_game.txt / csgo_english.txt /
csgo_spanish.txt and the guardian .cfg files (SteamDatabase/GameTracking-CSGO, e.g. commit 7b3f6874, 1.37.4.6).
Output: zlib-compressed JSON embedded as a byte array; ReFix merges it into the game files at startup
(src/csgo_operation_patch.cpp).

usage: gen_csgo_operation_patch.py <game_csgo_dir> <later_dir> <out_header> [season]
"""
import json
import os
import re
import sys
import urllib.request
import zlib

GAMETRACKING = "https://raw.githubusercontent.com/SteamDatabase/GameTracking-CSGO/7b3f687448014af149377517bf243b87e43ca9a2/csgo/"


def read_text(path):
    raw = open(path, "rb").read()
    if raw[:2] == b"\xff\xfe":
        return raw[2:].decode("utf-16-le")
    return raw.decode("utf-8", errors="replace")


def sections(text, name):
    """Yields (start, end) of every top-level `"name" { ... }` block body."""
    for m in re.finditer(r'\n\t"%s"\s*\n\t\{' % re.escape(name), text):
        i, depth = m.end(), 1
        j = i
        while depth and j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
            j += 1
        yield i, j - 1


def quest_blocks(text):
    out = {}
    for a, b in sections(text, "quest_definitions"):
        body = text[a:b]
        for m in re.finditer(r'\n\t\t"(\d+)"\s*\n\t\t\{.*?\n\t\t\}', body, re.S):
            out[int(m.group(1))] = m.group(0).strip("\n")
    return out


def mission_cards(text, season):
    cards = {}
    for a, b in sections(text, "seasonaloperations"):
        body = text[a:b]
        m = re.search(r'\n\t\t"%d"\s*\n\t\t\{' % season, body)
        if not m:
            continue
        for c in re.finditer(r'"quest_mission_card"\s*\{(.*?)\}', body[m.end():], re.S):
            cid = re.search(r'"id"\s*"(\d+)"', c.group(1))
            quests = re.search(r'"quests"\s*"([^"]*)"', c.group(1))
            name = re.search(r'"name"\s*"([^"]*)"', c.group(1))
            if cid and quests:
                cards[cid.group(1)] = {"quests": quests.group(1), "name": name.group(1) if name else ""}
    return cards


def expand(spec):
    ids = []
    for part in spec.split(","):
        part = part.strip()
        if "-" in part:
            lo, hi = part.split("-")
            ids += list(range(int(lo), int(hi) + 1))
        elif part.isdigit():
            ids.append(int(part))
    return ids


def tokens(text):
    out = {}
    for m in re.finditer(r'^\s*"([^"]+)"\s+"((?:[^"\\]|\\.)*)"', text, re.M):
        out[m.group(1).lower()] = (m.group(1), m.group(2))
    return out


def main():
    game, later, out_header = sys.argv[1], sys.argv[2], sys.argv[3]
    season = int(sys.argv[4]) if len(sys.argv) > 4 else 8
    old_items = read_text(os.path.join(game, "scripts", "items", "items_game.txt"))
    new_items = read_text(os.path.join(later, "items_game_2020.txt"))

    old_cards, new_cards = mission_cards(old_items, season), mission_cards(new_items, season)
    old_quests, new_quests = quest_blocks(old_items), quest_blocks(new_items)

    cards = {cid: c["quests"] for cid, c in new_cards.items()
             if old_cards.get(cid, {}).get("quests") == "locked" and c["quests"] != "locked"}
    quest_ids = sorted({q for spec in cards.values() for q in expand(spec)} - set(old_quests))
    quests = {str(q): new_quests[q] for q in quest_ids if q in new_quests}

    wanted = set()
    for block in quests.values():
        wanted |= {t.lower() for t in re.findall(r'"#([A-Za-z0-9_]+)"', block)}
    for cid in cards:
        n = new_cards[cid]["name"]
        if n.startswith("#"):
            wanted.add(n[1:].lower())

    lang = {}
    for name in ("english", "spanish"):
        have = tokens(read_text(os.path.join(game, "resource", "csgo_%s.txt" % name)))
        later_tokens = tokens(read_text(os.path.join(later, "csgo_%s.txt" % name)))
        lang[name] = {later_tokens[k][0]: later_tokens[k][1] for k in sorted(wanted) if k in later_tokens and k not in have}

    cfgs = {}
    for block in quests.values():
        for cfg in re.findall(r'execwithwhitelist\s+(\S+\.cfg)', block):
            rel = "maps/cfg/" + cfg
            if os.path.exists(os.path.join(game, rel)):
                continue
            try:
                cfgs[rel] = urllib.request.urlopen(GAMETRACKING + rel, timeout=30).read().decode("utf-8")
            except Exception as e:  # a missing cfg only breaks that guardian mission
                print("cfg not found:", rel, e)

    data = {"season": season, "cards": cards, "quests": quests, "tokens": lang, "cfgs": cfgs}
    blob = zlib.compress(json.dumps(data, ensure_ascii=False).encode("utf-8"), 9)
    with open(out_header, "w", newline="\n") as f:
        f.write("// Generated by tools/gen_csgo_operation_patch.py - do not edit.\n")
        f.write("// Operation season %d: %d weeks, %d quests, %d/%d tokens (en/es), %d cfgs.\n"
                % (season, len(cards), len(quests), len(lang["english"]), len(lang["spanish"]), len(cfgs)))
        f.write("#pragma once\n#include <cstddef>\n\nnamespace refix::csgo {\n\n")
        f.write("constexpr size_t kOperationPatchRawSize = %d;\n" % len(json.dumps(data, ensure_ascii=False).encode("utf-8")))
        f.write("static const unsigned char kOperationPatchZ[] = {\n")
        for i in range(0, len(blob), 24):
            f.write("    " + ",".join(str(b) for b in blob[i:i + 24]) + ",\n")
        f.write("};\n\n} // namespace refix::csgo\n")
    print("weeks", len(cards), "quests", len(quests), "tokens", {k: len(v) for k, v in lang.items()}, "cfgs", len(cfgs),
          "bytes", len(blob))


if __name__ == "__main__":
    main()
