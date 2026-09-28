/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Ground track history
 */

#include <proto/graphics.h>
#include "geo.h"
#include "trail.h"

/* Join fixes at most this far apart; the ISS covers ~12 degrees of
 * longitude in 3 minutes, so a longer gap would draw a wrong chord. */
#define MAX_JOIN_SECS 180UL

typedef struct
{
    unsigned long ts;
    short x;
    short y;
} fix;

static fix ring[TRAIL_LEN];
static int head;     /* next slot to write */
static int count;

void trail_add(const iss_pos *pos)
{
    int last = (head + TRAIL_LEN - 1) % TRAIL_LEN;

    if (count && ring[last].ts == pos->ts)
        return;

    ring[head].ts = pos->ts;
    ring[head].x = (short)geo_lon_to_x(pos->lon_h);
    ring[head].y = (short)geo_lat_to_y(pos->lat_h);
    head = (head + 1) % TRAIL_LEN;
    if (count < TRAIL_LEN)
        count++;
}

void trail_draw(struct RastPort *rp, int y0)
{
    int i = (head + TRAIL_LEN - count) % TRAIL_LEN;
    int n;
    const fix *prev = 0;

    for (n = 0; n < count; n++)
    {
        const fix *f = &ring[i];
        int dx = prev ? f->x - prev->x : 0;

        if (prev && f->ts - prev->ts <= MAX_JOIN_SECS &&
            dx < MAP_W / 2 && dx > -MAP_W / 2)
        {
            Draw(rp, f->x, y0 + f->y);
        }
        else
        {
            Move(rp, f->x, y0 + f->y);
            WritePixel(rp, f->x, y0 + f->y);
        }
        prev = f;
        i = (i + 1) % TRAIL_LEN;
    }
}
