"""Builds src/overlay/refix_icons_font.h: the overlay's icons from Fluent System Icons (MIT, Microsoft) placed at the
Segoe Fluent Icons / Segoe MDL2 Assets code points the overlay already uses. Wine and Proton prefixes have neither
Segoe icon font, so without this every icon is drawn as "?".

Usage: python tools/gen_icon_font.py <folder with FluentSystemIcons-Regular/Filled .ttf and .json>
Get them from https://github.com/microsoft/fluentui-system-icons/tree/main/fonts (needs: pip install fonttools).
"""
import io
import json
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

FAMILY = "ReFix Icons"

# Segoe code point -> (Fluent icon name, style). Keep in sync with overlay_theme.h (icon::) and kStock avatars.
ICONS = {
    0xE711: ("dismiss", "regular"),          # Close
    0xE72B: ("arrow_left", "regular"),       # Back
    0xE734: ("star", "regular"),             # Star
    0xE735: ("star", "filled"),              # StarFill
    0xE768: ("play", "regular"),             # Join
    0xE724: ("send", "regular"),             # Invite
    0xE716: ("people", "regular"),           # People
    0xE719: ("shopping_bag", "regular"),     # Store
    0xE8F2: ("chat", "regular"),             # Chat
    0xE736: ("book_open", "regular"),        # Guides
    0xE90F: ("wrench", "regular"),           # Workshop
    0xE789: ("news", "regular"),             # News
    0xE774: ("globe", "regular"),            # Globe
    0xE7C1: ("trophy", "regular"),           # Trophy
    0xE710: ("add", "regular"),              # Add
    0xE70F: ("edit", "regular"),             # Edit
    0xE8AB: ("arrow_swap", "regular"),       # Switch
    0xE8B7: ("folder", "regular"),           # Folder
    0xE8B5: ("arrow_import", "regular"),     # Import
    0xE73E: ("checkmark", "regular"),        # Check
    0xE946: ("info", "regular"),             # Info
    0xE7BA: ("warning", "regular"),          # Warning
    0xE839: ("router", "regular"),           # Lan
    0xE7FC: ("games", "regular"),            # Game, controller avatar
    0xE76B: ("chevron_left", "regular"),     # ChevronLeft
    0xE76C: ("chevron_right", "regular"),    # ChevronRight
    0xE70D: ("chevron_down", "regular"),     # ChevronDown
    0xE721: ("search", "regular"),           # Search
    0xE74D: ("delete", "regular"),           # Delete
    0xE72E: ("lock_closed", "regular"),      # Lock
    0xE77D: ("local_language", "regular"),   # Language
    0xE72C: ("arrow_clockwise", "regular"),  # Refresh
    0xE8A7: ("open", "regular"),             # OpenExternal
    0xE713: ("settings", "regular"),         # Settings
    0xEA8F: ("alert", "regular"),            # Bell
    0xE767: ("speaker_2", "regular"),        # Volume
    0xEC4A: ("top_speed", "regular"),        # Speed
    0xE753: ("cloud", "regular"),            # Cloud
    0xE88E: ("usb_stick", "regular"),        # Usb
    # Stock profile avatars (kStock in overlay_theme.cpp, stock glyphs in refix_overlay.cpp)
    0xE99A: ("bot", "regular"),
    0xE945: ("flash", "regular"),
    0xE706: ("weather_sunny", "regular"),
    0xE708: ("weather_moon", "regular"),
    0xE909: ("earth", "regular"),
    0xEA86: ("puzzle_piece", "regular"),
    0xE76E: ("emoji", "regular"),
    0xE8D6: ("music_note_2", "regular"),
    0xED56: ("food_pizza", "regular"),
    0xEB51: ("heart", "filled"),
    0xE8E1: ("thumb_like", "regular"),
}


def subset_font(path, codepoints):
    font = TTFont(path)
    opts = subset.Options()
    opts.glyph_names = True
    opts.notdef_outline = True
    opts.layout_features = []
    opts.hinting = False
    opts.name_IDs = []
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=codepoints)
    sub.subset(font)
    return font


def main(src):
    by_style = {}
    for style in ("regular", "filled"):
        cps = json.load(open(os.path.join(src, f"FluentSystemIcons-{style.title()}.json"), encoding="utf-8"))
        want = {}
        for target, (name, s) in ICONS.items():
            if s == style:
                key = f"ic_fluent_{name}_24_{style}"
                if key not in cps:
                    raise SystemExit(f"{key} is not in the {style} font")
                want[target] = cps[key]
        if want:
            font = subset_font(os.path.join(src, f"FluentSystemIcons-{style.title()}.ttf"), list(want.values()))
            by_style[style] = (font, want)

    base, base_want = by_style["regular"]
    cmap = {}
    src_cmap = base.getBestCmap()
    for target, fluent in base_want.items():
        cmap[target] = src_cmap[fluent]

    if "filled" in by_style:
        filled, filled_want = by_style["filled"]
        assert filled["head"].unitsPerEm == base["head"].unitsPerEm
        order = base.getGlyphOrder()
        fcmap = filled.getBestCmap()
        for target, fluent in filled_want.items():
            old = fcmap[fluent]
            new = f"{old}.filled"
            glyph = filled["glyf"][old]
            if glyph.isComposite():
                raise SystemExit(f"{old} is a composite glyph; copy its components too")
            order.append(new)
            base["glyf"].glyphs[new] = glyph
            base["hmtx"].metrics[new] = filled["hmtx"].metrics[old]
            cmap[target] = new
        base.setGlyphOrder(order)
        base["glyf"].glyphOrder = order

    # One BMP table with only the Segoe code points.
    from fontTools.ttLib.tables._c_m_a_p import CmapSubtable
    table = CmapSubtable.newSubtable(4)
    table.platformID, table.platEncID, table.language = 3, 1, 0
    table.cmap = cmap
    base["cmap"].tables = [table]
    base["post"].formatType = 3.0  # no glyph names in the file

    name = base["name"]
    name.names = []
    for nid, value in ((1, FAMILY), (2, "Regular"), (3, FAMILY), (4, FAMILY), (6, "ReFixIcons-Regular"),
                       (0, "Icons: Copyright (c) 2020 Microsoft Corporation, MIT License (Fluent System Icons)")):
        name.setName(value, nid, 3, 1, 0x409)

    out = io.BytesIO()
    base.save(out)
    data = out.getvalue()

    lines = [
        "// Generated by tools/gen_icon_font.py. Do not edit.",
        "// The overlay's icons for Wine/Proton, where Segoe Fluent Icons and Segoe MDL2 Assets are missing, at the",
        "// same code points. Glyphs from Fluent System Icons: Copyright (c) 2020 Microsoft Corporation, MIT License",
        "// (https://github.com/microsoft/fluentui-system-icons/blob/main/LICENSE).",
        "#pragma once",
        "",
        f"// Font family name, for GDI after AddFontMemResourceEx. {len(cmap)} glyphs.",
        f'#define REFIX_ICONS_FAMILY L"{FAMILY}"',
        "",
        f"static const unsigned char kReFixIconsTtf[{len(data)}] = {{",
    ]
    for i in range(0, len(data), 24):
        lines.append("    " + ",".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",")
    lines.append("};")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(root, "src", "overlay", "refix_icons_font.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(cmap)} icons, {len(data)} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
