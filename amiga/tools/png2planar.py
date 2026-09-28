#!/usr/bin/env python3
"""Convert the world map and night lights into Amiga planar data.

The map uses bitplanes 0-3 of a 5-plane (32 colour) lores screen. Bitplane 4
is the day/night mask, so every colour n has a darker "night" twin at n+16.
Some colour registers are claimed by other things and cannot be map colours:

  1-3, 5-7   UI pens (text, trail, panel). Their night twins 17-19 are
             the mouse pointer and 21-23 are the ISS hardware sprite.
  4          never part of the map: its night twin 20 is the city light
             colour, and 4 itself is the UFO pilot's green.

That leaves nine indices for terrain: 0 and 8-15. Index 0 is the most
common map colour (the ocean), which also makes it the panel background.

City lights come from NASA's Black Marble night imagery. The source's grey
land and ice background is subtracted, the remaining light is box-averaged
into each map pixel (so small towns still count) and saved as a greyscale
intensity map. Intensity then becomes the density of lit pixels through a
fixed per-pixel random threshold (an ordered dither would put towns on a
visible grid): metropolitan cores are solid, rural areas scattered.

Usage:
  png2planar.py gfx/map.png gfx/lights.png src/map_data.c [--preview out.png]
  png2planar.py --source land_shallow_topo_2048.jpg \
                --lights-source BlackMarble_2016_01deg.jpg \
                gfx/map.png gfx/lights.png src/map_data.c

With --source/--lights-source, the equirectangular source images are first
reduced to 320x160 and saved as the PNGs. Requires Pillow.
"""

import argparse
import sys

from PIL import Image, ImageEnhance

MAP_W, MAP_H = 320, 160
TERRAIN_SLOTS = [0] + list(range(8, 16))
LIGHT_PEN = 4                  # drawn as 4 + 16: only ever on the night side
LIGHT_RGB = (15, 13, 7)        # warm sodium-lamp glow
ALIEN_RGB = (4, 15, 2)         # little green man

# Black Marble: grey land/ice background stays below this; lights above it
LIGHT_FLOOR = 60
LIGHT_GAIN = 2.1               # scale on sqrt(intensity) before dithering

# 12-bit Amiga colours (r, g, b 0..15) for the UI pens and sprite registers.
UI = {
    1: (15, 15, 15),   # text / window title bar
    2: (0, 0, 0),      # shadow / outlines
    3: (15, 13, 0),    # ground track
    5: (10, 10, 11),   # panel rule / secondary text
    6: (0, 13, 15),    # panel labels
    7: (15, 4, 4),     # errors
    17: (14, 4, 4),    # mouse pointer (Intuition default colours)
    18: (0, 0, 0),
    19: (14, 14, 12),
    21: (15, 11, 0),   # ISS sprite: solar arrays (cycled at run time)
    22: (12, 12, 13),  # ISS sprite: truss and modules
    23: (15, 15, 15),  # ISS sprite: beacon (cycled at run time)
}


