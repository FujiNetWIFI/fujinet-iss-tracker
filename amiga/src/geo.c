/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Geography and time maths (portable, integer only)
 */

#include "geo.h"

/* sin(0..90 degrees) in Q14 */
static const short sin_table[91] =
{
    0,286,572,857,1143,1428,1713,1997,2280,2563,
    2845,3126,3406,3686,3964,4240,4516,4790,5063,5334,
    5604,5872,6138,6402,6664,6924,7182,7438,7692,7943,
    8192,8438,8682,8923,9162,9397,9630,9860,10087,10311,
    10531,10749,10963,11174,11381,11585,11786,11982,12176,12365,
    12551,12733,12911,13085,13255,13421,13583,13741,13894,14044,
    14189,14330,14466,14598,14726,14849,14968,15082,15191,15296,
    15396,15491,15582,15668,15749,15826,15897,15964,16026,16083,
    16135,16182,16225,16262,16294,16322,16344,16362,16374,16382,
    16384
};

int geo_parse_hundredths(const char *s, long *out)
{
    long whole = 0;
    long frac = 0;
    int neg = 0;
    int digits = 0;
    int fdigits = 0;

    while (*s == ' ' || *s == '"')
        s++;
    if (*s == '-')
    {
        neg = 1;
        s++;
    }
    else if (*s == '+')
        s++;

    while (*s >= '0' && *s <= '9')
    {
        whole = whole * 10 + (*s++ - '0');
        digits++;
    }
    if (*s == '.')
    {
        s++;
        while (*s >= '0' && *s <= '9')
        {
            if (fdigits < 3)
                frac = frac * 10 + (*s - '0');
            fdigits++;
            digits++;
            s++;
        }
    }
    if (!digits)
        return 0;

    /* normalise frac to thousandths, then round to hundredths */
    while (fdigits < 3)
    {
        frac *= 10;
        fdigits++;
    }
    whole = whole * 100 + (frac + 5) / 10;
    *out = neg ? -whole : whole;
    return 1;
}

int geo_lon_to_x(long lon_h)
{
    long x = (lon_h + 18000L) * MAP_W / 36000L;

    if (x < 0)
        x = 0;
    if (x > MAP_W - 1)
        x = MAP_W - 1;
    return (int)x;
}

int geo_lat_to_y(long lat_h)
{
    long y = (9000L - lat_h) * MAP_H / 18000L;

    if (y < 0)
        y = 0;
    if (y > MAP_H - 1)
        y = MAP_H - 1;
    return (int)y;
}

void geo_utc(unsigned long ts, geo_tm *tm)
{
    /* civil_from_days() by Howard Hinnant, restricted to dates after 1970 */
    long z = (long)(ts / 86400UL) + 719468L;
    unsigned long secs = ts % 86400UL;
    long era = z / 146097L;
    long doe = z - era * 146097L;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    long d = doy - (153 * mp + 2) / 5 + 1;
    long m = mp < 10 ? mp + 3 : mp - 9;
    long y = yoe + era * 400 + (m <= 2);
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    static const short cumdays[12] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    tm->year = (int)y;
    tm->month = (int)m;
    tm->day = (int)d;
    tm->hour = (int)(secs / 3600UL);
    tm->minute = (int)((secs / 60UL) % 60UL);
    tm->second = (int)(secs % 60UL);
    tm->yday = cumdays[m - 1] + (int)d - 1 + (leap && m > 2);
}

long geo_sin(long a)
{
    long deg, rem, s;
    int neg = 0;

    a %= 36000L;
    if (a < 0)
        a += 36000L;
    if (a >= 18000L)
    {
        a -= 18000L;
        neg = 1;
    }
    if (a > 9000L)
        a = 18000L - a;

    deg = a / 100;
    rem = a % 100;
    s = sin_table[deg];
    if (rem)
        s += (sin_table[deg + 1] - s) * rem / 100;
    return neg ? -s : s;
}

long geo_cos(long a)
{
    return geo_sin(a + 9000L);
}

void geo_terminator(unsigned long ts, unsigned char edge[MAP_W],
                    unsigned char *night_below)
{
    static short row_sin[MAP_H];
    static short row_cos[MAP_H];
    static int rows_ready;
    geo_tm tm;
    long decl, sin_d, cos_d, b, eot, sun_lon;
    int x, y;

    if (!rows_ready)
    {
        for (y = 0; y < MAP_H; y++)
        {
            long lat = 9000L - ((2L * y + 1) * 18000L) / (2L * MAP_H);
            row_sin[y] = (short)geo_sin(lat);
            row_cos[y] = (short)geo_cos(lat);
        }
        rows_ready = 1;
    }

    geo_utc(ts, &tm);

    /* Solar declination, hundredths of a degree. */
    decl = -(2344L * geo_cos(((long)tm.yday + 10L) * 36000L / 365L)) / GEO_ONE;
    sin_d = geo_sin(decl);
    cos_d = geo_cos(decl);

    /* Equation of time, hundredths of a minute. */
    b = ((long)tm.yday - 81L) * 36000L / 365L;
    eot = (987L * geo_sin(2 * b) - 753L * geo_cos(b) - 150L * geo_sin(b))
          / GEO_ONE;

    /* Longitude where it is solar noon right now. */
    sun_lon = 18000L - (long)(ts % 86400UL) * 5L / 12L - eot / 4L;

    b = sin_d;
    *night_below = b >= 0;

    for (x = 0; x < MAP_W; x++)
    {
        long lon = ((2L * x + 1) * 36000L) / (2L * MAP_W) - 18000L;
        long a = cos_d * geo_cos(lon - sun_lon) / GEO_ONE;
        int lo = 0, hi = MAP_H;

        if (b == 0)
        {
            edge[x] = a < 0 ? 0 : MAP_H;
            continue;
        }

        /* Sun elevation changes sign exactly once from pole to pole:
         * find the first row on the far side of the terminator. */
        while (lo < hi)
        {
            int mid = (lo + hi) / 2;
            long f = b * row_sin[mid] + a * row_cos[mid];

            if (b > 0 ? f < 0 : f >= 0)
                hi = mid;
            else
                lo = mid + 1;
        }
        edge[x] = (unsigned char)lo;
    }
}

