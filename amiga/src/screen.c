/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Custom screen, main window, map compositor and status panel
 *
 * Kickstart 1.3 compatible: NewScreen/NewWindow structures, no tag lists.
 * The map is composed in an off-screen chip RAM bitmap and blitted through
 * the window's layer, so menus and the crew window are never overdrawn.
 */

#include <string.h>
#include <stdio.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "geo.h"
#include "map_data.h"
#include "night.h"
#include "screen.h"
#include "trail.h"

#define DEPTH 5
#define COLS  (320 / 8)          /* characters per panel line */
#define LINE_H 9

extern struct GfxBase *GfxBase;

struct Screen *scr;
struct Window *win;
int map_top;

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
static struct BitMap off;        /* MAP_W x MAP_H, 5 planes */
static int off_ok;
static int pal;                  /* PAL (256 lines) or NTSC (200) */
static int panel_top;

/* Panel text rows (baseline y) for PAL and NTSC layouts */
static int row_title, row_pos, row_time, row_status, row_help;

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
        row_status = row_time + LINE_H * 2;
        row_help = row_status + LINE_H * 2;
    }
    else
    {
        row_title = row_help = -1;
        row_pos = base;
        row_time = base + LINE_H;
        row_status = base + LINE_H * 2;
    }
}

static void draw_panel_frame(void)
{
    struct RastPort *rp = win->RPort;

    SetAPen(rp, PEN_OCEAN);
    RectFill(rp, 0, panel_top, 319, win->Height - 1);
    SetAPen(rp, PEN_RULE);
    Move(rp, 0, panel_top);
    Draw(rp, 319, panel_top);

    if (row_title > 0)
        text_at((320 - 27 * 8) / 2, row_title, PEN_LABEL,
                "INTERNATIONAL SPACE STATION");
    if (row_help > 0)
        text_at(8, row_help, PEN_RULE, "R:Now W:Crew T:Trail N:Night Q:Quit");

    /* Placeholders until the first fix arrives */
    text_at(8, row_pos, PEN_LABEL, "LAT");
    text_at(8 + 4 * 8, row_pos, PEN_RULE, "  --.-- -");
    text_at(8 + 17 * 8, row_pos, PEN_LABEL, "LON");
    text_at(8 + 21 * 8, row_pos, PEN_RULE, "  --.-- -");
    field_at(1, row_time, 28, PEN_RULE, "----------  --:--:-- UTC");
}

int screen_open(struct Menu *menu, const char **why)
{
    struct NewScreen ns;
    struct NewWindow nw;
    int height, p;

    pal = (GfxBase->DisplayFlags & PAL) != 0;
    height = pal ? 256 : 200;

    memset(&ns, 0, sizeof ns);
    ns.Width = 320;
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
    nw.Width = 320;
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

    layout_panel();
    draw_panel_frame();
    return 1;
}

void screen_close(void)
{
    int p;

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
        night_fill(off.Planes[4], off.BytesPerRow, pos->ts);
    else
        memset(off.Planes[4], 0, MAP_PLANE_BYTES);

    if (trail)
    {
        /* A plain bitmap RastPort has no clipping; trail points are
         * clamped to the map by geo_lon_to_x/geo_lat_to_y. The trail pen
         * clears bitplane 4, so the track stays bright on the night side. */
        InitRastPort(&rp);
        rp.BitMap = &off;
        SetAPen(&rp, PEN_TRAIL);
        trail_draw(&rp, 0);
    }

    BltBitMapRastPort(&off, 0, 0, win->RPort, 0, map_top, MAP_W, MAP_H, 0xC0);
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

    text_at(8, row_pos, PEN_LABEL, "LAT");
    text_at(8 + 4 * 8, row_pos, PEN_TEXT, lat);
    text_at(8 + 17 * 8, row_pos, PEN_LABEL, "LON");
    text_at(8 + 21 * 8, row_pos, PEN_TEXT, lon);

    geo_utc(pos->ts, &tm);
    sprintf(line, "%04d-%02d-%02d  %02d:%02d:%02d UTC",
            tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
    field_at(1, row_time, 28, PEN_TEXT, line);
}

void screen_status(const char *text, int pen)
{
    field_at(1, row_status, 28, pen, text);
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
