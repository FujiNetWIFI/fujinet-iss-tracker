/*
 * Host-side tests for the portable ISS Tracker logic (JSON, geo, time,
 * terminator).  Build and run with `make test` in amiga/.
 */

#include <stdio.h>
#include <string.h>
#include "../src/geo.h"
#include "../src/json.h"
#include "../src/night.h"
#include "../src/region.h"
#include "../src/config.h"
#include "../src/ufo_path.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static const char iss_now[] =
    "{\"timestamp\": 1790544840, \"message\": \"success\", \"iss_position\": "
    "{\"latitude\": \"11.6291\", \"longitude\": \"-9.0523\"}}";

static const char astros[] =
    "{\"people\": [{\"craft\": \"ISS\", \"name\": \"Oleg Kononenko\"}, "
    "{\"craft\": \"ISS\", \"name\": \"Tracy Caldwell Dyson\"}, "
    "{\"craft\": \"Tiangong\", \"name\": \"Li \\\"Cong\\\" \\u00e9\"}], "
    "\"number\": 3, \"message\": \"success\"}";

static void test_json(void)
{
    char buf[40];
    const char *end = iss_now + strlen(iss_now);
    const char *cur = 0, *obj, *obj_end;
    const char *aend = astros + strlen(astros);
    int n = 0;

    CHECK(json_get(iss_now, end, "timestamp", buf, sizeof buf));
    CHECK(!strcmp(buf, "1790544840"));
    CHECK(json_get(iss_now, end, "latitude", buf, sizeof buf));
    CHECK(!strcmp(buf, "11.6291"));
    CHECK(json_get(iss_now, end, "longitude", buf, sizeof buf));
    CHECK(!strcmp(buf, "-9.0523"));
    CHECK(json_get(iss_now, end, "message", buf, sizeof buf));
    CHECK(!strcmp(buf, "success"));
    CHECK(!json_get(iss_now, end, "altitude", buf, sizeof buf));
    CHECK(json_get(iss_now, end, "latitude", buf, 4) && !strcmp(buf, "11."));

    CHECK(json_get(astros, aend, "number", buf, sizeof buf));
    CHECK(!strcmp(buf, "3"));
    while (json_next_object(astros, aend, "people", &cur, &obj, &obj_end))
    {
        CHECK(json_get(obj, obj_end, "name", buf, sizeof buf));
        if (n == 0) CHECK(!strcmp(buf, "Oleg Kononenko"));
        if (n == 1) CHECK(!strcmp(buf, "Tracy Caldwell Dyson"));
        if (n == 2) CHECK(!strcmp(buf, "Li \"Cong\" ?"));
        CHECK(json_get(obj, obj_end, "craft", buf, sizeof buf));
        if (n == 2) CHECK(!strcmp(buf, "Tiangong"));
        n++;
    }
    CHECK(n == 3);

    /* truncated response: no complete object */
    cur = 0;
    CHECK(!json_next_object(astros, astros + 20, "people", &cur, &obj, &obj_end));
}

static void test_parse(void)
{
    long v;

    CHECK(geo_parse_hundredths("11.6291", &v) && v == 1163);
    CHECK(geo_parse_hundredths("-9.0523", &v) && v == -905);
    CHECK(geo_parse_hundredths("-179.995", &v) && v == -18000);
    CHECK(geo_parse_hundredths("45", &v) && v == 4500);
    CHECK(geo_parse_hundredths("0.5", &v) && v == 50);
    CHECK(!geo_parse_hundredths("abc", &v));
}

static void test_pixels(void)
{
    CHECK(geo_lon_to_x(-18000) == 0);
    CHECK(geo_lon_to_x(0) == MAP_W / 2);
    CHECK(geo_lon_to_x(17999) == MAP_W - 1);
    CHECK(geo_lon_to_x(18000) == MAP_W - 1);
    CHECK(geo_lat_to_y(9000) == 0);
    CHECK(geo_lat_to_y(0) == MAP_H / 2);
    CHECK(geo_lat_to_y(-9000) == MAP_H - 1);
}

