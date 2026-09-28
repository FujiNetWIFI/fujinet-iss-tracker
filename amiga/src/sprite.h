/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose ISS hardware sprite with colour cycling, plus a spacewalker
 */

#ifndef SPRITE_H
#define SPRITE_H

#include <intuition/screens.h>

/* Allocate the ISS and astronaut sprites. Returns 0 if none is free (the
 * tracker still works, without them). */
int sprite_open(struct Screen *s);
void sprite_close(void);

/* Place the ISS centre at map pixel (x, y); the map starts at row top. */
void sprite_place(int x, int y, int top);

/* Send the astronaut out for one lap around the station (about 12 s at
 * ten ticks a second). Returns 1 if a spacewalk started, 0 if one is
 * already under way or there is no astronaut sprite. */
int sprite_spacewalk(void);

/* Hide the sprites while a window covers the map, or show them again. */
void sprite_suspend(int on);

/* Call ten times a second: colour cycling, spacewalk animation, hiding
 * while another screen is in front. Returns 1 when a spacewalk ends. */
int sprite_tick(void);

#endif /* SPRITE_H */
