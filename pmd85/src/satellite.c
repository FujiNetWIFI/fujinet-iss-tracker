/**
 * ISS Tracker
 *
 * @author Thomas Cherryhomes
 * @email thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE.md for details.
 */

#define CENTER_X 4
#define CENTER_Y 4

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "satellite.h"
#include "fujinet-network.h"

#define DISPLAY 0xC000

const unsigned char satellite[24] = {
  0x00, 0x00,
  0x08, 0x00,
  0x14, 0x00,
  0x0A, 0x00,
  0x14, 0x02,
  0x20, 0x01,
  0x30, 0x01,
  0x30, 0x0A,
  0x00, 0x14,
  0x00, 0x0A,
  0x00, 0x04,
  0x00, 0x00,
};

const unsigned char mask[24] = {
  0xE1, 0xFF,
  0x00, 0xFF,
  0x00, 0xFE,
  0x00, 0xF8,
  0x00, 0xF8,
  0xC1, 0xF0,
  0xC3, 0xE0,
  0xC7, 0x00,
  0xC7, 0x00,
  0xDF, 0x00,
  0xFF, 0x00,
  0xFF, 0xE1,
};

unsigned char save[24];
unsigned char stage[24];

unsigned char x,y;
unsigned char *video_ptr;

/* AWK script to calculate the longitude and latitude values:

awk 'BEGIN{
  x=1;
  MAXX=280;
  MAXY=160;
  printf("unsigned char longitude[360] =\n{\n");
  for(i=0;i<360;i++) {
    printf("%d,",int(i*MAXX/360));
    if (x%10 == 0) { printf("\n"); }
    x++;
  }
  printf("};\n\nunsigned char latitude[180] =\n{\n");
  for(i=179;i>=0;i--) {
    printf("%d,",int(i*MAXY/180));
    if (x%10 == 0) { printf("\n"); }
    x++;
  }
  printf("};\n");
}'

*/
unsigned char longitude[360] =
{
  0,0,0,0,0,0,0,0,1,1,
  1,1,1,1,1,1,2,2,2,2,
  2,2,2,3,3,3,3,3,3,3,
  3,4,4,4,4,4,4,4,4,5,
  5,5,5,5,5,5,6,6,6,6,
  6,6,6,6,7,7,7,7,7,7,
  7,7,8,8,8,8,8,8,8,9,
  9,9,9,9,9,9,9,10,10,10,
  10,10,10,10,10,11,11,11,11,11,
  11,11,12,12,12,12,12,12,12,12,
  13,13,13,13,13,13,13,13,14,14,
  14,14,14,14,14,15,15,15,15,15,
  15,15,15,16,16,16,16,16,16,16,
  16,17,17,17,17,17,17,17,18,18,
  18,18,18,18,18,18,19,19,19,19,
  19,19,19,19,20,20,20,20,20,20,
  20,21,21,21,21,21,21,21,21,22,
  22,22,22,22,22,22,22,23,23,23,
  23,23,23,23,24,24,24,24,24,24,
  24,24,25,25,25,25,25,25,25,25,
  26,26,26,26,26,26,26,27,27,27,
  27,27,27,27,27,28,28,28,28,28,
  28,28,28,29,29,29,29,29,29,29,
  30,30,30,30,30,30,30,30,31,31,
  31,31,31,31,31,31,32,32,32,32,
  32,32,32,33,33,33,33,33,33,33,
  33,34,34,34,34,34,34,34,34,35,
  35,35,35,35,35,35,36,36,36,36,
  36,36,36,36,37,37,37,37,37,37,
  37,37,38,38,38,38,38,38,38,39,
  39,39,39,39,39,39,39,40,40,40,
  40,40,40,40,40,41,41,41,41,41,
  41,41,42,42,42,42,42,42,42,42,
  43,43,43,43,43,43,43,43,44,44,
  44,44,44,44,44,45,45,45,45,45,
  45,45,45,46,46,46,46,46,46,46,
};

