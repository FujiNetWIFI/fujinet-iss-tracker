/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Custom screen, main window, map compositor and status panel
 *
 * Kickstart 1.3 structures only (no tag lists). The map is composed off
 * screen and blitted through the window's layer, so windows and menus on
 * top are never overdrawn.
 */

#include <string.h>
#include <stdio.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "geo.h"
#include "home.h"
#include "map_data.h"
#include "night.h"
#include "screen.h"
#include "trail.h"
#include "twinkle.h"

#define DEPTH 5
#define COLS  (MAP_W / 8)        /* characters per panel line */
#define LINE_H 9
#define STATUS_COLS 28           /* status text, left of the countdown */
#define HOME_COLS 38

extern struct GfxBase *GfxBase;

struct Screen *scr;
struct Window *win;
static int map_top;              /* map row 0 in the main window */

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
static struct BitMap off;        /* MAP_W x MAP_H, 5 planes */
static int off_ok;
static int pal;                  /* PAL (256 lines) or NTSC (200) */
static int panel_top;

static struct Window *saver;     /* screen saver window, when showing */
static UWORD *no_pointer;        /* chip RAM: an empty pointer sprite */
#define NO_POINTER_WORDS 6

/* Panel text rows (baseline y) for PAL and NTSC layouts */
static int row_title, row_pos, row_time, row_home, row_status, row_help;

static void text_at(int x, int y, int pen, const char *s)
{
    struct RastPort *rp = win->RPort;

    SetAPen(rp, pen);
    SetBPen(rp, PEN_OCEAN);
    SetDrMd(rp, JAM2);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, strlen(s));
}

/* Draw s padded with spaces to width characters so stale text is erased */
static void field_at(int col, int y, int width, int pen, const char *s)
{
    char buf[COLS + 1];
    int n = strlen(s);

    if (width > COLS)
        width = COLS;
    if (n > width)
        n = width;
    memcpy(buf, s, n);
    memset(buf + n, ' ', width - n);
    buf[width] = 0;
    text_at(col * 8, y, pen, buf);
}

static void layout_panel(void)
{
    int base = panel_top + 2 + win->RPort->TxBaseline;

    if (pal)
    {
        row_title = base + 2;
        row_pos = base + 2 + LINE_H * 2;
        row_time = row_pos + LINE_H;
        row_home = row_time + LINE_H;
        row_status = row_home + LINE_H;
        row_help = row_status + LINE_H;
    }
    else
    {
        row_title = row_home = row_help = -1;
        row_pos = base;
        row_time = base + LINE_H;
        row_status = base + LINE_H * 2;
    }
}

static void draw_panel_frame(void)
{
    struct RastPort *rp = win->RPort;

    SetAPen(rp, PEN_OCEAN);
    RectFill(rp, 0, panel_top, MAP_W - 1, win->Height - 1);
    SetAPen(rp, PEN_RULE);
    Move(rp, 0, panel_top);
    Draw(rp, MAP_W - 1, panel_top);

    if (row_title > 0)
        text_at((320 - 27 * 8) / 2, row_title, PEN_LABEL,
                "INTERNATIONAL SPACE STATION");
    if (row_help > 0)
    {
        text_at(8, row_help, PEN_RULE, "R:Now W:Crew S:Walk T:Trail N:Night");
        text_at(8, row_help + LINE_H, PEN_RULE, "V:Circle M:Sound H:Help B:Saver Q:Quit");
    }

    /* labels, and placeholders until the first fix */
    text_at(8, row_pos, PEN_LABEL, "LAT");
    text_at(8 + 4 * 8, row_pos, PEN_RULE, "  --.-- -");
    text_at(8 + 17 * 8, row_pos, PEN_LABEL, "LON");
    text_at(8 + 21 * 8, row_pos, PEN_RULE, "  --.-- -");
    field_at(1, row_time, STATUS_COLS, PEN_RULE, "----------  --:--:-- UTC");
}

/* A borderless window's title bar is shorter than its close gadget, by
 * different rows on different Kickstarts (3.x leaves the top and bottom
 * row unpainted, 2.04 the second-last). So look at what Intuition drew:
 * fill each row where the gadget is painted but the bar is not, from the
 * gadget to the bar's right end. Intuition's own redraws never reach
 * those rows, so they stay. Then redraw the title, which a fill may
 * have clipped. */
