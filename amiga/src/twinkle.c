/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Twinkling city lights
 *
 * All lights share one colour register, so a twinkle is done per pixel:
 * a light is repainted in the night colour of the terrain under it for a
 * few ticks, then lit again.
 */

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include "geo.h"
#include "map_data.h"
#include "twinkle.h"

#define MAX_DIM 10
#define KEEP_DX 64       /* footprint half-width in pixels near 60N/S */
#define KEEP_DY 26

static unsigned short *lights;   /* y * MAP_W + x of every lit pixel */
static long nlights;
static unsigned long seed = 12345;

static struct
{
    unsigned short at;
    unsigned char ticks;
} dim[MAX_DIM];
static int ndim;

static unsigned long rnd(void)
{
    seed = seed * 1103515245UL + 12345UL;
    return seed >> 16;
}

int twinkle_init(void)
{
    long i, n = 0;

    for (i = 0; i < MAP_PLANE_BYTES; i++)
    {
        unsigned char b = map_lights[i];

        for (; b; b &= b - 1)
            n++;
    }
    lights = AllocMem(n * sizeof *lights, MEMF_ANY);
    if (!lights)
        return 0;
    for (i = 0; i < MAP_PLANE_BYTES * 8L; i++)
        if (map_lights[i / 8] & (0x80 >> (i % 8)))
            lights[nlights++] = (unsigned short)i;
    return 1;
}

void twinkle_free(void)
{
    if (lights)
        FreeMem(lights, nlights * sizeof *lights);
    lights = 0;
    nlights = 0;
}

void twinkle_reset(void)
{
    ndim = 0;
}

static int pixel(const struct BitMap *bm, long at)
{
    long byte = at / 8;
    int bit = 0x80 >> (at % 8), p, v = 0;

    for (p = 0; p < 5; p++)
        if (bm->Planes[p][byte] & bit)
            v |= 1 << p;
    return v;
}

static int terrain(long at)
{
    long byte = at / 8;
    int bit = 0x80 >> (at % 8), p, v = 0;

    for (p = 0; p < 4; p++)
        if (map_planes[p][byte] & bit)
            v |= 1 << p;
    return v;
}

void twinkle_tick(struct RastPort *rp, int y0, const struct BitMap *map,
                  int keep_x, int keep_y)
{
    int i;

    /* light up the ones whose time is over */
    for (i = 0; i < ndim;)
    {
        if (--dim[i].ticks == 0)
        {
            SetAPen(rp, PEN_LIGHT + 16);
            WritePixel(rp, dim[i].at % MAP_W, y0 + dim[i].at / MAP_W);
            dim[i] = dim[--ndim];
        }
        else
            i++;
    }

    if (!nlights || ndim >= MAX_DIM)
        return;

    {
        unsigned short at = lights[rnd() % nlights];
        int x = at % MAP_W, y = at / MAP_W;
        int dx = x - keep_x, dy = y - keep_y;

        if (dx < 0)
            dx = -dx;
        if (dx > MAP_W / 2)
            dx = MAP_W - dx;             /* the footprint wraps too */
        if (dy < 0)
            dy = -dy;
        if (dx < KEEP_DX && dy < KEEP_DY)
            return;
        if (pixel(map, at) != PEN_LIGHT + 16)
            return;                      /* daytime, or under the trail */
        for (i = 0; i < ndim; i++)
            if (dim[i].at == at)
                return;

        SetAPen(rp, terrain(at) + 16);
        WritePixel(rp, x, y0 + y);
        dim[ndim].at = at;
        dim[ndim].ticks = (unsigned char)(2 + rnd() % 5);
        ndim++;
    }
}
