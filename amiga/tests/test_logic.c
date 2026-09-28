/*
 * Host-side tests for the portable ISS Tracker logic (JSON, geo, time,
 * terminator).  Build and run with `make test` in amiga/.
 */

#include <stdio.h>
#include <string.h>
#include "../src/geo.h"
#include "../src/json.h"
#include "../src/night.h"

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

    if (failures)
    {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All ISS Tracker logic tests passed\n");
    return 0;
}
