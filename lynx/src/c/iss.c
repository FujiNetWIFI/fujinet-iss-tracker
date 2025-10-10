
#include <lynx.h>
#include <conio.h>
#include <tgi.h>
#include <stdio.h>
#include <string.h>


extern char img_iss[];

SCB_REHV_PAL issSprite = {
  BPP_4 | TYPE_BACKGROUND, REHV | PACKED, 0x0,
  0, (unsigned char *) &img_iss[0],
  0, 0,
  256, 256,
  {0x03,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF}
};


void iss(signed int lon, signed int lat)
{
  signed int x, y;

  // normalize lon
  if (lon < -180) lon += 360;
  if (lon > 180) lon -= 360;
  if (lat < -90) lat = -90;
  if (lat > 90) lat = 90;

  // ---- Integer math, but force unsigned to avoid negatives ----
  // Add 180 so lon range [−180,+180] → [0,360]
  // Do multiplication before division for precision.
  x = (((unsigned int)(lon + 180) * 160u) + 180u) / 360u;  // +180 for rounding
  y = (((unsigned int)(90 - lat) * 102u)  + 90u)  / 180u;

  issSprite.hpos = (unsigned int) x;
  issSprite.vpos = (unsigned int) y;
}
