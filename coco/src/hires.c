/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose CoCo 3 640x192x4 hi-res via direct GIME/MMU, mirrors the pmode3 API.
 */

#ifdef COCO3

#include <cmoc.h>
#include <coco.h>
#include "pmode3.h"
#include "charset_coco3.h"

#define SCREEN_PTR    ((byte *)0x8000)
#define BYTES_PER_ROW 160
#define SCREEN_BYTES  30720U

/* MMU task 1: low half same as task 0 (code/data stay mapped),
 * high half = the graphics blocks holding the screen buffer. */
static const byte video_mmu_blocks[8] = {
    56, 57, 58, 59,
    52, 53, 54, 55,
};

/* slots: 0 ocean, 1 land, 2 panel/coast, 3 text/satellite (RGB 6-bit) */
byte palette[4] = { 11, 20, 0, 63 };

static const byte andTable[4] = { 0x3F, 0xCF, 0xF3, 0xFC };
static const byte orTable[4][4] =
{
    { 0x00, 0x00, 0x00, 0x00 },
    { 0x40, 0x10, 0x04, 0x01 },
    { 0x80, 0x20, 0x08, 0x02 },
    { 0xC0, 0x30, 0x0C, 0x03 },
};

#define mmu_use_main()  (*(byte *)0xFF91 = 0)
#define mmu_use_video() (*(byte *)0xFF91 = 1)
#define BEGIN_GFX       disableInterrupts(); mmu_use_video();
#define END_GFX         mmu_use_main(); enableInterrupts();

void pmode3(void)
{
    byte i;
    byte *pal_io = (byte *)0xFFB0;

    initCoCoSupport();
    disableInterrupts();

    for (i = 0; i < 8; i++)
        ((byte *)0xFFA8)[i] = video_mmu_blocks[i];

    for (i = 0; i < 4; i++)
        pal_io[i] = palette[i];

    asm { sync }

    *(byte *)0xFF90 = 0x4C;   /* native CoCo 3, MMU enabled */
    *(byte *)0xFF98 = 0x80;   /* graphics mode */
    *(byte *)0xFF99 = 0x1D;   /* 640x192x4 */
    *(byte *)0xFF9A = 0;
    *(byte *)0xFF9D = 0xD0;   /* video start = block 52 */
    *(byte *)0xFF9E = 0x00;

    mmu_use_video();
    memset(SCREEN_PTR, 0, SCREEN_BYTES);
    mmu_use_main();

    enableInterrupts();
}

void pset(int x, int y, unsigned char c)
{
    byte *p = SCREEN_PTR + (word)y * BYTES_PER_ROW + (x >> 2);
    BEGIN_GFX
    *p = (*p & andTable[x & 3]) | orTable[c][x & 3];
    END_GFX
}

void pxor(int x, int y)
{
    byte *p = SCREEN_PTR + (word)y * BYTES_PER_ROW + (x >> 2);
    BEGIN_GFX
    *p ^= orTable[3][x & 3];
    END_GFX
}

/* RLE source is small and lives below $8000, so it stays mapped while
 * task 1 exposes the video buffer at $8000. */
void blit_rle(const byte *rle, word len)
{
    byte *dst = SCREEN_PTR;
    word i;
    BEGIN_GFX
    for (i = 0; i < len; i += 2)
    {
        byte count = rle[i];
        byte val = rle[i + 1];
        do { *dst++ = val; } while (--count);
    }
    END_GFX
}

void pmode3_xor(void)
{
}

void clear_bottom(void)
{
    BEGIN_GFX
    memset(SCREEN_PTR + (word)144 * BYTES_PER_ROW, 0xAA, (word)48 * BYTES_PER_ROW);
    END_GFX
}

void hires_cls(byte c)
{
    byte fill = (byte)((c << 6) | (c << 4) | (c << 2) | c);
    BEGIN_GFX
    memset(SCREEN_PTR, fill, SCREEN_BYTES);
    END_GFX
}

static byte glyph_byte(byte g, byte color)
{
    byte out = 0;
    if (g & 0xC0) out |= color << 6;
    if (g & 0x30) out |= color << 4;
    if (g & 0x0C) out |= color << 2;
    if (g & 0x03) out |= color;
    return out;
}

static byte lit_mask(byte g)
{
    byte m = 0;
    if (g & 0xC0) m |= 0xC0;
    if (g & 0x30) m |= 0x30;
    if (g & 0x0C) m |= 0x0C;
    if (g & 0x03) m |= 0x03;
    return m;
}

/* transparent: only the glyph's lit pixels are drawn */
void putc(int x, int y, char ch, char color)
{
    unsigned char uch = (unsigned char)ch;
    const byte *src;
    byte *dest = SCREEN_PTR + (word)y * BYTES_PER_ROW + (x >> 2);
    byte i;

    if (uch >= CHARSET_COCO3_N) uch = 0;
    src = &charset_coco3[uch][0];

    BEGIN_GFX
    for (i = 0; i < 8; i++)
    {
        dest[0] = (byte)((dest[0] & ~lit_mask(src[0])) | glyph_byte(src[0], color));
        dest[1] = (byte)((dest[1] & ~lit_mask(src[1])) | glyph_byte(src[1], color));
        src += 2;
        dest += BYTES_PER_ROW;
    }
    END_GFX
}

void puts(int x, int y, char color, const char *s)
{
    while (*s)
    {
        putc(x, y, *s++, color);
        x += 8;
    }
}

void putc2x(int x, int y, char ch, char color)
{
    unsigned char uch = (unsigned char)ch;
    const byte *src;
    byte r, c;

    if (uch >= CHARSET_COCO3_N) uch = 0;
    src = &charset_coco3[uch][0];

    for (r = 0; r < 8; r++)
    {
        byte g0 = src[0], g1 = src[1];
        src += 2;
        for (c = 0; c < 8; c++)
        {
            byte px = (c < 4) ? (g0 >> ((3 - c) * 2)) & 3
                              : (g1 >> ((3 - (c - 4)) * 2)) & 3;
            if (px)
            {
                int dx = x + (c << 1);
                int dy = y + (r << 1);
                pset(dx,     dy,     color);
                pset(dx + 1, dy,     color);
                pset(dx,     dy + 1, color);
                pset(dx + 1, dy + 1, color);
            }
        }
    }
}

void puts2x_centered(int y, char color, const char *s)
{
    int x = (640 - (int)strlen(s) * 16) / 2;
    if (x < 0) x = 0;
    while (*s)
    {
        putc2x(x, y, *s++, color);
        x += 16;
    }
}

unsigned char input_delay(unsigned char view)
{
    word start = getTimer();

    while ((word)(getTimer() - start) < 7200U)
    {
        if (inkey() == ' ')
        {
            while (inkey())
                ;
            return (unsigned char)(view ^ 1);
        }
    }
    return view;
}

#endif /* COCO3 */
