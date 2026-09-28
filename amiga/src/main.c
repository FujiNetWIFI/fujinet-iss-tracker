/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Amiga main program: startup, event loop, menus and keys
 *
 * Runs on Kickstart/Workbench 1.3 and later, from the Shell or Workbench.
 * Needs fujinet-nio.device resident (installed from the FujiNet NIO disk).
 */

#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include "config.h"
#include "fetch.h"
#include "geo.h"
#include "home.h"
#include "map_data.h"
#include "region.h"
#include "screen.h"
#include "sound.h"
#include "sprite.h"
#include "trail.h"
#include "twinkle.h"
#include "who.h"

#define TICK_MICROS   100000L    /* sprite animation rate */
#define TICKS_PER_SEC 10
#define REFRESH_SECS  60
#define RETRY_SECS    5
#define NODEVICE_SECS 15
#define FIRST_WALK_SECS 20       /* first spacewalk after the first fix */
#define WALK_EVERY_SECS 120
#define MAX_DEAD_RECKON 180      /* seconds to keep moving without a fix */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *IconBase;
extern struct WBStartup *_WBenchMsg;

static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static int timer_open;
static int timer_pending;

static config cfg;
static iss_pos pos;              /* latest fix */
static iss_pos prev;             /* the fix before it */
static iss_pos est;              /* dead-reckoned position now */
static int have_pos;
static int have_prev;
static int since_fix;            /* seconds */
static int tracking;             /* status line shows "Over: ..." */
static char status_text[40];
static int in_view;
static int show_night = 1;
static int show_trail = 1;
static int countdown;
static int walk_countdown = FIRST_WALK_SECS;
static int idle_secs;
static int warned_nodevice;

/* ---- Menus (Intuition 1.3 structures) ---------------------------------- */

enum { M_REFRESH, M_WHO, M_WALK, M_TRAIL, M_NIGHT, M_SOUND, M_SAVER,
       M_ABOUT, M_QUIT };

#define ITEM_W (LOWCHECKWIDTH + 15 * 8 + LOWCOMMWIDTH + 4)
#define ITEM_H 10

static struct IntuiText t_quit    = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Quit", 0 };
static struct IntuiText t_about   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"About...", 0 };
static struct IntuiText t_saver   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Screen saver", 0 };
static struct IntuiText t_sound   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Sound", 0 };
static struct IntuiText t_night   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Night shading", 0 };
static struct IntuiText t_trail   = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Ground track", 0 };
static struct IntuiText t_walk    = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Spacewalk!", 0 };
static struct IntuiText t_who     = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Who's in space?", 0 };
static struct IntuiText t_refresh = { 0, 1, JAM2, LOWCHECKWIDTH, 1, 0, (UBYTE *)"Refresh now", 0 };

#define ITEM(next, n, flags, text, key) \
    { next, 0, (n) * ITEM_H, ITEM_W, ITEM_H, \
      ITEMTEXT | ITEMENABLED | HIGHCOMP | COMMSEQ | (flags), \
      0, (APTR)&text, 0, key, 0, 0 }
#define TOGGLE (CHECKIT | MENUTOGGLE | CHECKED)

static struct MenuItem i_quit = ITEM(0, 8, 0, t_quit, 'Q');
static struct MenuItem i_about = ITEM(&i_quit, 7, 0, t_about, '?');
static struct MenuItem i_saver = ITEM(&i_about, 6, 0, t_saver, 'B');
static struct MenuItem i_sound = ITEM(&i_saver, 5, TOGGLE, t_sound, 'M');
static struct MenuItem i_night = ITEM(&i_sound, 4, TOGGLE, t_night, 'N');
static struct MenuItem i_trail = ITEM(&i_night, 3, TOGGLE, t_trail, 'T');
static struct MenuItem i_walk = ITEM(&i_trail, 2, 0, t_walk, 'S');
static struct MenuItem i_who = ITEM(&i_walk, 1, 0, t_who, 'W');
static struct MenuItem i_refresh = ITEM(&i_who, 0, 0, t_refresh, 'R');

static struct Menu menu =
    { 0, 0, 0, 72, 0, MENUENABLED, (APTR)"Project", &i_refresh, 0, 0, 0, 0 };

/* ---- Settings ---------------------------------------------------------- */

