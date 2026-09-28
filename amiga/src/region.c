/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose "What's below": country or ocean at a position
 */

#include "geo.h"
#include "region.h"
#include "region_data.h"

const char *region_name(long lat_h, long lon_h)
{
    long row = (9000L - lat_h) / 100;
    long col = (geo_wrap_lon(lon_h) + 18000L) / 100;
    const unsigned char *run;

    if (row < 0)
        row = 0;
    if (row > REGION_H - 1)
        row = REGION_H - 1;
    if (col > REGION_W - 1)
        col = REGION_W - 1;

    run = region_runs + region_rows[row];
    while (col >= run[0])
    {
        col -= run[0];
        run += 2;
    }
    return region_names[run[1]];
}
