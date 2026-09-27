/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Ground track history
 */

#ifndef TRAIL_H
#define TRAIL_H

#include <graphics/rastport.h>
#include "fetch.h"

/* ~4 hours at one fix a minute: between two and three orbits */
#define TRAIL_LEN 256

void trail_add(const iss_pos *pos);

/* Draw the track as line segments in the current pen of rp, offset so
 * map row 0 is at y0. Segments across the date line or across a gap in
 * the fixes are skipped. */
void trail_draw(struct RastPort *rp, int y0);

#endif /* TRAIL_H */