static void test_utc(void)
{
    geo_tm tm;

    geo_utc(0, &tm);
    CHECK(tm.year == 1970 && tm.month == 1 && tm.day == 1 && tm.yday == 0);

    geo_utc(1790544840UL, &tm);
    CHECK(tm.year == 2026 && tm.month == 9 && tm.day == 27);
    CHECK(tm.hour == 21 && tm.minute == 34 && tm.second == 0);
    CHECK(tm.yday == 269);

    geo_utc(1709251199UL, &tm);   /* 2024-02-29 23:59:59 */
    CHECK(tm.year == 2024 && tm.month == 2 && tm.day == 29);
    CHECK(tm.hour == 23 && tm.minute == 59 && tm.second == 59);
    CHECK(tm.yday == 59);
}

static void test_trig(void)
{
    CHECK(geo_sin(0) == 0);
    CHECK(geo_sin(9000) == GEO_ONE);
    CHECK(geo_sin(-9000) == -GEO_ONE);
    CHECK(geo_sin(3000) == 8192);
    CHECK(geo_cos(18000) == -GEO_ONE);
    CHECK(geo_sin(3050) > 8192 && geo_sin(3050) < 8438);
}

static int near(int a, int b, int tol)
{
    return a >= b - tol && a <= b + tol;
}

static void test_terminator(void)
{
    unsigned char edge[MAP_W];
    unsigned char below;
    int x;

    /* June solstice, 12:00 UTC: sun over ~23.4N, ~1.5E */
    geo_terminator(1782043200UL, edge, &below);
    CHECK(below == 1);
    /* noon meridian: night only beyond the Antarctic circle (66.6S) */
    CHECK(near(edge[MAP_W / 2], geo_lat_to_y(-6656), 2));
    /* midnight meridian: day only inside the Arctic circle (66.6N) */
    CHECK(near(edge[0], geo_lat_to_y(6656), 2));
    CHECK(near(edge[MAP_W - 1], geo_lat_to_y(6656), 2));
    /* 90 degrees west of the sun the terminator crosses the equator */
    x = geo_lon_to_x(-9000 + 150);
    CHECK(near(edge[x], MAP_H / 2, 3));

    /* December solstice, 00:00 UTC: sun over ~23.4S near 180 */
    geo_terminator(1797811200UL, edge, &below);
    CHECK(below == 0);
    /* Greenwich is at midnight: night down to the Antarctic circle */
    CHECK(near(edge[MAP_W / 2], geo_lat_to_y(-6656), 2));
}

/* The byte-at-a-time fast path must match a per-pixel reference. */
static void test_night_plane(void)
{
    static unsigned char plane[MAP_W / 8 * MAP_H];
    unsigned char edge[MAP_W];
    unsigned char below;
    unsigned long times[3] = { 1782043200UL, 1797811200UL, 1790544840UL };
    int t, x, y, bad = 0;

    for (t = 0; t < 3; t++)
    {
        night_fill(plane, MAP_W / 8, times[t]);
        geo_terminator(times[t], edge, &below);
        for (y = 0; y < MAP_H; y++)
            for (x = 0; x < MAP_W; x++)
            {
                int d = below ? y - edge[x] : edge[x] - 1 - y;
                int want = d >= 1 || (d == 0 && ((x + y) & 1));
                int got = (plane[y * (MAP_W / 8) + x / 8] >> (7 - (x & 7))) & 1;

                if (want != got)
                    bad++;
            }
    }
    CHECK(bad == 0);

    /* June solstice noon: London (51.5N, 0) is day, Sydney is night */
    night_fill(plane, MAP_W / 8, 1782043200UL);
    x = geo_lon_to_x(0);
    y = geo_lat_to_y(5150);
    CHECK(!((plane[y * (MAP_W / 8) + x / 8] >> (7 - (x & 7))) & 1));
    x = geo_lon_to_x(15120);
    y = geo_lat_to_y(-3387);
    CHECK((plane[y * (MAP_W / 8) + x / 8] >> (7 - (x & 7))) & 1);
}

