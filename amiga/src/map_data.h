/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Generated world map (see tools/png2planar.py)
 */

#ifndef MAP_DATA_H
#define MAP_DATA_H

#define MAP_PLANE_BYTES (320 * 160 / 8)

/* Pens reserved by the palette layout in tools/png2planar.py */
#define PEN_OCEAN   0
#define PEN_LIGHT   4   /* night only: 4 + 16 is the city light colour */
#define PEN_TEXT    1
#define PEN_SHADOW  2
#define PEN_TRAIL   3
#define PEN_RULE    5
#define PEN_LABEL   6
#define PEN_ERROR   7

/* 12-bit colours for all 32 registers: 0-15 day, 16-31 night twins */
extern const unsigned short map_palette[32];

/* Bitplanes 0-3 of the map, 40 bytes per row */
extern const unsigned char map_planes[4][MAP_PLANE_BYTES];

/* City lights (NASA Black Marble): set bits glow on the night side */
extern const unsigned char map_lights[MAP_PLANE_BYTES];

#endif /* MAP_DATA_H */