unsigned char latitude[180] =
{
  165,164,163,162,161,160,159,158,157,156,
  155,154,154,153,152,151,150,149,148,147,
  146,145,144,143,142,142,141,140,139,138,
  137,136,135,134,133,132,131,130,130,129,
  128,127,126,125,124,123,122,121,120,119,
  118,118,117,116,115,114,113,112,111,110,
  109,108,107,106,106,105,104,103,102,101,
  100,99,98,97,96,95,94,94,93,92,
  91,90,89,88,87,86,85,84,83,83,
  82,81,80,79,78,77,76,75,74,73,
  72,71,71,70,69,68,67,66,65,64,
  63,62,61,60,59,59,58,57,56,55,
  54,53,52,51,50,49,48,47,47,46,
  45,44,43,42,41,40,39,38,37,36,
  35,35,34,33,32,31,30,29,28,27,
  26,25,24,23,23,22,21,20,19,18,
  17,16,15,14,13,12,11,11,10,9,
  8,7,6,5,4,3,2,1,0,0,
};

const char url[]="n:http://api.open-notify.org/iss-now.json";
const char longitude_query[]="/iss_position/longitude";
const char latitude_query[]="/iss_position/latitude";
const char timestamp_query[]="/timestamp";

uint8_t buffer[128];

bool satellite_fetch(int *lon, int *lat, char *lon_s, char *lat_s, unsigned long *ts)
{
  // unsigned short len;
  uint8_t err;
  int16_t count;

  memset(lon_s, 0, 16);
  memset(lat_s, 0, 16);
  memset(buffer, 0, 128);

  err = network_open((char *) url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
  if (err != FN_ERR_OK) return false;

  err = network_json_parse((char *) url);
  if (err != FN_ERR_OK) return false;

  count = network_json_query((char *) url, (char *) timestamp_query, (char *) buffer);
  if (count <= 0) return false;
  *ts = atol((char *) buffer);

  count = network_json_query((char *) url, (char *) longitude_query, (char *) lon_s);
  if (count <= 0) return false;
  *lon = atoi(lon_s);

  count = network_json_query((char *) url, (char *) latitude_query, (char *) lat_s);
  if (count <= 0) return false;
  *lat = atoi(lat_s);

  network_close((char *) url);

  return true; // todo come back here and add error handling.
}

void save_display(unsigned char *vp, unsigned char *dst)
{
  video_ptr = vp;
  unsigned char *p = dst;
  for(int i = 0; i<12; i++)
  {
    *p++ = *video_ptr++;
    *p++ = *video_ptr;
    video_ptr += 63;
  }
}

void load_display(unsigned char *vp, const unsigned char *src)
{
  video_ptr = vp;
  unsigned char *p = src;
  for(int i = 0; i<12; i++)
  {
    *video_ptr++ = *p++;
    *video_ptr = *p++;
    video_ptr += 63;
  }
}

void apply_mask(void)
{
  unsigned char *s = save;
  unsigned char *m = mask;
  unsigned char *l = satellite;
  unsigned char *d = stage;

  for(int i = 0; i<24; i++)
  {
    *d++ = *s++ & *m++ | *l++;
  }
}

void satellite_draw(int lon, int lat)
{
  unsigned char i,j;
  signed char b;
  x = longitude[lon+180];
  y = latitude[lat+90];
  unsigned char *vp = DISPLAY + 20*64 + 64 * y + x;
  save_display(vp, save);
  apply_mask();
  load_display(vp, stage);
}

void satellite_erase(void)
{
  unsigned char *vp = DISPLAY + 20*64 + 64 * y + x;
  load_display(vp, save);
}

void satelite_init(void)
{
    // make first save, loop in main can start with satellite_erase()
    x = longitude[180];
    y = latitude[90];
    unsigned char *vp = DISPLAY + 20*64 + 64 * y + x;
    save_display(vp, save);
}
