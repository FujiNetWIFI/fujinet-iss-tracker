/**
 * ISS Tracker
 *
 * @author Thomas Cherryhomes
 * @email thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE.md for details.
 */

#include <conio.h>
#include <tgi.h>
#include <apple2.h>
#include <stdbool.h>
#include "map.h"
#include "satellite.h"
#include "osd.h"

#include "fujinet-network.h"

extern uint8_t sp_get_network_id();

unsigned char net;
char lon_s[16], lat_s[16];
int lat, lon;
long timer;
unsigned long ts;

void main(void)
{
  bool is_ok;
  clrscr();
  network_init();
  net = sp_get_network_id();
  map();
  tgi_install(tgi_static_stddrv);
  tgi_init();
  tgi_apple2_mix(true);
  if (get_ostype() == APPLE_IIIEM)
    allow_lowercase(true);

  while (1)
    {
      timer=524088;
      clrscr();
      is_ok = satellite_fetch(&lon,&lat,lon_s,lat_s,&ts);
      if (!is_ok) {
        continue;
      }

      osd(lon_s,lat_s,ts);
      satellite_draw(lon,lat);

      while (timer>0)
        {
          if (kbhit())
            switch(cgetc())
              {
              case CH_ESC:
                return;
              case CH_ENTER:
                timer=0;
                break;
              default:
                break;
              }

          timer--;
        }

      satellite_erase();
    }
}
