/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Custom screen, main window, map compositor and status panel
 */

#ifndef SCREEN_H
#define SCREEN_H

#include <intuition/intuition.h>
#include "fetch.h"

extern struct Screen *scr;
extern struct Window *win;
extern int map_top;          /* first map row in window/screen coordinates */

/* Open the 32 colour screen and its window. Returns 0 on failure with a
 * reason in *why. */
int screen_open(struct Menu *menu, const char **why);
void screen_close(void);

/* Rebuild the map (terrain, optional night shading and trail) off screen
 * and blit it into the window. pos may be NULL before the first fix. */
void screen_draw_map(const iss_pos *pos, int night, int trail);

/* Status panel. */
void screen_draw_position(const iss_pos *pos);
void screen_status(const char *text, int pen);
void screen_countdown(int secs);

#endif /* SCREEN_H */