/* Lights only replace pixels on the night side, with pen 4 + 16. */
static void test_night_lights(void)
{
    enum { BYTES = MAP_W / 8 * MAP_H };
    static unsigned char pl[5][BYTES];
    static unsigned char lights[BYTES];
    unsigned char *planes[5];
    int i, bad = 0, lit = 0, day = 0;

    for (i = 0; i < 5; i++)
        planes[i] = pl[i];
    memset(pl, 0, sizeof pl);
    memset(pl[0], 0xFF, BYTES);          /* terrain pen 9 = planes 0 and 3 */
    memset(pl[3], 0xFF, BYTES);
    for (i = 0; i < BYTES; i++)
        lights[i] = (i & 1) ? 0x0F : 0xF0;
    night_fill(pl[4], MAP_W / 8, 1782043200UL);
    night_lights(planes, lights, BYTES);

    for (i = 0; i < MAP_W * MAP_H; i++)
    {
        int byte = i / 8, bit = 0x80 >> (i % 8), v = 0, p;
        int night = (pl[4][byte] & bit) != 0;
        int want;

        for (p = 0; p < 5; p++)
            if (pl[p][byte] & bit)
                v |= 1 << p;
        want = night && (lights[byte] & bit) ? 20 : night ? 25 : 9;
        if (v != want)
            bad++;
        if (v == 20)
            lit++;
        if (v == 9)
            day++;
    }
    CHECK(bad == 0);
    CHECK(lit > 0);
    CHECK(day > 0);
}

static int within(long a, long b, long tol)
{
    return a >= b - tol && a <= b + tol;
}

static void test_inverse_trig(void)
{
    CHECK(within(geo_asin(8192), 3000, 2));
    CHECK(geo_asin(GEO_ONE) == 9000 && geo_asin(-GEO_ONE) == -9000);
    CHECK(within(geo_acos(0), 9000, 1));
    CHECK(within(geo_acos(8192), 6000, 2));
    CHECK(within(geo_atan2(100, 100), 4500, 2));
    CHECK(within(geo_atan2(-100, -100), -13500, 2));
    CHECK(within(geo_atan2(0, -100), 18000, 1));
    CHECK(within(geo_atan2(100, 0), 9000, 1));
    CHECK(geo_wrap_lon(18000) == -18000 && geo_wrap_lon(-18100) == 17900);
}

static void test_great_circle(void)
{
    long lat, lon, a;
    int b;

    CHECK(within(geo_angle_between(0, 0, 0, 9000), 9000, 5));
    /* London - New York is about 5570 km */
    a = geo_angle_between(5151, -13, 4071, -7401);
    CHECK(within(geo_angle_to_km(a), 5570, 40));

    geo_destination(0, 0, 9000, 2000, &lat, &lon);
    CHECK(within(lat, 0, 3) && within(lon, 2000, 5));
    geo_destination(0, 0, 0, 2000, &lat, &lon);
    CHECK(within(lat, 2000, 3) && within(lon, 0, 3));
    /* footprint points really are 20.3 degrees away, even across the
     * date line and at the ISS's highest latitude */
    for (b = 0; b < 36000; b += 4500)
    {
        geo_destination(5160, 17000, b, 2030, &lat, &lon);
        CHECK(within(geo_angle_between(5160, 17000, lat, lon), 2030, 15));
    }
}

/* Regression: a bearing loop stepping 36000/64 = 562 produced 65 points
 * and overran 64-entry arrays on the stack (Line-F crash on real
 * hardware). Guard words either side catch any overrun. */
