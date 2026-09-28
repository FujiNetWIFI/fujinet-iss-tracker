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

/* Ten times a second: briefly dim a few random lights that are showing
 * on the night side of map (the composed off-screen bitmap), drawing on rp
 * with map row 0 at y0. Lights near (keep_x, keep_y) are left alone so
 * they never overlap the footprint dots. */
void twinkle_tick(struct RastPort *rp, int y0, const struct BitMap *map,
                  int keep_x, int keep_y);

#endif /* TWINKLE_H */
