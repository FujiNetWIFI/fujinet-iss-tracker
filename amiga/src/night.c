/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Day/night shading
 */

#include "geo.h"
#include "night.h"

#define BYTES (MAP_W / 8)

void night_fill(unsigned char *plane, int bytes_per_row, unsigned long ts)
{
    static unsigned char edge[MAP_W];
    static unsigned char emin[BYTES];
    static unsigned char emax[BYTES];
    unsigned char below;
    int x, y, b;

    geo_terminator(ts, edge, &below);

    for (b = 0; b < BYTES; b++)
    {
        unsigned char lo = 255, hi = 0;

        for (x = b * 8; x < b * 8 + 8; x++)
        {
            if (edge[x] < lo)
                lo = edge[x];
            if (edge[x] > hi)
                hi = edge[x];
        }
        emin[b] = lo;
        emax[b] = hi;
    }

    for (y = 0; y < MAP_H; y++)
    {
        unsigned char *row = plane + (long)y * bytes_per_row;

        for (b = 0; b < BYTES; b++)
        {
            unsigned char v = 0;

            /* Whole byte on one side of the terminator? Most are, which
             * keeps this to a fraction of a second on a 68000. */
            if (below ? y > emax[b] : y + 2 <= emin[b])
                v = 0xFF;
            else if (below ? y < emin[b] : y >= emax[b])
                v = 0x00;
            else
            {
                for (x = 0; x < 8; x++)
                {
                    int e = edge[b * 8 + x];
                    int d = below ? y - e : e - 1 - y;

                    if (d >= 1 || (d == 0 && ((b * 8 + x + y) & 1)))
                        v |= 0x80 >> x;
                }
            }
            row[b] = v;
        }
    }
}