static void test_circle_bounds(void)
{
    struct { short guard0[4]; short xs[64]; short guard1[4]; } a;
    struct { short guard0[4]; short ys[64]; short guard1[4]; } b;
    long lat, lon;
    int n, i, most = 0, bad = 0;

    for (lat = -5200; lat <= 5200; lat += 400)
        for (lon = -18000; lon < 18000; lon += 1300)
        {
            for (i = 0; i < 4; i++)
                a.guard0[i] = a.guard1[i] = b.guard0[i] = b.guard1[i] = 0x5A5A;
            n = geo_circle_pixels(lat, lon, 2030, 64, a.xs, b.ys);
            if (n > most)
                most = n;
            for (i = 0; i < 4; i++)
                if (a.guard0[i] != 0x5A5A || a.guard1[i] != 0x5A5A ||
                    b.guard0[i] != 0x5A5A || b.guard1[i] != 0x5A5A)
                    bad++;
            for (i = 0; i < n; i++)
                if (a.xs[i] < 0 || a.xs[i] >= MAP_W ||
                    b.ys[i] < 0 || b.ys[i] >= MAP_H)
                    bad++;
        }
    CHECK(bad == 0);
    CHECK(most == 64);      /* the full circle is used, and never more */
}

static void test_extrapolate(void)
{
    long lat, lon;

    /* crossing the date line eastwards */
    CHECK(geo_extrapolate(0, 17900, 1000, 100, -17900, 1060, 30, &lat, &lon));
    CHECK(lat == 150 && lon == -17800);
    /* stale or unordered fixes are not used */
    CHECK(!geo_extrapolate(0, 0, 1000, 100, 100, 1300, 30, &lat, &lon));
    CHECK(lat == 100 && lon == 100);
    CHECK(!geo_extrapolate(0, 0, 1060, 100, 100, 1060, 30, &lat, &lon));
}

static void test_regions(void)
{
    CHECK(!strcmp(region_name(5150, -10), "United Kingdom"));
    CHECK(!strcmp(region_name(3570, 13970), "Japan"));
    CHECK(!strcmp(region_name(0, -3000), "South Atlantic Ocean"));
    CHECK(!strcmp(region_name(2000, -15500), "North Pacific Ocean"));
    CHECK(!strcmp(region_name(-8000, 0), "Antarctica"));
    CHECK(!strcmp(region_name(-9000, 17999), "Antarctica"));
    CHECK(!strcmp(region_name(9000, -18000), "Arctic Ocean"));
}

static void test_config(void)
{
    config c;

    config_defaults(&c);
    CHECK(!config_has_home(&c) && c.saver_minutes == 10 && c.sound &&
          c.circle);
    CHECK(config_arg(&c, "HOMELAT=40.71"));
    CHECK(!config_has_home(&c));
    CHECK(config_arg(&c, "homelon=-74.01"));
    CHECK(config_has_home(&c) && c.home_lat == 4071 && c.home_lon == -7401);
    CHECK(config_arg(&c, "HOMELAT=95"));        /* out of range: ignored */
    CHECK(c.home_lat == 4071);
    CHECK(config_arg(&c, "SAVER=0") && c.saver_minutes == 0);
    CHECK(config_arg(&c, "Sound=Off") && !c.sound);
    CHECK(config_arg(&c, "SOUND=ON") && c.sound);
    CHECK(config_arg(&c, "circle=off") && !c.circle);
    CHECK(config_arg(&c, "CIRCLE=1") && c.circle);
    CHECK(!config_arg(&c, "(HOMELAT=1)"));      /* bracketed ToolType */
    CHECK(!config_arg(&c, "HOMELATX=1"));
    CHECK(!config_arg(&c, "WINDOW=CON:"));
}

static int art_ok(const ufo_art *a)
{
    int r;

    for (r = 0; r < a->h; r++)
        if ((int)strlen(a->rows[r]) != a->w)
            return 0;
    return 1;
}

