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

/* Open the 32 colour screen and its window. Returns 0 on failure with a
 * reason in *why. */
int screen_open(struct Menu *menu, const char **why);
void screen_close(void);

/* Rebuild the map (terrain, optional night shading and lights, trail and
 * home marker) off screen and blit it where the map is shown. pos may be
 * NULL before the first fix. */
void screen_draw_map(const iss_pos *pos, int night, int trail);

/* Where the map is shown: the main window, or the screen saver. */
struct RastPort *screen_map_rp(void);
int screen_map_y(void);                 /* screen row of map row 0 */
const struct BitMap *screen_map_bitmap(void);   /* the composed map */

/* Screen saver: a full-screen window with just the map and the ISS, and
 * no pointer. Returns 1 if it is now showing. */
int screen_saver(int on);
struct Window *screen_saver_window(void);

/* A window centred over the map, filled with the ocean colour, for a
 * list; 0 if it can't be opened. screen_popup_wait() then waits for a
 * key, a click or the close gadget, and closes it. */
struct Window *screen_popup(const char *title, int width, int height);
void screen_popup_wait(struct Window *w);

/* Status panel. */
void screen_draw_position(const iss_pos *pos);
void screen_status(const char *text, int pen);
void screen_countdown(int secs);
void screen_home(const char *text, int pen);   /* PAL only */

#endif /* SCREEN_H */
