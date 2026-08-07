/**
 * ISS
 */

#include <6502.h>
#include <lynx.h>
#include <tgi.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "map.h"
#include "iss.h"
#include "lynxfnio.h"

#define JSON 1

const char iss_url[]="N:HTTP://api.open-notify.org/iss-now.json";
const char astros_url[]="N:HTTP://api.open-notify.org/astros.json";
const char longitude_query[]="/iss_position/longitude";
const char latitude_query[]="/iss_position/latitude";

struct _oc
{
  unsigned char cmd;
  unsigned char mode;
  unsigned char trans;
  char url[64];
} OC; // open command data

struct _scm
{
  unsigned char cmd;
  unsigned char mode;
} SCM; // set channel mode

struct _c
{
  unsigned char cmd;
} CMD;

struct _jsonq
{
  unsigned char cmd;
  char query[128];
} JSONQ;


char longitude[16];
char latitude[16];

signed int lat, lon;

char timestamp_str[11];
unsigned long timestamp;

unsigned char num_astros;
char astros_name[20][40];
char astros_craft[20][20];
char query[128];
char buf[64];


void json_query(char *json_q, char *buf)
{
  unsigned short len = 0;
  unsigned int r;

  r = 5;      // Retry five times
  
  JSONQ.cmd = FUJICMD_JSON_QUERY;
  memset(JSONQ.query, 0, 128);
  strcpy(JSONQ.query, json_q);

  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &JSONQ, strlen(JSONQ.query)+2);
  do {
    fnio_recv_buf(&buf[0], &len);
    r--;
  } while (len == 0 && r != 0);
  buf[len] = '\0';
}


void get_iss_pos()
{
  // Open the ISS URL
  OC.cmd = FUJICMD_OPEN; // OPEN
  OC.mode = 12; // Read/write aka HTTP GET
  OC.trans = 0; // No translation
  strncpy(OC.url, iss_url, sizeof(OC.url));
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &OC, sizeof(OC));
  fnio_recv_ack();

  sleep(2);

  // Set channel mode to JSON
  SCM.cmd  = FUJICMD_JSON; // Set channel mode
  SCM.mode = JSON; // to JSON mode
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &SCM, sizeof(SCM));
  fnio_recv_ack();
  CMD.cmd = FUJICMD_JSON_PARSE;
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &CMD, sizeof(CMD)); // Parse
  fnio_recv_ack();

  sleep(2);

  // Get longitude
  json_query((char *) &longitude_query, (char *) &longitude);

  // Get latitude
  json_query((char *) &latitude_query, (char *) &latitude);

  // Close the channel
  CMD.cmd = FUJICMD_CLOSE;
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &CMD, sizeof(CMD));
  fnio_recv_ack();

  // convert strings
  if (latitude[0] != '\0') 
    lat = atoi(latitude);
  if (longitude[0] != '\0')
    lon = atoi(longitude);    
}


void get_astros()
{
  unsigned char i;


  // Open the ISS URL
  OC.cmd = FUJICMD_OPEN; // OPEN
  OC.mode = 12; // Read/write aka HTTP GET
  OC.trans = 0; // No translation
  strncpy(OC.url, astros_url, sizeof(OC.url));
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &OC, sizeof(OC));
  fnio_recv_ack();

  sleep(2);

  // Set channel mode to JSON
  SCM.cmd  = FUJICMD_JSON; // Set channel mode
  SCM.mode = JSON; // to JSON mode
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &SCM, sizeof(SCM));
  fnio_recv_ack();
  CMD.cmd = FUJICMD_JSON_PARSE;
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &CMD, sizeof(CMD)); // Parse
  fnio_recv_ack();

  sleep(2);

  // Get number of astros
  strcpy(query, "/number");
  json_query((char*) &query, (char *) &buf);
  num_astros = atoi(buf);
  if (num_astros > 20)        // cap to 20
    num_astros = 20;

  // Get astros names
  for (i=0; i<num_astros; i++) {
    sprintf(query, "/people/%d/name", i);
    json_query((char *) &query, (char *) &astros_name[i]);
    sprintf(query, "/people/%d/craft", i);
    json_query((char *) &query, (char *) &astros_craft[i]);
  }

  // Close the channel
  CMD.cmd = FUJICMD_CLOSE;
  fnio_send_buf(FUJI_DEVICEID_NETWORK, (char *) &CMD, sizeof(CMD));
  fnio_recv_ack();
}


extern void display_astros();


void main(void)
{
  unsigned long t;


  // Setup TGI
  tgi_install(tgi_static_stddrv);
  tgi_init();
  tgi_setcolor(TGI_COLOR_WHITE);

  // Init Fujinet and Serial
  fnio_init();

  // Display msg to user
  tgi_clear();
  tgi_setcolor(TGI_COLOR_YELLOW);
  tgi_outtextxy(16, 1, "LYNX ISS TRACKER");
  
  tgi_setcolor(TGI_COLOR_WHITE);
  tgi_outtextxy(1, 17, "Getting location...");
  get_iss_pos();
  sprintf(query, "Lat:  %s", latitude);
  tgi_outtextxy(8, 25, query);
  sprintf(query, "Long: %s", longitude);
  tgi_outtextxy(8, 33, query);

  tgi_outtextxy(1, 49, "Getting astronauts...");
  get_astros();
  sprintf(buf, "People: %d", num_astros);
  tgi_outtextxy(8, 57, buf);

  tgi_setcolor(TGI_COLOR_YELLOW);
  tgi_outtextxy(28, 90, "Press Option 1");
  cgetc();

  tgi_clear();
  // Main ISS location loop
  while (1)
  {
    iss(lon, lat);
    map();

    t = clock();
    while (1) {
      if (kbhit()) {
        cgetc();
        display_astros();
        map();
        break;
      }

      if (((clock() - t) / CLOCKS_PER_SEC) > 60)
        break;
    }    

    get_iss_pos();    // update position
  }
}