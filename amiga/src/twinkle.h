/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Twinkling city lights
 */

#ifndef TWINKLE_H
#define TWINKLE_H

#include <graphics/gfx.h>
#include <graphics/rastport.h>

/* Index the lit pixels of the light map. Returns 0 if out of memory. */
int twinkle_init(void);
void twinkle_free(void);

/* The map was redrawn: every dimmed light is lit again. */
void twinkle_reset(void);

/* Call ten times a second: dim random lights shown in map (the composed
 * bitmap) by drawing on rp, map row 0 at y0. Lights near (keep_x, keep_y),
 * the footprint's area, are left alone. */
void twinkle_tick(struct RastPort *rp, int y0, const struct BitMap *map,
                  int keep_x, int keep_y);

#endif /* TWINKLE_H */