def to12(rgb):
    return tuple(min(15, (c + 8) // 17) for c in rgb)


def night(c):
    r, g, b = c
    return (r * 11 // 20, g * 11 // 20, min(15, b * 13 // 20 + 1))


def reduce_lights(source, out_png):
    """Black Marble -> 320x160 greyscale light intensity (0..255)."""
    src = Image.open(source).convert("L")
    lights = src.point(lambda v: max(0, v - LIGHT_FLOOR) * 255 // (255 - LIGHT_FLOOR))
    lights.resize((MAP_W, MAP_H), Image.BOX).save(out_png)


def noise(x, y):
    """Deterministic 0..1 threshold per pixel, so regenerating is stable."""
    h = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def light_mask(lights_png):
    img = Image.open(lights_png).convert("L")
    if img.size != (MAP_W, MAP_H):
        sys.exit(f"{lights_png} must be {MAP_W}x{MAP_H}, got {img.size}")
    mask = bytearray(MAP_W * MAP_H // 8)
    lit = []
    for i, v in enumerate(img.tobytes()):
        x, y = i % MAP_W, i // MAP_W
        level = min(1.0, (v / 255) ** 0.5 * LIGHT_GAIN)
        on = v > 0 and level > noise(x, y)
        lit.append(on)
        if on:
            mask[i // 8] |= 0x80 >> (i % 8)
    return mask, lit


def build(src_png, lights_png, out_c, preview):
    img = Image.open(src_png).convert("RGB")
    if img.size != (MAP_W, MAP_H):
        sys.exit(f"{src_png} must be {MAP_W}x{MAP_H}, got {img.size}")

    # Blue Marble is dark and muted; lift it so nine colours read well on a
    # TV. Octree keeps land hues distinct instead of spending most of the
    # palette on shades of ocean. Snap to 12-bit, then dither against the
    # snapped palette.
    img = ImageEnhance.Color(img).enhance(1.5)
    img = ImageEnhance.Brightness(img).enhance(1.2)
    q = img.quantize(colors=len(TERRAIN_SLOTS), method=Image.Quantize.FASTOCTREE,
                     dither=Image.Dither.NONE)
    raw = q.getpalette()[: 3 * len(TERRAIN_SLOTS)]
    cols12 = [to12(raw[i:i + 3]) for i in range(0, len(raw), 3)]
    if len(set(cols12)) != len(cols12):
        sys.exit(f"palette collapsed after 12-bit snapping: {cols12}")

    pal_img = Image.new("P", (1, 1))
    flat = []
    for c in cols12:
        flat += [v * 17 for v in c]
    pal_img.putpalette(flat)
    flat_px = list(img.quantize(palette=pal_img, dither=Image.Dither.NONE).tobytes())
    px = list(img.quantize(palette=pal_img, dither=Image.Dither.FLOYDSTEINBERG).tobytes())

    # Most common colour (ocean) goes to index 0. Error diffusion speckles
    # the open ocean, so keep it solid and dither only everything else.
    counts = [flat_px.count(i) for i in range(len(cols12))]
    order = sorted(range(len(cols12)), key=lambda i: -counts[i])
    ocean = order[0]
    px = [ocean if f == ocean else p for f, p in zip(flat_px, px)]
    slot_of = {src: TERRAIN_SLOTS[n] for n, src in enumerate(order)}

    palette = [(0, 0, 0)] * 32
    for src, slot in slot_of.items():
        palette[slot] = cols12[src]
        palette[slot + 16] = night(cols12[src])
    for idx, c in UI.items():
        palette[idx] = c
    palette[LIGHT_PEN] = ALIEN_RGB
    palette[LIGHT_PEN + 16] = LIGHT_RGB

    lights, lit = light_mask(lights_png)

    planes = [bytearray(MAP_W * MAP_H // 8) for _ in range(4)]
    for i, p in enumerate(px):
        v = slot_of[p]
        byte, bit = divmod(i, 8)
        for pl in range(4):
            if v & (1 << pl):
                planes[pl][byte] |= 0x80 >> bit

    with open(out_c, "w") as f:
        f.write("/* GENERATED by tools/png2planar.py from gfx/map.png and gfx/lights.png\n"
                " * - do not edit. Map: NASA Blue Marble; lights: NASA Black Marble\n"
                " * (both public domain), 320x160 equirectangular. */\n\n")
        f.write('#include "map_data.h"\n\n')
        f.write("const unsigned short map_palette[32] =\n{\n")
        for i in range(0, 32, 8):
            f.write("    " + ", ".join("0x%03X" % (r << 8 | g << 4 | b)
                                       for r, g, b in palette[i:i + 8]) + ",\n")
        f.write("};\n\n")
        f.write("const unsigned char map_planes[4][MAP_PLANE_BYTES] =\n{\n")
        for pl in planes:
            f.write("    {\n")
            for i in range(0, len(pl), 16):
                f.write("        " + ",".join("0x%02X" % b for b in pl[i:i + 16]) + ",\n")
            f.write("    },\n")
        f.write("};\n\n")
        f.write("const unsigned char map_lights[MAP_PLANE_BYTES] =\n{\n")
        for i in range(0, len(lights), 16):
            f.write("    " + ",".join("0x%02X" % b for b in lights[i:i + 16]) + ",\n")
        f.write("};\n")

    if preview:
        day = Image.new("RGB", (MAP_W, MAP_H * 2))
        for i, p in enumerate(px):
            v = slot_of[p]
            x, y = i % MAP_W, i // MAP_W
            day.putpixel((x, y), tuple(c * 17 for c in palette[v]))
            nv = LIGHT_PEN + 16 if lit[i] else v + 16
            day.putpixel((x, y + MAP_H), tuple(c * 17 for c in palette[nv]))
        day.resize((MAP_W * 2, MAP_H * 4), Image.NEAREST).save(preview)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", help="equirectangular source image to resize")
    ap.add_argument("--lights-source", help="Black Marble night image to reduce")
    ap.add_argument("--preview", help="write a day/night preview PNG")
    ap.add_argument("map_png")
    ap.add_argument("lights_png")
    ap.add_argument("out_c")
    a = ap.parse_args()

    if a.source:
        src = Image.open(a.source).convert("RGB")
        src.resize((MAP_W, MAP_H), Image.LANCZOS).save(a.map_png)
    if a.lights_source:
        reduce_lights(a.lights_source, a.lights_png)
    build(a.map_png, a.lights_png, a.out_c, a.preview)


if __name__ == "__main__":
    main()
