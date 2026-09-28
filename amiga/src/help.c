/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Help window: the keys and what they do
 */

#include <string.h>
#include <proto/graphics.h>
#include "help.h"
#include "map_data.h"
#include "screen.h"

#define HELP_W 300
#define LINE_H 9
#define GAP 5
#define KEY_X 10
#define TEXT_X (KEY_X + 3 * 8)
#define STATE_X (HELP_W - 10 - 3 * 8)

typedef struct
{
    const char *key;
    const char *text;
    int toggle;                  /* index into the states, or -1 */
} help_line;

enum { T_TRAIL, T_NIGHT, T_CIRCLE, T_SOUND };

static const help_line keys[] =
{
    { "R", "Refresh the position now", -1 },
    { "W", "Who's in space right now", -1 },
    { "S", "Send out a spacewalker", -1 },
    { "U", "Call in a UFO sighting", -1 },
    { "T", "Ground track", T_TRAIL },
    { "N", "Night shading", T_NIGHT },
    { "V", "Viewable From Ground Circle", T_CIRCLE },
    { "M", "Sound effects", T_SOUND },
    { "B", "Screen saver (any key wakes)", -1 },
    { "H", "This help", -1 },
    { "Q", "Quit (or Esc)", -1 },
};

static const char *const notes[] =
{
    "Position refreshes every minute.",
    "Tool types HOMELAT and HOMELON",
    "mark home; alerts when ISS is up.",
};

#define NKEYS (int)(sizeof keys / sizeof keys[0])
#define NNOTES (int)(sizeof notes / sizeof notes[0])

static void put(struct RastPort *rp, int x, int y, int pen, const char *s)
{
    SetAPen(rp, pen);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, strlen(s));
}

void help_show(int trail, int night, int circle, int sound)
{
    static const char heading[] = "ISS Tracker keys";
    struct Window *w;
    struct RastPort *rp;
    int state[4];
    int i, y;
    int top = scr->BarHeight + 1;

    state[T_TRAIL] = trail;
    state[T_NIGHT] = night;
    state[T_CIRCLE] = circle;
    state[T_SOUND] = sound;

    w = screen_popup("Help", HELP_W, top + 6 + LINE_H + GAP + NKEYS * LINE_H +
                                     GAP + NNOTES * LINE_H + 6);
    if (!w)
        return;
    rp = w->RPort;

    y = w->BorderTop + 4 + rp->TxBaseline;
    put(rp, (HELP_W - (int)strlen(heading) * 8) / 2, y, PEN_LABEL, heading);
    y += LINE_H + GAP;

    for (i = 0; i < NKEYS; i++, y += LINE_H)
    {
        put(rp, KEY_X, y, PEN_TRAIL, keys[i].key);
        put(rp, TEXT_X, y, PEN_TEXT, keys[i].text);
        if (keys[i].toggle >= 0)
        {
            int on = state[keys[i].toggle];

            put(rp, STATE_X, y, on ? PEN_LABEL : PEN_RULE, on ? " ON" : "OFF");
        }
    }

    y += GAP;
    for (i = 0; i < NNOTES; i++, y += LINE_H)
        put(rp, KEY_X, y, PEN_RULE, notes[i]);

    screen_popup_wait(w);
}
