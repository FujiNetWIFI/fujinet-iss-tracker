/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose On Screen Display Functions
 */

#include <cmoc.h>
#include "globals.h"
#include "pmode3.h"
#include "ftime.h"
#include "map.h"

char lon_output[20];
char lat_output[20];
char ts_output[20];

/**
 * @brief Update output strings
 */
void _update_output(void)
{
    Timestamp tm;
    
    timestamp(ts,&tm);
    sprintf(lon_output,"%5s %9s","LON:",lon_s);
    sprintf(lat_output,"%5s %9s","LAT:",lat_s);
    sprintf(ts_output,"  %s %02u %02u:%02u",time_month(tm.month),tm.day,tm.hour,tm.min);
}

/**
 * @brief output on screen display
 */
void osd(void)
{
    _update_output();
#ifdef COCO3
    {
        char line[40];
        const char *title = "ISS POSITION";
        const char *hint = "Hit SPACE to show who's in space";
        clear_bottom();
        puts((640-(int)strlen(title)*8)/2,144,3,title);
        sprintf(line,"LON %s  LAT %s",lon_s,lat_s);
        puts2x_centered(152,3,line);
        puts2x_centered(168,3,ts_output+2);
        puts((640-(int)strlen(hint)*8)/2,184,3,hint);
    }
#else
    map_clear_osd();
    puts4((128-17*4)/2,152,1,"SPACE : CREW LIST");
    puts(0,160,1,"  ISS POSITION  ");
    puts(0,168,1,lon_output);
    puts(0,176,1,lat_output);
    puts(0,184,1,ts_output);
#endif
}
