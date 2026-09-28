/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Home location marker and the ISS visibility footprint
 */

#ifndef HOME_H
#define HOME_H

#include <graphics/rastport.h>

/* Great-circle radius of the ISS's horizon circle: from ~420 km up it can
 * see (and be seen from) points up to acos(R / (R + h)) = 20.3 deg away. */
#define FOOTPRINT_H 2030L

void home_set(long lat_h, long lon_h);
int home_known(void);

/* Draw the home crosshair into a map bitmap rastport (no layers, map
 * row 0 at y0). */
void home_draw_marker(struct RastPort *rp, int y0);

/* Distance from home to (lat, lon) in km, and whether that point is within
 * the ISS footprint of home. */
long home_distance_km(long lat_h, long lon_h);
int home_in_view(long lat_h, long lon_h);

/* Show the footprint around (lat, lon) on rp, map row 0 at y0, erasing
 * the previous one. The dots are white; the pixels under them are saved
 * and put back when the footprint moves. */
void footprint_show(struct RastPort *rp, int y0, long lat_h, long lon_h);

/* Remove the footprint (before anything else draws over it). */
void footprint_hide(void);

/* The map under the footprint was redrawn: it is gone already. */
void footprint_forget(void);

#endif /* HOME_H */
