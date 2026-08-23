# intv map pipeline

`../map.bas` is generated from `../iss-intv.png` by:

```
python3 workfiles/png2map.py iss-intv.png map.bas workfiles/preview.png
```

(or `make regen-map`). The generated `map.bas` is committed, so normal
builds need only `intybasic`/`as1600` — Python 3 + Pillow are required
only to regenerate it after changing the PNG.

The source map is a 159x72 equirectangular projection (rows 0-71 of the
PNG; rows 72-95 are the black text area). It contains 99 unique
two-color 8x8 coastline cards — more than the STIC's 64 GRAM cards — so
the script greedily merges the most similar patterns (minimum Hamming
distance, weighted per-pixel majority vote) down to 62. Today that
alters 159 of 11448 map pixels (~1.4%); the script prints the stats and
asserts the damage stays small. `preview.png` is a round-trip decode of
the emitted data for visual comparison against the source.

GRAM budget: cards 0-61 merged coastline patterns, 62 solid green,
63 the satellite MOB (defined by `iss.bas`). Solid blue cells cost no
GRAM (GROM blank card over the color-stack blue). BACKTAB cell 180
carries the CS_ADVANCE bit that switches the color stack from blue to
black for the three text rows — nothing may ever overwrite that cell.

The script also emits the longitude/latitude → map pixel lookup tables
(`lon2x`, `lat2y`) and the canonical 8x8 satellite bitmap shared by the
other ISS tracker clients.