static void read_settings(int argc, char **argv)
{
    int i;

    config_defaults(&cfg);
    if (argc > 0)
    {
        for (i = 1; i < argc; i++)
            config_arg(&cfg, argv[i]);
    }
    else if (_WBenchMsg && _WBenchMsg->sm_NumArgs > 0 &&
             (IconBase = OpenLibrary((STRPTR)"icon.library", 33)) != 0)
    {
        struct WBArg *arg = &_WBenchMsg->sm_ArgList[0];
        BPTR old = CurrentDir(arg->wa_Lock);
        struct DiskObject *dob = GetDiskObject(arg->wa_Name);

        if (dob)
        {
            char **tt = (char **)dob->do_ToolTypes;

            for (; tt && *tt; tt++)
                config_arg(&cfg, *tt);
            FreeDiskObject(dob);
        }
        CurrentDir(old);
        CloseLibrary(IconBase);
        IconBase = 0;
    }
    if (config_has_home(&cfg))
        home_set(cfg.home_lat, cfg.home_lon);
}

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

/* ---- Live position ----------------------------------------------------- */

static void status(const char *text, int pen)
{
    tracking = 0;
    screen_status(text, pen);
}

/* Show the dead-reckoned position: sprite, panel, footprint, home and the
 * country or ocean below. Called every second and after every fix. */
static void show_live(void)
{
    char text[40];
    int now_in;

    if (!have_pos)
        return;

    est = pos;
    if (have_prev && since_fix <= MAX_DEAD_RECKON)
        geo_extrapolate(prev.lat_h, prev.lon_h, prev.ts,
                        pos.lat_h, pos.lon_h, pos.ts,
                        since_fix, &est.lat_h, &est.lon_h);
    est.ts = pos.ts + since_fix;

    sprite_place(geo_lon_to_x(est.lon_h), geo_lat_to_y(est.lat_h),
                 screen_map_y());
    screen_draw_position(&est);
    footprint_show(screen_map_rp(), screen_map_y(), est.lat_h, est.lon_h);

    now_in = home_in_view(est.lat_h, est.lon_h);
    if (home_known())
    {
        sprintf(text, "HOME %5ld km%s", home_distance_km(est.lat_h, est.lon_h),
                now_in ? "   ISS IN VIEW!" : "");
        screen_home(text, now_in ? PEN_TRAIL : PEN_TEXT);
        if (now_in && !in_view)
            sound_alert();
    }
    in_view = now_in;

    if (tracking)
    {
        sprintf(text, "%s%.22s", now_in ? "In view: " : "Over: ",
                region_name(est.lat_h, est.lon_h));
        if (strcmp(text, status_text))
        {
            strcpy(status_text, text);
            screen_status(text, now_in ? PEN_TRAIL : PEN_LABEL);
        }
    }
}

static void start_tracking(void)
{
    tracking = 1;
    status_text[0] = 0;
    show_live();
}

/* ---- Actions ----------------------------------------------------------- */

static void redraw_map(void)
{
    screen_draw_map(have_pos ? &pos : 0, show_night, show_trail);
    show_live();
}

