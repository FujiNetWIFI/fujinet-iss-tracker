/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Settings from icon ToolTypes or Shell arguments
 */

#include <stdlib.h>
#include "config.h"
#include "geo.h"

#define DEFAULT_SAVER_MINUTES 10

void config_defaults(config *c)
{
    c->have_lat = c->have_lon = 0;
    c->home_lat = c->home_lon = 0;
    c->saver_minutes = DEFAULT_SAVER_MINUTES;
    c->sound = 1;
}

static int upper(int ch)
{
    return ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
}

/* If arg is KEY=value (any case), return the value, else 0. */
static const char *value_of(const char *arg, const char *key)
{
    while (*key && upper(*arg) == *key)
    {
        arg++;
        key++;
    }
    return !*key && *arg == '=' ? arg + 1 : 0;
}

int config_arg(config *c, const char *arg)
{
    const char *v;
    long h;

    if ((v = value_of(arg, "HOMELAT")) != 0)
    {
        if (geo_parse_hundredths(v, &h) && h >= -9000 && h <= 9000)
        {
            c->home_lat = h;
            c->have_lat = 1;
        }
        return 1;
    }
    if ((v = value_of(arg, "HOMELON")) != 0)
    {
        if (geo_parse_hundredths(v, &h) && h >= -18000 && h <= 18000)
        {
            c->home_lon = geo_wrap_lon(h);
            c->have_lon = 1;
        }
        return 1;
    }
    if ((v = value_of(arg, "SAVER")) != 0)
    {
        c->saver_minutes = atoi(v);
        if (c->saver_minutes < 0)
            c->saver_minutes = 0;
        return 1;
    }
    if ((v = value_of(arg, "SOUND")) != 0)
    {
        c->sound = !(upper(v[0]) == 'O' && upper(v[1]) == 'F') && v[0] != '0';
        return 1;
    }
    return 0;
}

int config_has_home(const config *c)
{
    return c->have_lat && c->have_lon;
}
