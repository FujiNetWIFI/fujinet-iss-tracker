#!/usr/bin/env python3
"""Generate map.bas (IntyBASIC data) from iss-intv.png.

Usage: python3 workfiles/png2map.py iss-intv.png map.bas workfiles/preview.png

The 159x96 source holds a 159x72 equirectangular world map (rows 0-71,
blue ocean / green land) over a black 24-row text area.  As 8x8 cards the
map needs 99 unique two-color coastline patterns, more than fit in the
STIC's 64 GRAM cards, so near-identical patterns are merged (greedy
minimum-Hamming-distance pairing, weighted per-pixel majority vote) until
they fit the budget:

    GRAM 0..61   merged coastline cards (FG green on color-stack blue)
    GRAM 62      solid green
    GRAM 63      reserved for the satellite MOB (defined by iss.bas)

Solid blue cells use GROM card 0 (blank) over the color-stack blue, which
costs no GRAM.  BACKTAB cell 180 (row 9, column 0) carries CS_ADVANCE so
rows 9-11 render on black for the text overlay; iss.bas must never write
that cell.

Also emits the lon/lat -> pixel lookup tables (the map is a clean linear
equirectangular fit: x = (lon+180)*159/360, y = (90-lat)*72/180) and the
canonical 8x8 satellite bitmap shared by the other clients.
"""

import sys
from PIL import Image

MAP_W, MAP_H = 159, 96          # source PNG (col 159 padded blue to reach 160)
CARD_COLS, MAP_CARD_ROWS = 20, 9
MAX_MIXED = 62                  # cards 0..61
SOLID_GREEN_CARD = 62
MAX_WRONG_PIXELS = 500

BLUE, GREEN, BLACK = (0, 0, 255), (0, 255, 0), (0, 0, 0)

CS_ADVANCE = 0x2000
GRAM = 0x0800
FG_GREEN = 5

SATELLITE_ROWS = [0x20, 0x50, 0xA4, 0x58, 0x1A, 0x05, 0x0A, 0x04]


def load_pixels(path):
    im = Image.open(path).convert("RGB")
    if im.size not in ((159, 96), (160, 96)):
        sys.exit(f"error: {path} is {im.size}, expected 159x96 (or 160x96)")
    px = im.load()

    def at(x, y):
        if x >= im.size[0]:
            return BLUE  # pad the missing 160th column with ocean
        return px[x, y]

    for y in range(96):
        for x in range(im.size[0]):
            c = at(x, y)
            if y < 72 and c not in (BLUE, GREEN):
                sys.exit(f"error: non-map color {c} at ({x},{y}) in map area")
            if y >= 72 and c != BLACK:
                sys.exit(f"error: non-black pixel {c} at ({x},{y}) in text area")
    return at


def extract_cards(at):
    """Return dict (col,row) -> 8-tuple of row bytes, bit 7 = leftmost pixel."""
    cards = {}
    for cy in range(MAP_CARD_ROWS):
        for cx in range(CARD_COLS):
            rows = []
            for dy in range(8):
                b = 0
                for dx in range(8):
                    b = (b << 1) | (1 if at(cx * 8 + dx, cy * 8 + dy) == GREEN else 0)
                rows.append(b)
            cards[(cx, cy)] = tuple(rows)
    return cards


def hamming(a, b):
    return sum(bin(x ^ y).count("1") for x, y in zip(a, b))


def merge_mixed(census):
    """census: pattern -> cell count. Greedy-merge to <= MAX_MIXED clusters.

    Returns (mapping pattern -> representative, list of representatives).
    """
    clusters = [{"rep": p, "weight": n, "members": {p}} for p, n in census.items()]
    while len(clusters) > MAX_MIXED:
        best = None
        for i in range(len(clusters)):
            for j in range(i + 1, len(clusters)):
                d = hamming(clusters[i]["rep"], clusters[j]["rep"])
                w = clusters[i]["weight"] + clusters[j]["weight"]
                if best is None or (d, w) < (best[0], best[1]):
                    best = (d, w, i, j)
        _, _, i, j = best
        a, b = clusters[i], clusters[j]
        # weighted per-pixel majority vote; ties go to green (keeps coastlines solid)
        rep = []
        for r in range(8):
            byte = 0
            for bit in range(8):
                mask = 0x80 >> bit
                green_w = (a["weight"] if a["rep"][r] & mask else 0) + \
                          (b["weight"] if b["rep"][r] & mask else 0)
                if green_w * 2 >= a["weight"] + b["weight"]:
                    byte |= mask
            rep.append(byte)
        a["rep"] = tuple(rep)
        a["weight"] += b["weight"]
        a["members"] |= b["members"]
        del clusters[j]
    mapping = {}
    for c in clusters:
        for m in c["members"]:
            mapping[m] = c["rep"]
    return mapping, [c["rep"] for c in clusters]


def card_words(rows):
    """Pack 8 row bytes into 4 DATA words; earlier row = LOW byte."""
    return [rows[i] | (rows[i + 1] << 8) for i in range(0, 8, 2)]


