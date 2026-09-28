#!/usr/bin/env python3
"""Build .info icons for every Workbench from text pixel art.

Each gfx/*.icon.txt file is a grid with one character per pixel, naming the
four icon pens. Workbench 1.3 and 2.x+ colour those pens differently:

  char  pen   Workbench 1.3   Workbench 2.x+
   .     0    blue            grey     (background)
   W     1    white           black
   B     2    black           white
   O     3    orange          blue

One set of icons has to look right in both, so the art follows three rules:
colour comes from pen 3 (gold or blue both suit solar arrays, continents
and text), edges use pens 1 and 2 as a highlight/shadow pair (a bevel reads
as a bevel whichever way round), and large areas use pen 0 so nothing turns
into a solid black or white slab.

Icons are written in the original (OS 1.x) DiskObject format: two-bitplane
Image, complemented highlight, no OS 2.x extensions, so every Workbench
loads them. Standard library only; Pillow is used for --preview if present,
which renders the icons in both palettes.

Usage: mkinfo.py [--preview out.png]     (run from amiga/)
"""

import argparse
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

PENS = {".": 0, "W": 1, "B": 2, "O": 3}
WB13_RGB = [(0x00, 0x55, 0xAA), (0xFF, 0xFF, 0xFF), (0x00, 0x00, 0x22), (0xFF, 0x88, 0x00)]
WB2_RGB = [(0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x66, 0x88, 0xBB)]

ICONS = [
    # art file, output, type, default tool, stack
    ("gfx/isstracker.icon.txt", "icons/ISSTracker.info", WBTOOL, None, 8192),
    ("gfx/disk.icon.txt", "icons/Disk.info", WBDISK, "SYS:System/DiskCopy", 0),
    ("gfx/readme.icon.txt", "icons/ReadMe.info", WBPROJECT, "SYS:Utilities/More", 4096),
]


def load_art(path):
    with open(path) as f:
        rows = [line.rstrip("\n") for line in f if line.strip()]
    width = max(len(r) for r in rows)
    grid = []
    for y, row in enumerate(rows):
        row = row.ljust(width, ".")
        try:
            grid.append([PENS[c] for c in row])
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


def preview(images, path):
    """One row per Workbench palette: 1.3 on top, 2.x+ below."""
    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed: skipping preview")
        return
    scale = 3
    pad = 8
    width = sum(len(g[0]) for g in images) + pad * (len(images) + 1)
    row_h = max(len(g) for g in images) * 2 + pad * 2
    palettes = (WB13_RGB, WB2_RGB)
    img = Image.new("RGB", (width, row_h * len(palettes)))
    for r, rgb in enumerate(palettes):
        y0 = r * row_h
        img.paste(rgb[0], (0, y0, width, y0 + row_h))
        x0 = pad
        for g in images:
            for y, row in enumerate(g):
                for x, p in enumerate(row):
                    # Workbench runs in hires: pixels are twice as tall as wide
                    img.putpixel((x0 + x, y0 + pad + y * 2), rgb[p])
                    img.putpixel((x0 + x, y0 + pad + y * 2 + 1), rgb[p])
            x0 += len(g[0]) + pad
    img.resize((width * scale, row_h * len(palettes) * scale),
               Image.NEAREST).save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preview", help="write a PNG preview of all icons")
    a = ap.parse_args()

    grids = []
    for art, out, kind, tool, stack in ICONS:
        grid = load_art(art)
        data = build_icon(grid, kind, tool, stack)
        check_icon(data, grid, kind)
        with open(out, "wb") as f:
            f.write(data)
        grids.append(grid)
        print(f"{out}: {len(grid[0])}x{len(grid)}, {len(data)} bytes")
    if a.preview:
        preview(grids, a.preview)


if __name__ == "__main__":
    main()
