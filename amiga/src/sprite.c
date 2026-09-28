/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose ISS hardware sprite with colour cycling, plus a spacewalker
 *
 * Sprite 2 is requested for the ISS because its colour registers (21-23)
 * are reserved for it by the palette (tools/png2planar.py). Sprites 0/1
 * belong to the mouse pointer. The astronaut uses the other sprite of the
 * same pair, so it shares those colours: a white suit, a gold visor that
 * shimmers with the solar arrays and a chest light pulsing with the beacon.
 * The lower numbered sprite has display priority, so the astronaut passes
 * behind the station as it goes in and out of the hatch.
 */

#include <string.h>
#include <exec/memory.h>
#include <graphics/sprite.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include "geo.h"
#include "sprite.h"

#define SPR_H 7                /* both sprites: solar panel height */
#define SPR_W 16
#define WANT_SPRITE 2

#define ASTRO_W 5              /* drawn width within the 16 pixel sprite */
#define WALK_OUT 15            /* ticks drifting out of the hatch */
#define WALK_LAP 90            /* ticks for one lap around the station */
#define WALK_STEPS (WALK_OUT + WALK_LAP + WALK_OUT)
#define WALK_RX 15             /* lap radius, pixels */
#define WALK_RY 10
#define FRAME_TICKS 3          /* ticks per swimming frame */
#define COLOUR_TICKS 2         /* ticks per colour cycle step */

extern struct IntuitionBase *IntuitionBase;

/* '1' solar arrays, '2' truss and modules, '3' beacon */
static const char *const iss_art[SPR_H] =
{
    "11.11......11.11",
    "11.11......11.11",
    "11.11..22..11.11",
    "2222222332222222",
    "11.11..22..11.11",
    "11.11......11.11",
    "11.11......11.11",
};

/* '2' suit, '1' gold visor, '3' chest light: two swimming frames */
static const char *const astro_art[2][SPR_H] =
{
    {
        ".222.",
        ".212.",
        "22222",
        ".232.",
        ".222.",
        ".2.2.",
        "2...2",
    },
    {
        ".222.",
        ".212.",
        ".222.",
        "22322",
        ".222.",
        "..2..",
        ".2.2.",
    },
};

/* Beacon pulse and solar array shimmer, 12-bit RGB */
static const UWORD beacon[] =
    { 0xFFF, 0xFFD, 0xFF9, 0xFE5, 0xFC0, 0xF80, 0xF40, 0xF80, 0xFC0, 0xFE5, 0xFF9, 0xFFD };
static const UWORD arrays[] =
    { 0xFB0, 0xFB0, 0xFC2, 0xFD4, 0xFE6, 0xFD4, 0xFC2, 0xFB0 };

#define NBEACON (sizeof beacon / sizeof beacon[0])
#define NARRAYS (sizeof arrays / sizeof arrays[0])

/* posctl, SPR_H rows of 2 words, end-of-sprite words */
#define DATA_WORDS (2 + SPR_H * 2 + 2)

typedef struct
{
    struct SimpleSprite ss;
    int num;                   /* hardware sprite, or -1 */
    UWORD *image;              /* image currently shown, 0 when blanked */
} hw_sprite;

static struct Screen *owner;
static hw_sprite iss = { { 0 }, -1, 0 };
static hw_sprite astro = { { 0 }, -1, 0 };

/* chip RAM: ISS image, two astronaut frames, blank */
static UWORD *chip;
#define CHIP_IMAGES 4
static UWORD *iss_img, *astro_img[2], *blank;

static int sx, sy;             /* ISS sprite position, off map until a fix */
static int map_y0 = -1;        /* map top row, from sprite_place */
static int suspended;
static unsigned ticks;
static int reg;                /* first colour register of the sprite pair */
static int walk = -1;          /* spacewalk step, -1 when inside */

static void sprite_tick_visual(void);

static void set_rgb(int r, UWORD c)
{
    SetRGB4(&owner->ViewPort, r, (c >> 8) & 15, (c >> 4) & 15, c & 15);
}

static void build(UWORD *img, const char *const *art, int width)
{
    int row, col;

    for (row = 0; row < SPR_H; row++)
    {
        UWORD p0 = 0, p1 = 0;

        for (col = 0; col < width; col++)
        {
            char c = art[row][col];
            UWORD bit = 0x8000 >> col;

            if (c == '1' || c == '3')
                p0 |= bit;
            if (c == '2' || c == '3')
                p1 |= bit;
        }
        img[2 + row * 2] = p0;
        img[2 + row * 2 + 1] = p1;
    }
}

/* Show img (or blank it with 0) and move the sprite to x, y. */
static void show(hw_sprite *s, UWORD *img, int x, int y)
{
    if (s->num < 0)
        return;
    if (s->image != img)
    {
        ChangeSprite(&owner->ViewPort, &s->ss, (APTR)(img ? img : blank));
        s->image = img;
    }
    MoveSprite(&owner->ViewPort, &s->ss, x, y);
}

