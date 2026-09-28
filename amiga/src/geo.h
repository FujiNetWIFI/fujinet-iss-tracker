/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Geography and time maths (portable, integer only)
 *
 * Angles are carried as signed hundredths of a degree (1234 == 12.34 deg)
 * and trigonometry is Q14 fixed point (16384 == 1.0), so nothing here needs
 * the soft-float library on a 68000.
 */

#ifndef GEO_H
#define GEO_H

#define MAP_W 320
#define MAP_H 160

#define GEO_ONE 16384L

typedef struct
{
    int year;
    int month;   /* 1..12 */
    int day;     /* 1..31 */
    int hour;
    int minute;
    int second;
    int yday;    /* 0..365 */
} geo_tm;

/* Parse a decimal string such as "-12.3456" into rounded hundredths.
 * Returns 1 on success, 0 if no digits were found. */
int geo_parse_hundredths(const char *s, long *out);

/* Map pixel for a longitude/latitude, clamped to the map. */
int geo_lon_to_x(long lon_h);
int geo_lat_to_y(long lat_h);

/* Convert a Unix timestamp to UTC calendar time. */
void geo_utc(unsigned long ts, geo_tm *tm);

/* Q14 sine/cosine of an angle in hundredths of a degree. */
long geo_sin(long ang_h);
long geo_cos(long ang_h);

/* Inverse trig. Inputs are Q14 (asin/acos) or any common scale (atan2);
 * results are hundredths of a degree. */
long geo_asin(long s);
long geo_acos(long c);
long geo_atan2(long y, long x);

/* Normalise a longitude into [-18000, 18000). */
long geo_wrap_lon(long lon_h);

/* Great-circle angle between two points, hundredths of a degree. */
long geo_angle_between(long lat1, long lon1, long lat2, long lon2);

/* Kilometres for a great-circle angle in hundredths of a degree. */
long geo_angle_to_km(long ang_h);

/* The point ang_h away from (lat, lon) along initial bearing brg_h
 * (0 = north, clockwise). */
void geo_destination(long lat, long lon, long brg_h, long ang_h,
                     long *lat2, long *lon2);

/* Map pixels of the circle ang_h around (lat, lon), as seen on the map:
 * points at `points` evenly spaced bearings, with repeats dropped. Writes
 * at most `points` entries to xs/ys and returns how many. */
int geo_circle_pixels(long lat, long lon, long ang_h, int points,
                      short *xs, short *ys);

/* Dead-reckon a position elapsed seconds after the fix at (lat1, lon1, t1),
 * from the motion since an earlier fix at (lat0, lon0, t0). Returns 0 (and
 * copies the later fix) when the two fixes are unusable. */
int geo_extrapolate(long lat0, long lon0, unsigned long t0,
                    long lat1, long lon1, unsigned long t1,
                    long elapsed, long *lat, long *lon);

/* Day/night terminator for the equirectangular map at time ts.
 *
 * For each column x, night covers rows y >= edge[x] when *night_below is
 * set, otherwise rows y < edge[x]. */
void geo_terminator(unsigned long ts, unsigned char edge[MAP_W],
                    unsigned char *night_below);

#endif /* GEO_H */