static void show_error(unsigned char err)
{
    status(fetch_error(err), PEN_ERROR);
    if (err == FETCH_ERR_NODEVICE && !warned_nodevice && !screen_saver_window())
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
    iss_pos fix;

    screen_countdown(-1);
    status("Contacting FujiNet...", PEN_TEXT);

    err = fetch_iss(&fix);
    if (err == 0)
    {
        if (have_pos && fix.ts > pos.ts)
        {
            prev = pos;
            have_prev = 1;
        }
        pos = fix;
        have_pos = 1;
        since_fix = 0;
        trail_add(&pos);
        tracking = 1;
        status_text[0] = 0;
        redraw_map();
        sound_ping();
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
    static struct IntuiText l6 = { PEN_SHADOW, 1, JAM1, 12, 56, 0,
        (UBYTE *)"Places: Natural Earth", 0 };
    static struct IntuiText l5 = { PEN_SHADOW, 1, JAM1, 12, 46, 0,
        (UBYTE *)"Lights: NASA Black Marble", &l6 };
    static struct IntuiText l4 = { PEN_SHADOW, 1, JAM1, 12, 36, 0,
        (UBYTE *)"Map: NASA Blue Marble", &l5 };
    static struct IntuiText l3 = { PEN_SHADOW, 1, JAM1, 12, 26, 0,
        (UBYTE *)"Data: open-notify.org", &l4 };
    static struct IntuiText l2 = { PEN_SHADOW, 1, JAM1, 12, 16, 0,
        (UBYTE *)"A FujiNet NIO client", &l3 };
    static struct IntuiText l1 = { PEN_SHADOW, 1, JAM1, 12, 6, 0,
        (UBYTE *)"ISS Tracker for the Amiga", &l2 };
    static struct IntuiText ok = { PEN_SHADOW, 1, JAM1, 6, 3, 0,
        (UBYTE *)"OK", 0 };

    AutoRequest(win, &l1, 0, &ok, 0, 0, 240, 106);
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

    status("Fetching crew list...", PEN_TEXT);
    sprite_suspend(1);
    footprint_hide();
    err = who_show();
    sprite_suspend(0);
    if (err)
        show_error(err);
    else if (have_pos)
        start_tracking();
    else
        status("", PEN_LABEL);
}

static void spacewalk(void)
{
    if (sprite_spacewalk())
        sound_quindar(1);
}

static void saver(int on)
{
    screen_saver(on);
    idle_secs = 0;
    show_live();
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
    case M_WALK:
        spacewalk();
        break;
    case M_TRAIL:
        show_trail = (i_trail.Flags & CHECKED) != 0;
        redraw_map();
        break;
    case M_NIGHT:
        show_night = (i_night.Flags & CHECKED) != 0;
        redraw_map();
        break;
    case M_SOUND:
        sound_enable((i_sound.Flags & CHECKED) != 0);
        break;
    case M_SAVER:
        saver(1);
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
    case 's': case 'S':
        return action(M_WALK);
    case 't': case 'T':
        set_checked(&i_trail, !(i_trail.Flags & CHECKED));
        return action(M_TRAIL);
    case 'n': case 'N':
        set_checked(&i_night, !(i_night.Flags & CHECKED));
        return action(M_NIGHT);
    case 'm': case 'M':
        set_checked(&i_sound, !(i_sound.Flags & CHECKED));
        return action(M_SOUND);
    case 'b': case 'B':
        return action(M_SAVER);
    case 'q': case 'Q': case 27:
        return 0;
    }
    return 1;
}

/* Ten times a second */
static void tick(void)
{
    if (sprite_tick())
        sound_quindar(0);
    if (have_pos && show_night)
        twinkle_tick(screen_map_rp(), screen_map_y(), screen_map_bitmap(),
                     geo_lon_to_x(est.lon_h), geo_lat_to_y(est.lat_h));
}

/* Once a second */
static void second(void)
{
    if (have_pos)
    {
        since_fix++;
        show_live();
    }
    if (have_pos && --walk_countdown <= 0)
    {
        spacewalk();
        walk_countdown = WALK_EVERY_SECS;
    }
    if (!screen_saver_window() && cfg.saver_minutes > 0 &&
        ++idle_secs >= cfg.saver_minutes * 60)
        saver(1);
    if (--countdown <= 0)
        update();
    else
        screen_countdown(countdown);
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
        struct Window *sv = screen_saver_window();
        ULONG sv_sig = sv ? 1UL << sv->UserPort->mp_SigBit : 0;
        ULONG sigs = Wait(win_sig | tmr_sig | sv_sig);
        struct IntuiMessage *msg;

        if (sigs & tmr_sig)
        {
            WaitIO((struct IORequest *)timer_req);
            timer_pending = 0;
            tick();
            if (++ticks >= TICKS_PER_SEC)
            {
                ticks = 0;
                second();
            }
            timer_start();
        }

        if (sv && (sigs & sv_sig))
        {
            int wake = 0;

            while ((msg = (struct IntuiMessage *)GetMsg(sv->UserPort)) != 0)
            {
                if ((msg->Class == IDCMP_RAWKEY && !(msg->Code & IECODE_UP_PREFIX)) ||
                    (msg->Class == IDCMP_MOUSEBUTTONS && !(msg->Code & IECODE_UP_PREFIX)))
                    wake = 1;
                ReplyMsg((struct Message *)msg);
            }
            if (wake)
                saver(0);
        }

        while (running && (msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != 0)
        {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;

            ReplyMsg((struct Message *)msg);
            idle_secs = 0;
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

int main(int argc, char **argv)
{
    const char *why = 0;
    int rc = 20;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 33);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 33);
    if (!IntuitionBase || !GfxBase)
        goto out;

    read_settings(argc, argv);
    if (!cfg.sound)
        i_sound.Flags &= ~CHECKED;

    if (!timer_init())
        goto out;
    if (!screen_open(&menu, &why))
        goto out;
    sprite_open(scr);
    twinkle_init();
    sound_open();
    sound_enable(cfg.sound);

    run();
    rc = 0;

out:
    fetch_shutdown();
    sound_close();
    twinkle_free();
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
