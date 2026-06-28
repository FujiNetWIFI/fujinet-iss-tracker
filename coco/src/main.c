/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Main Program
 */

#include <cmoc.h>
#include <coco.h>
#include "map.h"
#include "satellite.h"
#include "osd.h"
#include "fetch.h"
#include "pmode3.h"
#include "who.h"

int main(void)
{
    unsigned char view = 0;   /* 0 = map, 1 = who's in space */

    while(1)
    {
        if (view == 0)
        {
            map();
            fetch();
            satellite();
            osd();
        }
        else
        {
            who();
        }

        view = input_delay(view);
    }

    return 0;
}
