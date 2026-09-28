/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Home location marker and the ISS visibility footprint
 */

#include <proto/graphics.h>
#include "geo.h"
#include "home.h"
#include "map_data.h"

#define FP_POINTS 64

static int known;
static long home_lat, home_lon;

static struct RastPort *fp_rp;
static int fp_y0;
static int fp_n;
static short fp_x[FP_POINTS], fp_y[FP_POINTS];
static unsigned char fp_under[FP_POINTS];   /* pens the dots cover */

void home_set(long lat_h, long lon_h)
{
    home_lat = lat_h;
    home_lon = lon_h;
    known = 1;
}

int home_known(void)
{
    return known;
}

static void dot(struct RastPort *rp, int x, int y)
{
    if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H)
        WritePixel(rp, x, y);
}

void home_draw_marker(struct RastPort *rp, int y0)
{
    int x, y, i;

    if (!known)
        return;
    x = geo_lon_to_x(home_lon);
    y = geo_lat_to_y(home_lat);

    /* black-edged cyan crosshair with an open centre */
    SetAPen(rp, PEN_SHADOW);
    for (i = 2; i <= 4; i++)
    {
        dot(rp, x - i, y0 + y - 1);
        dot(rp, x - i, y0 + y + 1);
        dot(rp, x + i, y0 + y - 1);
        dot(rp, x + i, y0 + y + 1);
        dot(rp, x - 1, y0 + y - i);
        dot(rp, x + 1, y0 + y - i);
        dot(rp, x - 1, y0 + y + i);
        dot(rp, x + 1, y0 + y + i);
    }
    SetAPen(rp, PEN_LABEL);
    for (i = 2; i <= 4; i++)
    {
        dot(rp, x - i, y0 + y);
        dot(rp, x + i, y0 + y);
        dot(rp, x, y0 + y - i);
        dot(rp, x, y0 + y + i);
    }
}

long home_distance_km(long lat_h, long lon_h)
{
    return geo_angle_to_km(geo_angle_between(home_lat, home_lon, lat_h, lon_h));
}

int home_in_view(long lat_h, long lon_h)
{
    return known &&
           geo_angle_between(home_lat, home_lon, lat_h, lon_h) < FOOTPRINT_H;
}

/* Index of (x, y) in the dot list, or -1 */
static int fp_find(const short *xs, const short *ys, int n, int x, int y)
{
    int i;

    for (i = 0; i < n; i++)
        if (xs[i] == x && ys[i] == y)
            return i;
    return -1;
}

static void fp_restore(int i)
{
    SetAPen(fp_rp, fp_under[i]);
    WritePixel(fp_rp, fp_x[i], fp_y0 + fp_y[i]);
}

void footprint_hide(void)
{
    int i;

    if (fp_rp)
        for (i = fp_n - 1; i >= 0; i--)
            fp_restore(i);
    fp_n = 0;
}

void footprint_forget(void)
{
    fp_n = 0;
}

void footprint_show(struct RastPort *rp, int y0, long lat_h, long lon_h)
{
    short nx[FP_POINTS], ny[FP_POINTS];
    unsigned char nu[FP_POINTS];
    int n, i, j;

    if (rp != fp_rp || y0 != fp_y0)
        footprint_hide();
    fp_rp = rp;
    fp_y0 = y0;

    /* at most FP_POINTS dots, one per pixel (keeps the saved pens simple) */
    n = geo_circle_pixels(lat_h, lon_h, FOOTPRINT_H, FP_POINTS, nx, ny);

    /* Move without flicker: put back only the dots that are not part of
     * the new circle, and paint only the dots that are new. */
    for (i = fp_n - 1; i >= 0; i--)
        if (fp_find(nx, ny, n, fp_x[i], fp_y[i]) < 0)
            fp_restore(i);

    SetDrMd(rp, JAM1);
    for (i = 0; i < n; i++)
    {
        j = fp_find(fp_x, fp_y, fp_n, nx[i], ny[i]);
        if (j >= 0)
            nu[i] = fp_under[j];
        else
        {
            nu[i] = (unsigned char)ReadPixel(rp, nx[i], y0 + ny[i]);
            SetAPen(rp, PEN_TEXT);
            WritePixel(rp, nx[i], y0 + ny[i]);
        }
    }

    for (i = 0; i < n; i++)
    {
        fp_x[i] = nx[i];
        fp_y[i] = ny[i];
        fp_under[i] = nu[i];
    }
    fp_n = n;
}
