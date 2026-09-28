/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose "Who's in space" crew window
 */

#include <string.h>
#include <stdio.h>
#include <proto/graphics.h>
#include "fetch.h"
#include "json.h"
#include "map_data.h"
#include "screen.h"
#include "who.h"

#define WHO_W 280
#define LINE_H 9
#define NAME_COLS 22
#define CRAFT_COLS 9

static char buf[3072];

static void put(struct RastPort *rp, int x, int y, int pen, const char *s,
                int maxcols)
{
    int n = strlen(s);

    if (n > maxcols)
        n = maxcols;
    SetAPen(rp, pen);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, n);
}

unsigned char who_show(void)
{
    struct Window *w;
    struct RastPort *rp;
    const char *end, *cur = 0, *obj, *obj_end;
    char name[NAME_COLS + 8], craft[CRAFT_COLS + 8], head[40];
    unsigned short len;
    unsigned char err;
    int count = 0, rows, max_rows, y;
    int top = scr->BarHeight + 1;

    err = fetch_url(ASTROS_URL, buf, sizeof buf, &len);
    if (err != 0)
        return err;
    end = buf + len;

    while (json_next_object(buf, end, "people", &cur, &obj, &obj_end))
        count++;
    if (!count)
        return FETCH_ERR_PARSE;

    max_rows = (scr->Height - 16 - top - 30) / LINE_H;
    rows = count < max_rows ? count : max_rows;

    w = screen_popup("Who's in space?", WHO_W,
                     top + 8 + LINE_H + 6 + rows * LINE_H + 8);
    if (!w)
        return 0;
    rp = w->RPort;

    y = w->BorderTop + 6 + rp->TxBaseline;
    sprintf(head, "%d %s in space", count, count == 1 ? "person" : "people");
    put(rp, (WHO_W - (int)strlen(head) * 8) / 2, y, PEN_LABEL, head, 34);
    y += LINE_H + 6;

    cur = 0;
    while (rows-- > 0 && json_next_object(buf, end, "people", &cur, &obj, &obj_end))
    {
        if (!json_get(obj, obj_end, "name", name, sizeof name))
            strcpy(name, "?");
        if (!json_get(obj, obj_end, "craft", craft, sizeof craft))
            craft[0] = 0;
        put(rp, 10, y, PEN_TEXT, name, NAME_COLS);
        put(rp, WHO_W - 10 - CRAFT_COLS * 8, y, PEN_TRAIL, craft, CRAFT_COLS);
        y += LINE_H;
    }

    screen_popup_wait(w);
    return 0;
}