static void test_ufo_art(void)
{
    int i;

    for (i = 0; i < UFO_SIZES; i++)
        CHECK(art_ok(&ufo_body[i]));
    for (i = 1; i < UFO_SIZES; i++)
        CHECK(ufo_body[i].w > ufo_body[i - 1].w);
    for (i = 0; i < UFO_LOOKS; i++)
    {
        CHECK(art_ok(&ufo_head[i]));
        CHECK(ufo_head[i].w == UFO_HEAD_W && ufo_head[i].h == UFO_HEAD_H);
    }
    CHECK(UFO_HEAD_W < ufo_body[UFO_SIZES - 1].w);
}

/* Which side of the map a point is off, or -1 if it is on the map */
static int off_side(int x, int y)
{
    if (x < 0) return 0;
    if (x >= MAP_W) return 1;
    if (y < 0) return 2;
    if (y >= MAP_H) return 3;
    return -1;
}

static void test_ufo_path(void)
{
    unsigned long n, seed;
    ufo_path p;
    ufo_pose o, first, last;
    int step, bad = 0, prev_size, looks, max_rise;

    for (n = 1; n < 400; n += 3)
    {
        seed = n;
        ufo_path_init(&p, &seed);
        prev_size = 0;
        looks = 0;
        max_rise = 0;
        for (step = 0; step < UFO_STEPS; step++)
        {
            if (!ufo_path_step(&p, step, &o))
            {
                bad++;
                continue;
            }
            /* off the map only near either end */
            if (o.x < -40 || o.x >= MAP_W + 40 || o.y < -40 || o.y >= MAP_H + 40)
                bad++;
            /* hovering: the whole saucer and its pilot are on the map */
            if (step >= UFO_IN && step < UFO_IN + UFO_HOVER)
            {
                const ufo_art *b = &ufo_body[o.size];

                if (o.x - b->w / 2 < 0 || o.x - b->w / 2 + b->w > MAP_W ||
                    o.y - b->h / 2 - UFO_HEAD_H < 0 ||
                    o.y - b->h / 2 + b->h > MAP_H)
                    bad++;
            }
            if (o.size < 0 || o.size >= UFO_SIZES || o.rise < 0 ||
                o.rise > UFO_HEAD_H || o.look < 0 || o.look >= UFO_LOOKS)
                bad++;
            /* the pilot only shows on the full-size, hovering saucer */
            if (o.rise && (o.size != UFO_SIZES - 1 || step < UFO_IN ||
                           step >= UFO_IN + UFO_HOVER))
                bad++;
            /* grows on the way in, shrinks on the way out */
            if (step < UFO_IN + UFO_HOVER ? o.size < prev_size : o.size > prev_size)
                bad++;
            prev_size = o.size;
            looks |= 1 << o.look;
            if (o.rise > max_rise)
                max_rise = o.rise;
        }
        CHECK(!ufo_path_step(&p, UFO_STEPS, &o));
        ufo_path_step(&p, 0, &first);
        ufo_path_step(&p, UFO_STEPS - 1, &last);
        /* a speck off one edge, a speck off a different edge, both far
         * enough out not to show */
        CHECK(first.size == 0 && last.size == 0);
        CHECK(off_side(first.x, first.y) >= 0);
        CHECK(off_side(last.x, last.y) >= 0);
        CHECK(off_side(first.x, first.y) != off_side(last.x, last.y));
        CHECK(first.x + ufo_body[0].w < 0 || first.x - ufo_body[0].w >= MAP_W ||
              first.y + ufo_body[0].h < 0 || first.y - ufo_body[0].h >= MAP_H);
        CHECK(looks == 7 && max_rise == UFO_HEAD_H);
    }
    CHECK(bad == 0);
}

int main(void)
{
    test_json();
    test_parse();
    test_pixels();
    test_utc();
    test_trig();
    test_terminator();
    test_night_plane();
    test_night_lights();
    test_inverse_trig();
    test_great_circle();
    test_circle_bounds();
    test_extrapolate();
    test_regions();
    test_config();
    test_ufo_art();
    test_ufo_path();

    if (failures)
    {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All ISS Tracker logic tests passed\n");
    return 0;
}
