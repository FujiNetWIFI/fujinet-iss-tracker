/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose ISS hardware sprite with colour cycling
 */

#ifndef SPRITE_H
#define SPRITE_H

#include <intuition/screens.h>

/* Allocate a hardware sprite for the ISS. Returns 0 if none is free; the
 * tracker still works, just without the marker. */
int sprite_open(struct Screen *s);
void sprite_close(void);

/* Place the ISS centre at map pixel (x, y); the map starts at row top. */
void sprite_place(int x, int y, int top);

/* Hide the sprite (it would show through windows) or show it again. */
void sprite_suspend(int on);

/* Advance the colour cycle and keep the sprite glued to the screen (it is
 * hidden while another screen is in front). Call a few times a second. */
void sprite_tick(void);

#endif /* SPRITE_H */
