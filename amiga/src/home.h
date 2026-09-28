/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Home location marker and the ISS visibility footprint
 */

#ifndef HOME_H
#define HOME_H

#include <graphics/rastport.h>

/* ISS horizon radius at ~420 km: acos(R / (R + h)) = 20.3 degrees */
#define FOOTPRINT_H 2030L

void home_set(long lat_h, long lon_h);
int home_known(void);

/* Draw the home crosshair into a layerless map rastport, map row 0 at y0. */
void home_draw_marker(struct RastPort *rp, int y0);

/* Distance in km from home to the ISS at (lat, lon); returns 1 if the ISS
 * is above home's horizon. Needs home_known(). */
int home_check(long lat_h, long lon_h, long *km);

/* Draw the footprint around (lat, lon) on rp, map row 0 at y0, moving the
 * previous one. Covered pixels are saved and restored. */
void footprint_show(struct RastPort *rp, int y0, long lat_h, long lon_h);

/* Remove the footprint (before anything else draws over it). */
void footprint_hide(void);

/* The map under the footprint was redrawn: it is gone already. */
void footprint_forget(void);

#endif /* HOME_H */
