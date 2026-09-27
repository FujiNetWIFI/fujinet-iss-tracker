/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Amiga main program: startup, event loop, menus and keys
 *
 * Runs on Kickstart/Workbench 1.3 and later, from the Shell or Workbench.
 * Needs fujinet-nio.device resident (installed from the FujiNet NIO disk).
 */

#include <exec/types.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "fetch.h"
#include "geo.h"
#include "map_data.h"
#include "screen.h"
#include "sprite.h"
#include "trail.h"
#include "who.h"

#define TICK_MICROS   200000L    /* sprite colour cycle rate */
#define TICKS_PER_SEC 5
#define REFRESH_SECS  60
#define RETRY_SECS    5
#define NODEVICE_SECS 15

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static int timer_open;
static int timer_pending;

static iss_pos pos;
static int have_pos;
static int show_night = 1;
static int show_trail = 1;
static int countdown;
static int warned_nodevice;

/* ---- Menus (Intuition 1.3 structures) ---------------------------------- */

enum { M_REFRESH, M_WHO, M_TRAIL, M_NIGHT, M_ABOUT, M_QUIT };

#define ITEM_W (LOWCHECKWIDTH + 15 * 8 + LOWCOMMWIDTH + 4)
#define ITEM_H 10

static struct IntuiText t_quit    = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Quit", 0 };
static struct IntuiText t_about   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"About...", 0 };
static struct IntuiText t_night   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Night shading", 0 };
static struct IntuiText t_trail   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Ground track", 0 };
static struct IntuiText t_who     = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Who's in space?", 0 };
static struct IntuiText t_refresh = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Refresh now", 0 };

#define ITEM(next, n, flags, text, key) \
    { next, 0, (n) * ITEM_H, ITEM_W, ITEM_H, \
      ITEMTEXT | ITEMENABLED | HIGHCOMP | COMMSEQ | (flags), \
      0, (APTR)&text, 0, key, 0, 0 }

static struct MenuItem i_quit = ITEM(0, 5, 0, t_quit, 'Q');
static struct MenuItem i_about = ITEM(&i_quit, 4, 0, t_about, '?');
static struct MenuItem i_night = ITEM(&i_about, 3, CHECKIT | MENUTOGGLE | CHECKED, t_night, 'N');
static struct MenuItem i_trail = ITEM(&i_night, 2, CHECKIT | MENUTOGGLE | CHECKED, t_trail, 'T');
static struct MenuItem i_who = ITEM(&i_trail, 1, 0, t_who, 'W');
static struct MenuItem i_refresh = ITEM(&i_who, 0, 0, t_refresh, 'R');

static struct Menu menu =
    { 0, 0, 0, 72, 0, MENUENABLED, (APTR)"Project", &i_refresh, 0, 0, 0, 0 };

/* ---- Timer ------------------------------------------------------------- */

static int timer_init(void)
{
    timer_port = CreatePort(0, 0);
    if (!timer_port)
        return 0;
    timer_req = (struct timerequest *)CreateExtIO(timer_port, sizeof *timer_req);
    if (!timer_req)
        return 0;
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)timer_req, 0))
        return 0;
    timer_open = 1;
    return 1;
}

static void timer_start(void)
{
    timer_req->tr_node.io_Command = TR_ADDREQUEST;
    timer_req->tr_time.tv_secs = 0;
    timer_req->tr_time.tv_micro = TICK_MICROS;
    SendIO((struct IORequest *)timer_req);
    timer_pending = 1;
}

static void timer_cleanup(void)
{
    if (timer_pending)
    {
        AbortIO((struct IORequest *)timer_req);
        WaitIO((struct IORequest *)timer_req);
        timer_pending = 0;
    }
    if (timer_open)
        CloseDevice((struct IORequest *)timer_req);
    if (timer_req)
        DeleteExtIO((struct IORequest *)timer_req);
    if (timer_port)
        DeletePort(timer_port);
}

/* ---- Actions ----------------------------------------------------------- */

static void redraw_map(void)
{
    screen_draw_map(have_pos ? &pos : 0, show_night, show_trail);
}

static void show_error(unsigned char err)
{
    screen_status(fetch_error(err), PEN_ERROR);
    if (err == FETCH_ERR_NODEVICE && !warned_nodevice)
    {
        static struct IntuiText l3 = { PEN_SHADOW, 1, JAM1, 8, 26, 0,
            (UBYTE *)"See ReadMe on the ISS disk.", 0 };
        static struct IntuiText l2 = { PEN_SHADOW, 1, JAM1, 8, 16, 0,
            (UBYTE *)"Load the FujiNet NIO driver.", &l3 };
        static struct IntuiText l1 = { PEN_SHADOW, 1, JAM1, 8, 6, 0,
            (UBYTE *)"fujinet-nio.device missing.", &l2 };
        static struct IntuiText ok = { PEN_SHADOW, 1, JAM1, 6, 3, 0,
            (UBYTE *)"OK", 0 };

        warned_nodevice = 1;
        AutoRequest(win, &l1, 0, &ok, 0, 0, 300, 76);
    }
}

