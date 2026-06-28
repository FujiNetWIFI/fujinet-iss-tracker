/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Fetch current position from web
 */

#include <cmoc.h>
#include <coco.h>
#include "net.h"
#include "pmode3.h"
#include "globals.h"

#define HTTP_GET 12
#define NO_TRANSLATION 0

#define JSON 1

#define SUCCESS 1

#define API_ENDPOINT "N:HTTP://api.wheretheiss.at/v1/satellites/25544"

#define QUERY_LONGITUDE "/longitude"
#define QUERY_LATITUDE "/latitude"
#define QUERY_TIMESTAMP "/timestamp"

/**
 * @brief copy a numeric string keeping at most 2 fractional digits
 */
static void copy_trunc(const char *src, char *dst)
{
    int n = 0, dec = -1;

    while (src[n] && n < 14)
    {
        dst[n] = src[n];
        if (src[n] == '.')
            dec = n;
        n++;
        if (dec >= 0 && n == dec + 3)
            break;
    }
    dst[n] = 0;
}

/**
 * @brief fetch individual json element and populate vars.
 */
void fetch_json(const char *qs, char *s, int *i)
{
    NetworkStatus ns;
    char buf[32];
    unsigned n;

    // Set query
    net_set_json_query(0,qs);

    // Get # of bytes waiting
    net_status(0,&ns);

    // read into buffer
    n = ns.bytesWaiting;
    if (n > 31) n = 31;
    net_read(0,(unsigned char *)buf,n);
    buf[n] = 0;

    // Convert string to integer
    *i = atoi(buf);
    copy_trunc(buf,s);
}

/**
 * @brief fetch individual json element and populate vars.
 */
void fetch_json_timestamp(const char *qs, unsigned long *l)
{
    NetworkStatus ns;
    char tsi[16];
    
    // Set query
    net_set_json_query(0,qs);

    // Get # of bytes waiting
    net_status(0,&ns);

    // read into buffer
    net_read(0,(unsigned char *)tsi,ns.bytesWaiting);

    // Convert string to integer
    *l=atol(tsi);
}

/**
 * @brief Fetch JSON data from web endpoint, and populate vars
 */
void fetch(void)
{
    byte err;

#ifdef COCO3
    clear_bottom();
    puts2x_centered(168,3,"FETCHING ISS LOCATION...");
#else
    puts(0,176,1,"FETCHING ISS...");
#endif

    err = net_open(0,HTTP_GET,NO_TRANSLATION,API_ENDPOINT);

    if (err != SUCCESS)
    {
        puts(0,160,1,"OPEN ERROR.");
        return;
    }

    // Set channel mode to JSON
    net_set_channel_mode(0,JSON);

    err = net_parse_json(0);

    if (err != SUCCESS)
    {
        puts(0,160,1,"JSON PARSE ERROR.");
        return;
    }

    fetch_json(QUERY_LONGITUDE,&lon_s,&lon_i);
    fetch_json(QUERY_LATITUDE,&lat_s,&lat_i);
    fetch_json_timestamp(QUERY_TIMESTAMP,&ts);
    
    net_close(0);
    
}
