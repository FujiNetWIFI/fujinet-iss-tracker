/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Generated country/ocean grid (see tools/mkregions.py)
 */

#ifndef REGION_DATA_H
#define REGION_DATA_H

#define REGION_W 360           /* 1 degree cells from 180W */
#define REGION_H 180           /* from 90N */

/* Names, Latin-1 (the ROM topaz font has the accented letters) */
extern const char *const region_names[];

/* Byte offset of each row in region_runs */
extern const unsigned short region_rows[REGION_H];

/* Each row is (count, name index) pairs covering REGION_W cells */
extern const unsigned char region_runs[];

#endif /* REGION_DATA_H */