static void update(void)
{
    unsigned char err;

    screen_countdown(-1);
    screen_status("Contacting FujiNet...", PEN_TEXT);

    err = fetch_iss(&pos);
    if (err == 0)
    {
        have_pos = 1;
        trail_add(&pos);
        redraw_map();
        sprite_place(geo_lon_to_x(pos.lon_h), geo_lat_to_y(pos.lat_h), map_top);
        screen_draw_position(&pos);
        screen_status("Tracking", PEN_LABEL);
        countdown = REFRESH_SECS;
    }
    else
    {
        show_error(err);
        countdown = err == FETCH_ERR_NODEVICE ? NODEVICE_SECS : RETRY_SECS;
    }
    screen_countdown(countdown);
}

static void about(void)
{
    static struct IntuiText l4 = { PEN_SHADOW, 1, JAM1, 12, 36, 0,
        (UBYTE *)"Map: NASA Blue Marble", 0 };
    static struct IntuiText l3 = { PEN_SHADOW, 1, JAM1, 12, 26, 0,
        (UBYTE *)"Data: open-notify.org", &l4 };
    static struct IntuiText l2 = { PEN_SHADOW, 1, JAM1, 12, 16, 0,
        (UBYTE *)"A FujiNet NIO client", &l3 };
    static struct IntuiText l1 = { PEN_SHADOW, 1, JAM1, 12, 6, 0,
        (UBYTE *)"ISS Tracker for the Amiga", &l2 };
    static struct IntuiText ok = { PEN_SHADOW, 1, JAM1, 6, 3, 0,
        (UBYTE *)"OK", 0 };

    AutoRequest(win, &l1, 0, &ok, 0, 0, 240, 86);
}

static void set_checked(struct MenuItem *item, int on)
{
    ClearMenuStrip(win);
    if (on)
        item->Flags |= CHECKED;
    else
        item->Flags &= ~CHECKED;
    SetMenuStrip(win, &menu);
}

static void crew(void)
{
    unsigned char err;

    screen_status("Fetching crew list...", PEN_TEXT);
    sprite_suspend(1);
    err = who_show();
    sprite_suspend(0);
    if (err)
        show_error(err);
    else
        screen_status(have_pos ? "Tracking" : "", PEN_LABEL);
}

/* Returns 0 to quit */
static int action(int what)
{
    switch (what)
    {
    case M_REFRESH:
        update();
        break;
    case M_WHO:
        crew();
        break;
    case M_TRAIL:
        show_trail = (i_trail.Flags & CHECKED) != 0;
        redraw_map();
        break;
    case M_NIGHT:
        show_night = (i_night.Flags & CHECKED) != 0;
        redraw_map();
        break;
    case M_ABOUT:
        about();
        break;
    case M_QUIT:
        return 0;
    }
    return 1;
}

static int key(UWORD code)
{
    switch (code)
    {
    case 'r': case 'R':
        return action(M_REFRESH);
    case 'w': case 'W':
        return action(M_WHO);
    case 't': case 'T':
        set_checked(&i_trail, !(i_trail.Flags & CHECKED));
        return action(M_TRAIL);
    case 'n': case 'N':
        set_checked(&i_night, !(i_night.Flags & CHECKED));
        return action(M_NIGHT);
    case 'q': case 'Q': case 27:
        return 0;
    }
    return 1;
}

static void run(void)
{
    ULONG win_sig = 1UL << win->UserPort->mp_SigBit;
    ULONG tmr_sig = 1UL << timer_port->mp_SigBit;
    int ticks = 0;
    int running = 1;

    redraw_map();
    update();
    timer_start();

    while (running)
    {
        ULONG sigs = Wait(win_sig | tmr_sig);
        struct IntuiMessage *msg;

        if (sigs & tmr_sig)
        {
            WaitIO((struct IORequest *)timer_req);
            timer_pending = 0;
            sprite_tick();
            if (++ticks >= TICKS_PER_SEC)
            {
                ticks = 0;
                if (--countdown <= 0)
                    update();
                else
                    screen_countdown(countdown);
            }
            timer_start();
        }

        while (running && (msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != 0)
        {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;

            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW)
                running = 0;
            else if (cls == IDCMP_VANILLAKEY)
                running = key(code);
            else if (cls == IDCMP_MENUPICK)
            {
                while (running && code != MENUNULL)
                {
                    struct MenuItem *item = ItemAddress(&menu, code);

                    running = action(ITEMNUM(code));
                    code = item->NextSelect;
                }
            }
        }
    }
}

int main(void)
{
    const char *why = 0;
    int rc = 20;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 33);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 33);
    if (!IntuitionBase || !GfxBase)
        goto out;

    if (!timer_init())
        goto out;
    if (!screen_open(&menu, &why))
        goto out;
    sprite_open(scr);

    run();
    rc = 0;

out:
    fetch_shutdown();
    sprite_close();
    screen_close();
    timer_cleanup();
    if (why && IntuitionBase)
    {
        struct IntuiText body = { 0, 1, JAM1, 12, 8, 0, (UBYTE *)why, 0 };
        struct IntuiText ok = { 0, 1, JAM1, 6, 3, 0, (UBYTE *)"OK", 0 };

        AutoRequest(0, &body, 0, &ok, 0, 0, 320, 60);
    }
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
