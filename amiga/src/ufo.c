/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose UFO sightings: a blitter-drawn saucer and its curious pilot
 *
 * No hardware sprite is free (sprites 4-7 would recolour the night map),
 * so the saucer is a masked blit. Each frame the map under its old and new
 * positions is copied from the composed map into a scratch bitmap, the
 * saucer is cut in there, and the result goes to the screen in one blit:
 * nothing is erased first, so nothing flickers.
 *
 * The saucer flies in from off the map. Scratch covers its whole
 * rectangle, so the image blits need no clipping; only the part over the
 * map is filled from the map and copied to the screen.
 */

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include "geo.h"
#include "map_data.h"
#include "ufo.h"
#include "ufo_path.h"

#define DEPTH 5
#define SCR_W 96                 /* scratch: old and new saucer together */
#define SCR_H 48
#define MAX_LIGHTS 8
#define LIGHT_FRAMES 2           /* frames per rim light chase step */
#define MINTERM_COPY 0xC0
#define MINTERM_COOKIE 0xE0      /* ABC | ABNC | ANBC: through the mask */

typedef struct
{
    struct BitMap bm;
    PLANEPTR mask;
    int w, h;
    int nlights;
    UBYTE lx[MAX_LIGHTS], ly[MAX_LIGHTS];
} image;

typedef struct
{
    int x, y, w, h;
} rect;

static UBYTE *chip;
static long chip_size;
static image body[UFO_SIZES], head[UFO_LOOKS];
static struct BitMap scratch;
static struct RastPort srp;

static const UBYTE light_pens[] = { PEN_ERROR, PEN_TRAIL, PEN_TEXT };

static ufo_path path;
static int step = -1;            /* frame of the flight, -1 when none */
static rect shown;               /* where the saucer is drawn (unclipped) */
static struct RastPort *shown_rp;
static int shown_y0;
static int have_shown;

static int pen_of(char c)
{
    switch (c)
    {
    case 'k': return PEN_SHADOW;
    case 'g': return PEN_RULE;
    case 'w': return PEN_TEXT;
    case 'c': return PEN_LABEL;
    case 'L': return PEN_ERROR;
    case 'a': return PEN_ALIEN;
    }
    return -1;
}

static long image_bytes(const ufo_art *art)
{
    return (long)((art->w + 15) >> 4) * 2 * art->h * (DEPTH + 1);
}

/* Build art into im, planes and mask carved from mem */
static UBYTE *carve(image *im, const ufo_art *art, UBYTE *mem)
{
    int bpr = ((art->w + 15) >> 4) * 2;
    int plane = bpr * art->h;
    int p, x, y;

    InitBitMap(&im->bm, DEPTH, art->w, art->h);
    for (p = 0; p < DEPTH; p++)
    {
        im->bm.Planes[p] = mem;
        mem += plane;
    }
    im->mask = mem;
    mem += plane;
    im->w = art->w;
    im->h = art->h;
    im->nlights = 0;

    for (y = 0; y < art->h; y++)
        for (x = 0; x < art->w; x++)
        {
            char c = art->rows[y][x];
            int pen = pen_of(c);
            int byte = y * bpr + x / 8;
            UBYTE bit = 0x80 >> (x & 7);

            if (pen < 0)
                continue;
            im->mask[byte] |= bit;
            for (p = 0; p < DEPTH; p++)
                if (pen & (1 << p))
                    im->bm.Planes[p][byte] |= bit;
            if (c == 'L' && im->nlights < MAX_LIGHTS)
            {
                im->lx[im->nlights] = (UBYTE)x;
                im->ly[im->nlights] = (UBYTE)y;
                im->nlights++;
            }
        }
    return mem;
}

int ufo_open(void)
{
    long size = (long)DEPTH * (SCR_W / 8) * SCR_H;
    UBYTE *mem;
    int i;

    for (i = 0; i < UFO_SIZES; i++)
        size += image_bytes(&ufo_body[i]);
    for (i = 0; i < UFO_LOOKS; i++)
        size += image_bytes(&ufo_head[i]);

    chip = AllocMem(size, MEMF_CHIP | MEMF_CLEAR);
    if (!chip)
        return 0;
    chip_size = size;

    mem = chip;
    InitBitMap(&scratch, DEPTH, SCR_W, SCR_H);
    for (i = 0; i < DEPTH; i++)
    {
        scratch.Planes[i] = mem;
        mem += (SCR_W / 8) * SCR_H;
    }
    InitRastPort(&srp);
    srp.BitMap = &scratch;

    for (i = 0; i < UFO_SIZES; i++)
        mem = carve(&body[i], &ufo_body[i], mem);
    for (i = 0; i < UFO_LOOKS; i++)
        mem = carve(&head[i], &ufo_head[i], mem);
    return 1;
}

