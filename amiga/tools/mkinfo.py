#!/usr/bin/env python3
"""Build Workbench .info icons from text pixel art.

Workbench 1.3 and 2.x+ give the four icon pens different colours, so there
are two sets of art, one character per pixel, each naming colours in its
own palette:

  gfx/*.icon.txt  (Workbench 1.3)     gfx/*.icon2.txt  (Workbench 2.x+)
    .  pen 0  blue (background)         .  pen 0  grey (background)
    W  pen 1  white                     B  pen 1  black
    B  pen 2  black                     W  pen 2  white
    O  pen 3  orange                    U  pen 3  blue

Both sets are written in the original (OS 1.x) DiskObject format: a
two-bitplane Image with complemented highlight and no OS 2.x extensions,
which every Workbench loads. Standard library only; Pillow is used for
--preview if present.

Usage: mkinfo.py [--preview out.png]     (run from amiga/)
"""

import argparse
import os
import struct
import sys

WB_DISKMAGIC = 0xE310
WB_DISKVERSION = 1
WBDISK, WBDRAWER, WBTOOL, WBPROJECT = 1, 2, 3, 4
NO_ICON_POSITION = 0x80000000

GADGIMAGE = 0x0004
GADGHCOMP = 0x0000
RELVERIFY, GADGIMMEDIATE = 0x0001, 0x0002
BOOLGADGET = 0x0001

PALETTES = {
    # art suffix, output dir, character -> pen, preview RGB per pen
    "wb13": (".icon.txt", "icons/wb13", {".": 0, "W": 1, "B": 2, "O": 3},
             [(0x00, 0x55, 0xAA), (0xFF, 0xFF, 0xFF), (0x00, 0x00, 0x22), (0xFF, 0x88, 0x00)]),
    "wb2": (".icon2.txt", "icons/wb2", {".": 0, "B": 1, "W": 2, "U": 3},
            [(0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x66, 0x88, 0xBB)]),
}

ICONS = [
    # art name, icon name, type, default tool, stack
    ("isstracker", "ISSTracker.info", WBTOOL, None, 8192),
    ("disk", "Disk.info", WBDISK, "SYS:System/DiskCopy", 0),
    ("readme", "ReadMe.info", WBPROJECT, "SYS:Utilities/More", 4096),
]


def load_art(path, pens):
    with open(path) as f:
        rows = [line.rstrip("\n") for line in f if line.strip()]
    width = max(len(r) for r in rows)
    grid = []
    for y, row in enumerate(rows):
        row = row.ljust(width, ".")
        try:
            grid.append([pens[c] for c in row])
        except KeyError as e:
            sys.exit(f"{path}:{y + 1}: unknown pixel {e}")
    return grid


def image_data(grid):
    w = len(grid[0])
    words = (w + 15) // 16
    out = bytearray()
    for plane in range(2):
        for row in grid:
            bits = 0
            for x in range(words * 16):
                bits <<= 1
                if x < w and row[x] & (1 << plane):
                    bits |= 1
            out += bits.to_bytes(words * 2, "big")
    return out


def cstring(s):
    b = s.encode("latin-1") + b"\0"
    return struct.pack(">L", len(b)) + b


def build_icon(grid, kind, default_tool, stack):
    w, h = len(grid[0]), len(grid)
    has_drawer = kind in (WBDISK, WBDRAWER)

    gadget = struct.pack(
        ">LhhhhHHHLLLLLHL",
        0,                      # NextGadget
        0, 0, w, h,             # LeftEdge, TopEdge, Width, Height
        GADGIMAGE | GADGHCOMP,  # Flags
        RELVERIFY | GADGIMMEDIATE,
        BOOLGADGET,
        1,                      # GadgetRender (non-NULL: image follows)
        0,                      # SelectRender
        0, 0, 0,                # GadgetText, MutualExclude, SpecialInfo
        0,                      # GadgetID
        0,                      # UserData: 0 = OS 1.x icon
    )
    assert len(gadget) == 44

    disk = struct.pack(">HH", WB_DISKMAGIC, WB_DISKVERSION) + gadget + struct.pack(
        ">BBLLllLLl",
        kind, 0,
        1 if default_tool else 0,   # DefaultTool
        0,                          # ToolTypes
        NO_ICON_POSITION - (1 << 32), NO_ICON_POSITION - (1 << 32),
        1 if has_drawer else 0,     # DrawerData
        0,                          # ToolWindow
        stack,
    )
    assert len(disk) == 78

    out = bytearray(disk)
    if has_drawer:
        # NewWindow (48 bytes) + CurrentX/CurrentY: the window Workbench
        # opens for this disk.
        out += struct.pack(">hhhhBBLLLLLLLhhHHH", 40, 30, 400, 120, 255, 255,
                           0, 0, 0, 0, 0, 0, 0, 90, 40, 0xFFFF, 0xFFFF, 1)
        out += struct.pack(">ll", 0, 0)
    out += struct.pack(">hhhhhLBBL", 0, 0, w, h, 2, 1, 3, 0, 0)
    out += image_data(grid)
    if default_tool:
        out += cstring(default_tool)
    return bytes(out)


def check_icon(data, grid, kind):
    """Re-read the fields Workbench depends on."""
    magic, version = struct.unpack_from(">HH", data, 0)
    w, h = struct.unpack_from(">hh", data, 4 + 8)
    assert (magic, version) == (WB_DISKMAGIC, WB_DISKVERSION)
    assert data[48] == kind
    assert (w, h) == (len(grid[0]), len(grid))
    off = 78 + (56 if kind in (WBDISK, WBDRAWER) else 0)
    iw, ih, depth = struct.unpack_from(">hhh", data, off + 4)
    assert (iw, ih, depth) == (w, h, 2)


def preview(rows, path):
    """rows: [(rgb palette, [grid, ...]), ...], one row per Workbench."""
    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed: skipping preview")
        return
    scale = 3
    pad = 8
    width = max(sum(len(g[0]) for g in grids) + pad * (len(grids) + 1)
                for _, grids in rows)
    row_h = max(len(g) for _, grids in rows for g in grids) * 2 + pad * 2
    img = Image.new("RGB", (width, row_h * len(rows)))
    for r, (rgb, grids) in enumerate(rows):
        y0 = r * row_h
        for y in range(row_h):
            for x in range(width):
                img.putpixel((x, y0 + y), rgb[0])
        x0 = pad
        for g in grids:
            for y, row in enumerate(g):
                for x, p in enumerate(row):
                    # Workbench runs in hires: pixels are twice as tall as wide
                    img.putpixel((x0 + x, y0 + pad + y * 2), rgb[p])
                    img.putpixel((x0 + x, y0 + pad + y * 2 + 1), rgb[p])
            x0 += len(g[0]) + pad
    img.resize((width * scale, row_h * len(rows) * scale), Image.NEAREST).save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preview", help="write a PNG preview of all icons")
    a = ap.parse_args()

    rows = []
    for suffix, out_dir, pens, rgb in PALETTES.values():
        os.makedirs(out_dir, exist_ok=True)
        grids = []
        for art, name, kind, tool, stack in ICONS:
            grid = load_art(f"gfx/{art}{suffix}", pens)
            data = build_icon(grid, kind, tool, stack)
            check_icon(data, grid, kind)
            out = f"{out_dir}/{name}"
            with open(out, "wb") as f:
                f.write(data)
            grids.append(grid)
            print(f"{out}: {len(grid[0])}x{len(grid)}, {len(data)} bytes")
        rows.append((rgb, grids))
    if a.preview:
        preview(rows, a.preview)


if __name__ == "__main__":
    main()