static void fill_title_bar(void)
{
    struct RastPort *rp = win->RPort;
    struct Gadget *g, *close = 0;
    int x0, right, y;

    for (g = win->FirstGadget; g; g = g->NextGadget)
        if ((g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE)
            close = g;
    if (!close || close->LeftEdge < 0 || close->Height < 2)
        return;
    x0 = close->LeftEdge + close->Width;

    /* the bar's right end, on a row every Kickstart paints */
    right = win->Width - 1;
    while (right > x0 && ReadPixel(rp, right, close->Height / 2) != PEN_TEXT)
        right--;
    if (right <= x0)
        return;

    SetAPen(rp, PEN_TEXT);
    for (y = 0; y < close->Height; y++)
        if (ReadPixel(rp, close->LeftEdge, y) == PEN_TEXT &&
            ReadPixel(rp, right, y) != PEN_TEXT)
            RectFill(rp, x0, y, right, y);
    SetWindowTitles(win, win->Title, (UBYTE *)~0);
}

int screen_open(struct Menu *menu, const char **why)
{
    struct NewScreen ns;
    struct NewWindow nw;
    int height, p;

    pal = (GfxBase->DisplayFlags & PAL) != 0;
    height = pal ? 256 : 200;

    memset(&ns, 0, sizeof ns);
    ns.Width = MAP_W;
    ns.Height = height;
    ns.Depth = DEPTH;
    ns.DetailPen = PEN_OCEAN;
    ns.BlockPen = PEN_TEXT;
    ns.Type = CUSTOMSCREEN;
    ns.Font = &topaz8;
    ns.DefaultTitle = (UBYTE *)"ISS Tracker";

    scr = OpenScreen(&ns);
    if (!scr)
    {
        *why = "Not enough chip memory for the screen";
        return 0;
    }
    LoadRGB4(&scr->ViewPort, (UWORD *)map_palette, 32);

    memset(&nw, 0, sizeof nw);
    nw.Width = MAP_W;
    nw.Height = height;
    nw.DetailPen = (UBYTE)-1;
    nw.BlockPen = (UBYTE)-1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_MENUPICK;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_BORDERLESS | WFLG_ACTIVATE |
               WFLG_SMART_REFRESH | WFLG_NOCAREREFRESH;
    nw.Title = (UBYTE *)"ISS Tracker - FujiNet";
    nw.Screen = scr;
    nw.Type = CUSTOMSCREEN;

    win = OpenWindow(&nw);
    if (!win)
    {
        *why = "Could not open the window";
        screen_close();
        return 0;
    }
    SetMenuStrip(win, menu);
    SetFont(win->RPort, scr->RastPort.Font);
    fill_title_bar();

    map_top = win->BorderTop;
    if (map_top < scr->BarHeight + 1)
        map_top = scr->BarHeight + 1;
    panel_top = map_top + MAP_H;

    InitBitMap(&off, DEPTH, MAP_W, MAP_H);
    for (p = 0; p < DEPTH; p++)
    {
        off.Planes[p] = AllocRaster(MAP_W, MAP_H);
        if (!off.Planes[p])
        {
            *why = "Not enough chip memory for the map";
            screen_close();
            return 0;
        }
    }
    off_ok = 1;

    no_pointer = AllocMem(NO_POINTER_WORDS * 2, MEMF_CHIP | MEMF_CLEAR);

    layout_panel();
    draw_panel_frame();
    return 1;
}

void screen_close(void)
{
    int p;

    screen_saver(0);
    if (no_pointer)
    {
        FreeMem(no_pointer, NO_POINTER_WORDS * 2);
        no_pointer = 0;
    }
    if (win)
    {
        ClearMenuStrip(win);
        CloseWindow(win);
        win = 0;
    }
    for (p = 0; p < DEPTH; p++)
    {
        if (off.Planes[p])
        {
            FreeRaster(off.Planes[p], MAP_W, MAP_H);
            off.Planes[p] = 0;
        }
    }
    off_ok = 0;
    if (scr)
    {
        CloseScreen(scr);
        scr = 0;
    }
}

void screen_draw_map(const iss_pos *pos, int night, int trail)
{
    struct RastPort rp;
    int p;

    if (!off_ok)
        return;

    for (p = 0; p < 4; p++)
        CopyMem((APTR)map_planes[p], off.Planes[p], MAP_PLANE_BYTES);

    if (night && pos)
    {
        night_fill(off.Planes[4], off.BytesPerRow, pos->ts);
        night_lights((unsigned char *const *)off.Planes, map_lights,
                     MAP_PLANE_BYTES);
    }
    else
        memset(off.Planes[4], 0, MAP_PLANE_BYTES);

    /* No layer, so no clipping: trail and marker stay inside the map. The
     * trail pen clears bitplane 4, keeping it bright at night. */
    InitRastPort(&rp);
    rp.BitMap = &off;
    if (trail)
    {
        SetAPen(&rp, PEN_TRAIL);
        trail_draw(&rp, 0);
    }
    home_draw_marker(&rp, 0);

    BltBitMapRastPort(&off, 0, 0, screen_map_rp(), 0, screen_map_y(),
                      MAP_W, MAP_H, 0xC0);
    footprint_forget();
    twinkle_reset();
}

struct RastPort *screen_map_rp(void)
{
    return saver ? saver->RPort : win->RPort;
}

int screen_map_y(void)
{
    return saver ? (saver->Height - MAP_H) / 2 : map_top;
}

const struct BitMap *screen_map_bitmap(void)
{
    return &off;
}

struct Window *screen_saver_window(void)
{
    return saver;
}

int screen_saver(int on)
{
    struct NewWindow nw;

    if (!on == !saver || !scr)
        return saver != 0;

    footprint_hide();
    if (on)
    {
        memset(&nw, 0, sizeof nw);
        nw.Width = scr->Width;
        nw.Height = scr->Height;
        nw.DetailPen = (UBYTE)-1;
        nw.BlockPen = (UBYTE)-1;
        nw.IDCMPFlags = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS;
        nw.Flags = WFLG_BORDERLESS | WFLG_ACTIVATE | WFLG_RMBTRAP |
                   WFLG_SMART_REFRESH | WFLG_NOCAREREFRESH;
        nw.Screen = scr;
        nw.Type = CUSTOMSCREEN;
        saver = OpenWindow(&nw);
        if (!saver)
            return 0;
        if (no_pointer)
            SetPointer(saver, no_pointer, 1, 16, 0, 0);
        SetAPen(saver->RPort, PEN_SHADOW);
        RectFill(saver->RPort, 0, 0, saver->Width - 1, saver->Height - 1);
    }
    else
    {
        CloseWindow(saver);
        saver = 0;
        ActivateWindow(win);
    }

    if (off_ok)
    {
        BltBitMapRastPort(&off, 0, 0, screen_map_rp(), 0, screen_map_y(),
                          MAP_W, MAP_H, 0xC0);
        footprint_forget();
        twinkle_reset();
    }
    return saver != 0;
}

static void format_coord(char *out, long v, char pos, char neg)
{
    char hemi = v < 0 ? neg : pos;

    if (v < 0)
        v = -v;
    sprintf(out, "%4ld.%02ld %c", v / 100, v % 100, hemi);
}

void screen_draw_position(const iss_pos *pos)
{
    char lat[16], lon[16], line[COLS + 1];
    geo_tm tm;

    format_coord(lat, pos->lat_h, 'N', 'S');
    format_coord(lon, pos->lon_h, 'E', 'W');
    text_at(8 + 4 * 8, row_pos, PEN_TEXT, lat);
    text_at(8 + 21 * 8, row_pos, PEN_TEXT, lon);

    geo_utc(pos->ts, &tm);
    sprintf(line, "%04d-%02d-%02d  %02d:%02d:%02d UTC",
            tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
    field_at(1, row_time, STATUS_COLS, PEN_TEXT, line);
}

void screen_status(const char *text, int pen)
{
    field_at(1, row_status, STATUS_COLS, pen, text);
}

void screen_home(const char *text, int pen)
{
    if (row_home > 0)
        field_at(1, row_home, HOME_COLS, pen, text);
}

void screen_countdown(int secs)
{
    char buf[12];

    if (secs < 0)
        buf[0] = 0;
    else
        sprintf(buf, "%3ds", secs);
    field_at(COLS - 6, row_status, 5, PEN_RULE, buf);
}