void ufo_close(void)
{
    if (chip)
        FreeMem(chip, chip_size);
    chip = 0;
    step = -1;
}

int ufo_start(unsigned long *seed)
{
    if (!chip || step >= 0)
        return 0;
    ufo_path_init(&path, seed);
    step = 0;
    have_shown = 0;
    return 1;
}

int ufo_active(void)
{
    return step >= 0;
}

/* The part of r over the map; returns 0 if none is */
static int clip(const rect *r, rect *v)
{
    int x1 = r->x + r->w, y1 = r->y + r->h;

    v->x = r->x < 0 ? 0 : r->x;
    v->y = r->y < 0 ? 0 : r->y;
    v->w = (x1 > MAP_W ? MAP_W : x1) - v->x;
    v->h = (y1 > MAP_H ? MAP_H : y1) - v->y;
    return v->w > 0 && v->h > 0;
}

static void restore(const struct BitMap *map, const rect *r)
{
    rect v;

    if (clip(r, &v))
        BltBitMapRastPort((struct BitMap *)map, v.x, v.y, shown_rp,
                          v.x, shown_y0 + v.y, v.w, v.h, MINTERM_COPY);
}

int ufo_frame(struct RastPort *rp, int y0, const struct BitMap *map)
{
    ufo_pose pose;
    image *im;
    rect r, u, v;
    int bx, by, i;

    if (step < 0)
        return 0;
    /* shown somewhere else (screen saver on or off): that map was redrawn */
    if (rp != shown_rp || y0 != shown_y0)
        have_shown = 0;

    if (!ufo_path_step(&path, step, &pose))
    {
        if (have_shown)
            restore(map, &shown);
        have_shown = 0;
        step = -1;
        return 0;
    }

    /* the full-size saucer carries head room above its dome */
    im = &body[pose.size];
    r.w = im->w;
    r.h = im->h + (pose.size == UFO_SIZES - 1 ? UFO_HEAD_H : 0);
    r.x = pose.x - im->w / 2;
    r.y = pose.y - im->h / 2 - (r.h - im->h);

    u = r;
    if (have_shown)
    {
        u.x = shown.x < r.x ? shown.x : r.x;
        u.y = shown.y < r.y ? shown.y : r.y;
        u.w = (shown.x + shown.w > r.x + r.w ? shown.x + shown.w : r.x + r.w) - u.x;
        u.h = (shown.y + shown.h > r.y + r.h ? shown.y + shown.h : r.y + r.h) - u.y;
        if (u.w > SCR_W || u.h > SCR_H)
        {
            restore(map, &shown);        /* too far apart: two steps */
            u = r;
        }
    }

    shown = r;
    shown_rp = rp;
    shown_y0 = y0;
    have_shown = 1;
    step++;
    if (!clip(&u, &v))
        return 1;                        /* still off the map */

    BltBitMap((struct BitMap *)map, v.x, v.y, &scratch, v.x - u.x, v.y - u.y,
              v.w, v.h, MINTERM_COPY, 0xFF, 0);
    bx = r.x - u.x;
    by = r.y - u.y + (r.h - im->h);
    BltMaskBitMapRastPort(&im->bm, 0, 0, &srp, bx, by, im->w, im->h,
                          MINTERM_COOKIE, im->mask);
    for (i = 0; i < im->nlights; i++)
    {
        SetAPen(&srp, light_pens[(step / LIGHT_FRAMES + i) % 3]);
        WritePixel(&srp, bx + im->lx[i], by + im->ly[i]);
    }
    if (pose.rise > 0)
    {
        image *hd = &head[pose.look];

        /* the top rows of the head, emerging from the dome */
        BltMaskBitMapRastPort(&hd->bm, 0, 0, &srp, bx + (im->w - hd->w) / 2,
                              by - pose.rise, hd->w, pose.rise,
                              MINTERM_COOKIE, hd->mask);
    }
    BltBitMapRastPort(&scratch, v.x - u.x, v.y - u.y, rp, v.x, y0 + v.y,
                      v.w, v.h, MINTERM_COPY);
    return 1;
}
