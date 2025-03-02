/**
 * ISS Tracker
 *
 * @author Thomas Cherryhomes
 * @email thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE.md for details.
 */

#include <conio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#include "time_impl.h"

const char *monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void osd(char *lon, char *lat,unsigned long ts) {
    uint8_t lon_len;
    uint8_t lat_len;
    lon_len = strlen(lon);
    lat_len = strlen(lat);
    if (lon[lon_len - 1] == 0x0D) lon[lon_len - 1] = '\0';
    if (lat[lat_len - 1] == 0x0D) lat[lat_len - 1] = '\0';

    struct tm tm;
    secs_to_tm(ts,  &tm);

    gotoxy(9, 25);
    cprintf("LAT: %-10s",lat);
    gotoxy(24, 25);
    cprintf("LON: %-10s",lon);
    gotoxy(7, 27);
    cprintf("Time (GMT): %02d-%03s-%04d, %02d:%02d:%02d",
        tm.tm_mday, monthNames[tm.tm_mon], tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}