/**
 * ISS Tracker
 *
 * @author Thomas Cherryhomes
 * @email thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE.md for details.
 */

#include <conio.h>
#include <stdbool.h>
#include "map.h"
#include "satellite.h"
#include "osd.h"

#include "fujinet-network.h"

#define KEY_EOL          0x0A
#define KEY_K0           0x1B

char lon_s[16], lat_s[16];
int lat, lon;
unsigned timer;
unsigned long ts;

void main(void)
{
    bool is_ok;

    clrscr();
    network_init();
    map();
    gotoxy(16, 2);
    cprintf("[ ISS Position ]");
    satelite_init();

    while (1)
    {
        timer=39000;

        gotoxy(15, 29);
        textcolor(BLUE);
        cputs("Fetching data ...");
        is_ok = satellite_fetch(&lon,&lat,lon_s,lat_s,&ts);
        if (!is_ok) 
        {
            while (timer--);
            continue;
        }
        gotoxy(15, 29);
        textcolor(GREEN);
        cputs("                 ");

        osd(lon_s,lat_s, ts);
        satellite_erase();
        satellite_draw(lon,lat);

        while (timer>0)
        {
            if (kbhit() && cgetc() == KEY_EOL)
            {
                timer=0;
                break;
            }
            timer--;
        }
    }
}
