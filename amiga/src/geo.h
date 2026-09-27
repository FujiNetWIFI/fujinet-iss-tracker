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

/* Day/night terminator for the equirectangular map at time ts.
 *
 * For each column x, night covers rows y >= edge[x] when *night_below is
 * set, otherwise rows y < edge[x]. */
void geo_terminator(unsigned long ts, unsigned char edge[MAP_W],
                    unsigned char *night_below);

#endif /* GEO_H */
