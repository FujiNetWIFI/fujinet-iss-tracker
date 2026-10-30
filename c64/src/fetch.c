/**
 * #FujiNet ISS tracker for C64
 *
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3
 *
 * @brief Satellite position fetch functions
 */

#include <string.h>
#include <stdlib.h>
#include <fujinet-network.h> // also keeps underscore as itself in string literals

const char url[]="n:http://api.open-notify.org/iss-now.json";

const char lon_query[] = "/iss_position/longitude";
const char lat_query[] = "/iss_position/latitude";
const char ts_query[] = "/timestamp";

static char tmp[80];

/**
 * @brief query the parsed JSON into tmp, which network_json_query() fills
 * with whatever the value holds
 */
static void query(const char *q)
{
  memset(tmp,0,sizeof(tmp));
  network_json_query(url,q,tmp);
}

/**
 * @brief fetch satellite telemetry, and return as ints and strings
 * @param lon pointer to integer longitude
 * @param lat pointer to integer latitude
 * @param lon_s pointer to longitude string (for showing floating point val)
 * @param lat_s pointer to latitude string (for showing floating point val)
 * @param ts pointer to timestamp
 */
void fetch(int *lon, int *lat, char *lon_s, char *lat_s, unsigned long *ts)
{
  memset(lon_s,0,16);
  memset(lat_s,0,16);

  if (network_open(url,OPEN_MODE_HTTP_GET,OPEN_TRANS_NONE) != FN_ERR_OK)
    return;

  if (network_json_parse(url) == FN_ERR_OK)
    {
      query(ts_query);
      *ts = atol(tmp);

      query(lon_query);
      strncpy(lon_s,tmp,15);
      *lon = atoi(tmp);

      query(lat_query);
      strncpy(lat_s,tmp,15);
      *lat = atoi(tmp);
    }

  network_close(url);
}