long geo_asin(long s)
{
    long lo = -9000L, hi = 9000L;

    if (s >= GEO_ONE)
        return 9000L;
    if (s <= -GEO_ONE)
        return -9000L;
    while (lo < hi)
    {
        long mid = lo + (hi - lo) / 2;

        if (geo_sin(mid) < s)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

long geo_acos(long c)
{
    return 9000L - geo_asin(c);
}

long geo_atan2(long y, long x)
{
    long ax = x < 0 ? -x : x;
    long ay = y < 0 ? -y : y;
    long lo = 0, hi = 9000L, a;

    if (!ax && !ay)
        return 0;
    /* keep the products below 2^31 */
    while (ax > 32767L || ay > 32767L)
    {
        ax >>= 1;
        ay >>= 1;
    }
    while (lo < hi)
    {
        long mid = (lo + hi) / 2;

        if (geo_sin(mid) * ax < geo_cos(mid) * ay)
            lo = mid + 1;
        else
            hi = mid;
    }
    a = lo;
    if (x < 0)
        a = 18000L - a;
    return y < 0 ? -a : a;
}

long geo_wrap_lon(long lon)
{
    lon %= 36000L;
    if (lon >= 18000L)
        lon -= 36000L;
    else if (lon < -18000L)
        lon += 36000L;
    return lon;
}

long geo_angle_between(long lat1, long lon1, long lat2, long lon2)
{
    long c = (geo_sin(lat1) * geo_sin(lat2)) / GEO_ONE +
             (geo_cos(lat1) * geo_cos(lat2) / GEO_ONE) *
             geo_cos(lon2 - lon1) / GEO_ONE;

    return geo_acos(c);
}

long geo_angle_to_km(long ang_h)
{
    /* 6371 km * pi / 180 = 111.19 km per degree */
    return ang_h * 11119L / 10000L;
}

void geo_destination(long lat, long lon, long brg, long ang,
                     long *lat2, long *lon2)
{
    long sl = geo_sin(lat), cl = geo_cos(lat);
    long sd = geo_sin(ang), cd = geo_cos(ang);
    long s2 = (sl * cd + cl * sd / GEO_ONE * geo_cos(brg)) / GEO_ONE;
    long y, x;

    *lat2 = geo_asin(s2);
    y = geo_sin(brg) * sd / GEO_ONE * cl / GEO_ONE;
    x = cd - sl * s2 / GEO_ONE;
    *lon2 = geo_wrap_lon(lon + geo_atan2(y, x));
}

int geo_extrapolate(long lat0, long lon0, unsigned long t0,
                    long lat1, long lon1, unsigned long t1,
                    long elapsed, long *lat, long *lon)
{
    long dt = (long)(t1 - t0);
    long dlon;

    *lat = lat1;
    *lon = lon1;
    if (t1 <= t0 || dt > 180 || elapsed <= 0)
        return 0;

    dlon = geo_wrap_lon(lon1 - lon0);
    *lat = lat1 + (lat1 - lat0) * elapsed / dt;
    if (*lat > 9000L)
        *lat = 9000L;
    if (*lat < -9000L)
        *lat = -9000L;
    *lon = geo_wrap_lon(lon1 + dlon * elapsed / dt);
    return 1;
}

int geo_circle_pixels(long lat, long lon, long ang_h, int points,
                      short *xs, short *ys)
{
    int i, j, n = 0;

    /* Step by index, not by bearing: 36000 / points need not be exact,
     * and a bearing loop would then produce one point too many. */
    for (i = 0; i < points; i++)
    {
        long lat2, lon2;
        int x, y, dup = 0;

        geo_destination(lat, lon, (long)i * 36000L / points, ang_h,
                        &lat2, &lon2);
        x = geo_lon_to_x(lon2);
        y = geo_lat_to_y(lat2);
        for (j = 0; j < n && !dup; j++)
            dup = xs[j] == x && ys[j] == y;
        if (!dup)
        {
            xs[n] = (short)x;
            ys[n] = (short)y;
            n++;
        }
    }
    return n;
}