int sprite_open(struct Screen *s)
{
    owner = s;
    chip = AllocMem(CHIP_IMAGES * DATA_WORDS * 2, MEMF_CHIP | MEMF_CLEAR);
    if (!chip)
        return 0;
    iss_img = chip;
    astro_img[0] = chip + DATA_WORDS;
    astro_img[1] = chip + 2 * DATA_WORDS;
    blank = chip + 3 * DATA_WORDS;
    build(iss_img, iss_art, SPR_W);
    build(astro_img[0], astro_art[0], ASTRO_W);
    build(astro_img[1], astro_art[1], ASTRO_W);

    iss.ss.height = SPR_H;
    iss.num = GetSprite(&iss.ss, WANT_SPRITE);
    if (iss.num < 0)
        iss.num = GetSprite(&iss.ss, WANT_SPRITE + 1);
    if (iss.num < 0)
        iss.num = GetSprite(&iss.ss, -1);
    if (iss.num < 0)
    {
        sprite_close();
        return 0;
    }
    reg = 16 + (iss.num & 6) * 2;

    /* The partner sprite shares the ISS colours; without it the tracker
     * simply has no spacewalks. */
    astro.ss.height = SPR_H;
    astro.num = GetSprite(&astro.ss, iss.num ^ 1);

    /* Park them off the map until the first fix arrives */
    sx = -SPR_W;
    sy = 0;
    show(&iss, 0, sx, sy);
    show(&astro, 0, sx, sy);
    return 1;
}

void sprite_close(void)
{
    if (astro.num >= 0)
    {
        FreeSprite(astro.num);
        astro.num = -1;
    }
    if (iss.num >= 0)
    {
        FreeSprite(iss.num);
        iss.num = -1;
    }
    if (chip)
    {
        FreeMem(chip, CHIP_IMAGES * DATA_WORDS * 2);
        chip = 0;
    }
}

void sprite_place(int x, int y, int top)
{
    sx = x - SPR_W / 2;
    sy = top + y - SPR_H / 2;
    map_y0 = top;
    sprite_tick_visual();
}

int sprite_spacewalk(void)
{
    if (astro.num < 0 || walk >= 0 || map_y0 < 0)
        return 0;
    walk = 0;
    return 1;
}

/* Astronaut position for the current walk step, relative to the ISS. */
static void walk_offset(int *dx, int *dy)
{
    long ang = 27000L;         /* straight up out of the hatch */
    long r = WALK_OUT;         /* radius in 1/WALK_OUT units */

    if (walk < WALK_OUT)
        r = walk;
    else if (walk < WALK_OUT + WALK_LAP)
        ang += (long)(walk - WALK_OUT) * 36000L / WALK_LAP;
    else
        r = WALK_STEPS - walk;

    *dx = (int)(r * WALK_RX * geo_cos(ang) / (WALK_OUT * GEO_ONE));
    *dy = (int)(r * WALK_RY * geo_sin(ang) / (WALK_OUT * GEO_ONE));
}

static void sprite_tick_visual(void)
{
    int front = IntuitionBase->FirstScreen == owner && sx > -SPR_W &&
                !suspended;

    show(&iss, front ? iss_img : 0, sx, sy);

    if (front && walk >= 0)
    {
        int dx, dy, ax, ay;

        walk_offset(&dx, &dy);
        /* centre the 5x7 figure on the ISS centre plus the offset, kept
         * on the map when the station is near an edge */
        ax = sx + SPR_W / 2 + dx - ASTRO_W / 2;
        ay = sy + dy;
        if (ax < 0)
            ax = 0;
        if (ax > 319 - ASTRO_W)
            ax = 319 - ASTRO_W;
        if (ay < map_y0)
            ay = map_y0;
        if (ay > map_y0 + MAP_H - SPR_H)
            ay = map_y0 + MAP_H - SPR_H;
        show(&astro, astro_img[(ticks / FRAME_TICKS) & 1], ax, ay);
    }
    else
        show(&astro, 0, sx, sy);
}

int sprite_tick(void)
{
    int ended = 0;

    if (iss.num < 0)
        return 0;

    ticks++;
    if (ticks % COLOUR_TICKS == 0)
    {
        unsigned phase = ticks / COLOUR_TICKS;

        set_rgb(reg + 1, arrays[phase % NARRAYS]);
        set_rgb(reg + 2, 0xCCD);
        set_rgb(reg + 3, beacon[phase % NBEACON]);
    }

    if (walk >= 0 && ++walk >= WALK_STEPS)
    {
        walk = -1;
        ended = 1;
    }

    sprite_tick_visual();
    return ended;
}

void sprite_suspend(int on)
{
    suspended = on;
    if (iss.num >= 0)
        sprite_tick_visual();
}
