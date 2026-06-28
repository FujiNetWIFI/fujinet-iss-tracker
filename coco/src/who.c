/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose "Who's in space" crew display
 */

#include <cmoc.h>
#include <coco.h>
#include "net.h"
#include "pmode3.h"
#include "who.h"

#define HTTP_GET 12
#define NO_TRANSLATION 0
#define JSON 1
#define SUCCESS 1

#define ASTROS_URL "N:HTTP://api.open-notify.org/astros.json"

#ifdef COCO3
static void who_center(int y, const char *s)
{
    puts((640 - (int)strlen(s) * 8) / 2, y, 3, s);
}
#define who_title(y,s)  puts2x_centered((y), 3, (s))
#define who_cls()       hires_cls(2)
#define HDR_Y           8
#define LIST_Y          40
#define LIST_END        174
#define FOOT_Y          184
#define FOOT_TEXT       "Hit SPACE to return to Map"
#else
static void who_center(int y, const char *s)
{
    puts4((128 - (int)strlen(s) * 4) / 2, y, 1, s);
}
#define who_title(y,s)  who_center((y),(s))
#define who_cls()       pmode3_cls(0xAA)   /* blue background */
#define HDR_Y           4
#define LIST_Y          20
#define LIST_END        168
#define FOOT_Y          182
#define FOOT_TEXT       "SPACE : MAP"
#endif

static void who_query(const char *qs, char *s, int maxlen)
{
    NetworkStatus ns;
    unsigned n;

    net_set_json_query(0, qs);
    net_status(0, &ns);
    n = ns.bytesWaiting;
    if (n >= (unsigned)maxlen)
        n = maxlen - 1;
    net_read(0, (unsigned char *)s, n);
    s[n] = 0;

    /* strip trailing whitespace/control chars the JSON values carry */
    while (n > 0 && (unsigned char)s[n - 1] <= ' ')
        s[--n] = 0;
}

void who(void)
{
    byte err;
    int n, i, y;
    char q[24];
    char name[40];
    char craft[24];
    char line[64];

    who_cls();
    who_title(88, "FETCHING CREW LIST...");

    err = net_open(0, HTTP_GET, NO_TRANSLATION, ASTROS_URL);
    if (err != SUCCESS)
    {
        who_cls();
        who_title(88, "OPEN ERROR");
        return;
    }

    net_set_channel_mode(0, JSON);

    err = net_parse_json(0);
    if (err != SUCCESS)
    {
        net_close(0);
        who_cls();
        who_title(88, "JSON PARSE ERROR");
        return;
    }

    who_query("/number", line, sizeof(line));
    n = atoi(line);

    who_cls();
    sprintf(line, "%d PEOPLE IN SPACE", n);
    who_title(HDR_Y, line);

    y = LIST_Y;
    for (i = 0; i < n; i++)
    {
        sprintf(q, "/people/%d/name", i);
        who_query(q, name, sizeof(name));
        sprintf(q, "/people/%d/craft", i);
        who_query(q, craft, sizeof(craft));

        sprintf(line, "%s (%s)", name, craft);
        who_center(y, line);

        y += 10;
        if (y > LIST_END)
            break;
    }

    net_close(0);

    who_center(FOOT_Y, FOOT_TEXT);
}