def data_lines(values, per_line, fmt):
    out = []
    for i in range(0, len(values), per_line):
        out.append("\tDATA " + ",".join(fmt(v) for v in values[i:i + per_line]))
    return out


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    src, dst, preview = sys.argv[1:4]

    at = load_pixels(src)
    cards = extract_cards(at)

    solid_blue = [k for k, p in cards.items() if all(b == 0x00 for b in p)]
    solid_green = [k for k, p in cards.items() if all(b == 0xFF for b in p)]
    mixed_cells = {k: p for k, p in cards.items() if k not in set(solid_blue) | set(solid_green)}
    census = {}
    for p in mixed_cells.values():
        census[p] = census.get(p, 0) + 1
    print(f"card census: {len(solid_blue)} solid blue, {len(solid_green)} solid green, "
          f"{len(census)} unique mixed ({len(mixed_cells)} cells)")

    mapping, reps = merge_mixed(census)
    wrong = sum(hamming(p, mapping[p]) for p in mixed_cells.values())
    print(f"merged to {len(reps)} GRAM cards; {wrong} of {MAP_W * 72} map pixels changed")
    assert len(reps) <= MAX_MIXED, "merge failed to reach GRAM budget"
    assert wrong < MAX_WRONG_PIXELS, f"merge damage too high ({wrong} px)"

    card_index = {rep: i for i, rep in enumerate(reps)}

    # 63 emitted GRAM cards: merged reps, zero padding, solid green at 62
    gram = [reps[i] if i < len(reps) else (0,) * 8 for i in range(SOLID_GREEN_CARD)]
    gram.append((0xFF,) * 8)

    # BACKTAB words
    screen = []
    for cy in range(MAP_CARD_ROWS):
        for cx in range(CARD_COLS):
            k = (cx, cy)
            if k in mixed_cells:
                n = card_index[mapping[mixed_cells[k]]]
            elif k in solid_green:
                n = SOLID_GREEN_CARD
            else:
                screen.append(0x0000)  # GROM blank over stack blue
                continue
            screen.append(GRAM + n * 8 + FG_GREEN)
    screen.append(CS_ADVANCE)           # cell 180: advance stack blue -> black
    screen.extend([0x0000] * 59)        # rest of rows 9-11

    lon2x = [i * 159 // 360 for i in range(361)]            # index = lon + 180
    lat2y = [min(71, (180 - i) * 72 // 180) for i in range(181)]  # index = lat + 90

    lines = [
        "\t' GENERATED by workfiles/png2map.py -- do not edit.",
        f"\t' Source: {src}  ({len(reps)} merged coastline cards, {wrong} px altered)",
        "",
    ]
    for block in range(4):
        lo = block * 16
        hi = min(lo + 16, len(gram))
        lines.append(f"\t' GRAM cards {lo}-{hi - 1}")
        lines.append(f"screen_bitmaps_{block}:")
        for n in range(lo, hi):
            lines.append("\tDATA " + ",".join(f"${w:04X}" for w in card_words(gram[n])))
        lines.append("")
    lines.append("\t' 8x8 satellite (canonical bitmap shared by all ISS tracker clients)")
    lines.append("satellite_bmp:")
    lines.append("\tDATA " + ",".join(f"${w:04X}" for w in card_words(SATELLITE_ROWS)))
    lines.append("")
    lines.append("\t' 20x12 BACKTAB; cell 180 = CS_ADVANCE (blue -> black), never overwrite it")
    lines.append("screen_cards:")
    lines.extend(data_lines(screen, 20, lambda v: f"${v:04X}"))
    lines.append("")
    lines.append("\t' map pixel X for longitude; index = lon + 180 (0..360)")
    lines.append("lon2x:")
    lines.extend(data_lines(lon2x, 16, str))
    lines.append("")
    lines.append("\t' map pixel Y for latitude; index = lat + 90 (0..180)")
    lines.append("lat2y:")
    lines.extend(data_lines(lat2y, 16, str))
    lines.append("")

    with open(dst, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {dst}")

    # Round-trip preview: decode the emitted words back into an image.
    im = Image.new("RGB", (160, 96), BLACK)
    px = im.load()
    for cy in range(MAP_CARD_ROWS):
        for cx in range(CARD_COLS):
            word = screen[cy * 20 + cx]
            if word & GRAM:
                rows = gram[(word >> 3) & 0xFF]
            else:
                rows = (0,) * 8
            for dy in range(8):
                for dx in range(8):
                    px[cx * 8 + dx, cy * 8 + dy] = \
                        GREEN if rows[dy] & (0x80 >> dx) else BLUE
    im.save(preview)
    diff = sum(1 for y in range(72) for x in range(160)
               if px[x, y] != at(x, y))
    print(f"wrote {preview} (round-trip diff vs source: {diff} px)")
    assert diff == wrong, "round-trip diff should equal merge damage"


if __name__ == "__main__":
    main()
