/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose UFO sightings: a blitter-drawn saucer and its curious pilot
 */

#ifndef UFO_H
#define UFO_H

#include <graphics/gfx.h>
#include <graphics/rastport.h>

/* Build the saucer images in chip RAM. Returns 0 if out of memory; every
 * sighting is then silently skipped. */
int ufo_open(void);
void ufo_close(void);

/* Begin a sighting on a random path. Returns 0 if one is already flying
 * or there are no images. */
int ufo_start(unsigned long *seed);

/* 1 while a saucer is flying */
int ufo_active(void);

/* Draw the next frame on rp (map row 0 at y0), taking the background
 * from map, the composed map. Returns 0 once the saucer has gone, with
 * the map under it restored. */
int ufo_frame(struct RastPort *rp, int y0, const struct BitMap *map);

#endif /* UFO_H */
