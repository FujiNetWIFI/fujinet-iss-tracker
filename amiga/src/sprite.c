/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose ISS hardware sprite with colour cycling
 *
 * Sprite 2 is requested because its colour registers (21-23) are reserved
 * for it by the palette (tools/png2planar.py). Sprites 0/1 belong to the
 * mouse pointer.
 */

#include <string.h>
#include <exec/memory.h>
#include <graphics/sprite.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include "sprite.h"

#define SPR_H 7
#define SPR_W 16
#define WANT_SPRITE 2

extern struct IntuitionBase *IntuitionBase;

/* '1' solar arrays, '2' truss and modules, '3' beacon */
static const char *const art[SPR_H] =
{
    "11.11......11.11",
    "11.11......11.11",
    "11.11..22..11.11",
    "2222222332222222",
    "11.11..22..11.11",
    "11.11......11.11",
    "11.11......11.11",
};

/* Beacon pulse and solar array shimmer, 12-bit RGB */
static const UWORD beacon[] =
    { 0xFFF, 0xFFD, 0xFF9, 0xFE5, 0xFC0, 0xF80, 0xF40, 0xF80, 0xFC0, 0xFE5, 0xFF9, 0xFFD };
static const UWORD arrays[] =
    { 0xFB0, 0xFB0, 0xFC2, 0xFD4, 0xFE6, 0xFD4, 0xFC2, 0xFB0 };

#define NBEACON (sizeof beacon / sizeof beacon[0])
#define NARRAYS (sizeof arrays / sizeof arrays[0])

static struct SimpleSprite ss;
static struct Screen *owner;
static UWORD *shown;      /* chip RAM: posctl, SPR_H rows of 2 words, end */
static UWORD *blank;
static int num = -1;
static int sx, sy;
static int hidden;
static int suspended;
static unsigned phase;
static int reg;           /* first colour register of this sprite pair */

#define DATA_WORDS (2 + SPR_H * 2 + 2)

static void set_rgb(int r, UWORD c)
{
    SetRGB4(&owner->ViewPort, r, (c >> 8) & 15, (c >> 4) & 15, c & 15);
}

int sprite_open(struct Screen *s)
{
    int row, col;

    owner = s;
    shown = AllocMem(DATA_WORDS * 2, MEMF_CHIP | MEMF_CLEAR);
    blank = AllocMem(DATA_WORDS * 2, MEMF_CHIP | MEMF_CLEAR);
    if (!shown || !blank)
    {
        sprite_close();
        return 0;
    }

    for (row = 0; row < SPR_H; row++)
    {
        UWORD p0 = 0, p1 = 0;

        for (col = 0; col < SPR_W; col++)
        {
            char c = art[row][col];
            UWORD bit = 0x8000 >> col;

            if (c == '1' || c == '3')
                p0 |= bit;
            if (c == '2' || c == '3')
                p1 |= bit;
        }
        shown[2 + row * 2] = p0;
        shown[2 + row * 2 + 1] = p1;
    }

    memset(&ss, 0, sizeof ss);
    ss.height = SPR_H;
    num = GetSprite(&ss, WANT_SPRITE);
    if (num < 0)
        num = GetSprite(&ss, WANT_SPRITE + 1);
    if (num < 0)
        num = GetSprite(&ss, -1);
    if (num < 0)
    {
        sprite_close();
        return 0;
    }
    reg = 16 + (num & 6) * 2;

    /* Park it off the map until the first fix arrives */
    sx = -SPR_W;
    sy = 0;
    hidden = 1;
    ChangeSprite(&owner->ViewPort, &ss, (APTR)blank);
    return 1;
}

void sprite_close(void)
{
    if (num >= 0)
    {
        FreeSprite(num);
        num = -1;
    }
    if (shown)
    {
        FreeMem(shown, DATA_WORDS * 2);
        shown = 0;
    }
    if (blank)
    {
        FreeMem(blank, DATA_WORDS * 2);
        blank = 0;
    }
}

void sprite_place(int x, int y, int top)
{
    if (num < 0)
        return;
    sx = x - SPR_W / 2;
    sy = top + y - SPR_H / 2;
    MoveSprite(&owner->ViewPort, &ss, sx, sy);
    if (hidden && !suspended && IntuitionBase->FirstScreen == owner)
    {
        ChangeSprite(&owner->ViewPort, &ss, (APTR)shown);
        hidden = 0;
    }
}

void sprite_tick(void)
{
    int front;

    if (num < 0)
        return;

    phase++;
    set_rgb(reg + 1, arrays[phase % NARRAYS]);
    set_rgb(reg + 2, 0xCCD);
    set_rgb(reg + 3, beacon[phase % NBEACON]);

    front = IntuitionBase->FirstScreen == owner && sx > -SPR_W && !suspended;
    if (front && hidden)
    {
        ChangeSprite(&owner->ViewPort, &ss, (APTR)shown);
        hidden = 0;
    }
    else if (!front && !hidden)
    {
        ChangeSprite(&owner->ViewPort, &ss, (APTR)blank);
        hidden = 1;
    }
    MoveSprite(&owner->ViewPort, &ss, sx, sy);
}

void sprite_suspend(int on)
{
    suspended = on;
    sprite_tick();
}
