/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose UFO sighting: flight path and artwork (portable, integer only)
 */

#include "geo.h"
#include "ufo_path.h"

#define U 1024L                  /* curve parameter: 0 .. U */
#define EDGE 4                   /* curves stay this far inside the map ... */
#define OFF 24                   /* ... but flights start and end this far off it */
#define CORNER 20                /* and at least this far from a corner */
#define SWOOP 60                 /* sideways throw of the curves, pixels */

/* Hover timeline, in hover frames */
#define PEEK_AT   10             /* the head starts to come up */
#define LEFT_AT   36
#define RIGHT_AT  52
#define AHEAD_AT  68
#define SINK_AT   78             /* ... and goes back down */
#define GONE_AT   90
#define BOB_STEP  1440L          /* 1/25 turn per frame: a one second bob */

static const char *const body0[] =
{
    ".c.",
    "gLg",
};

static const char *const body1[] =
{
    "...kck...",
    "kgLgggLgk",
    "..kkkkk..",
};

static const char *const body2[] =
{
    "......kkk......",
    ".....kcwck.....",
    "..kkgggggggkk..",
    "kgLggLgggLggLgk",
    "..kkkkkkkkkkk..",
};

static const char *const body3[] =
{
    "........kkkkkkk........",
    ".......kcwccccck.......",
    "......kccccccccck......",
    "...kkgggggggggggggkk...",
    "kgwwggggggggggggggggggk",
    "kggLgggLgggLgggLgggLggk",
    "...kkgggggggggggggkk...",
    "......kkkkkkkkkkk......",
};

static const char *const body4[] =
{
    "............kkkkkkkkk............",
    "...........kcwwcccccck...........",
    "..........kccccccccccck..........",
    ".........kccccccccccccck.........",
    "....kkkgggggggggggggggggggkkk....",
    "kgwwggggggggggggggggggggggggggggk",
    "kgggLgggggLgggggLgggggLgggggLgggk",
    "..kkgggggggggggggggggggggggggkk..",
    ".......kkkkkkkkkkkkkkkkkkk.......",
};

static const char *const body5[] =
{
    ".................kkkkkkkkkkkkk.................",
    "................kccwwccccccccck................",
    "..............kcwwcccccccccccccck..............",
    ".............kccccccccccccccccccck.............",
    "............kccccccccccccccccccccck............",
    ".......kkkkgggggggggggggggggggggggggkkkk.......",
    "...kkgwwwgggggggggggggggggggggggggggggggggkk...",
    "kgwwggggggggggggggggggggggggggggggggggggggggggk",
    "kggggLgggggLgggggLgggggLgggggLgggggLgggggLggggk",
    "kgggggggggggggggggggggggggggggggggggggggggggggk",
    "...kkgggggggggggggggggggggggggggggggggggggkk...",
    ".......kkkkgggggggggggggggggggggggggkkkk.......",
    "............kkkkkkkkkkkkkkkkkkkkkkk............",
};

/* The pilot: white eyes, pupils showing where it looks */
#define HEAD_TOP \
    ".aa.......aa.", \
    "..a.......a..", \
    "...a.....a...", \
    "....aaaaa....", \
    "..aaaaaaaaa..", \
    ".awwwaaawwwa."
#define HEAD_BOTTOM \
    ".aaaaaaaaaaa.", \
    "..aaaaaaaaa..", \
    "...aaakaaa...", \
    "....aaaaa...."

static const char *const head_ahead[] =
{
    HEAD_TOP,
    ".awkwaaawkwa.",
    ".awwwaaawwwa.",
    HEAD_BOTTOM,
};

static const char *const head_left[] =
{
    HEAD_TOP,
    ".akwwaaakwwa.",
    ".awwwaaawwwa.",
    HEAD_BOTTOM,
};

static const char *const head_right[] =
{
    HEAD_TOP,
    ".awwkaaawwka.",
    ".awwwaaawwwa.",
    HEAD_BOTTOM,
};

const ufo_art ufo_body[UFO_SIZES] =
{
    { 3, 2, body0 },
    { 9, 3, body1 },
    { 15, 5, body2 },
    { 23, 8, body3 },
    { 33, 9, body4 },
    { 47, 13, body5 },
};

const ufo_art ufo_head[UFO_LOOKS] =
{
    { UFO_HEAD_W, UFO_HEAD_H, head_ahead },
    { UFO_HEAD_W, UFO_HEAD_H, head_left },
    { UFO_HEAD_W, UFO_HEAD_H, head_right },
};

unsigned long ufo_rand(unsigned long *seed)
{
    *seed = *seed * 1103515245UL + 12345UL;
    return (*seed >> 16) & 0x7FFF;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* A point just off a map edge, clear of the corners: side 0 left,
 * 1 right, 2 top, 3 bottom */
static ufo_pt edge_point(int side, unsigned long *seed)
{
    ufo_pt p;

    if (side < 2)
    {
        p.x = side ? MAP_W - 1 + OFF : -OFF;
        p.y = CORNER + ufo_rand(seed) % (MAP_H - 2 * CORNER);
    }
    else
    {
        p.x = CORNER + ufo_rand(seed) % (MAP_W - 2 * CORNER);
        p.y = side == 2 ? -OFF : MAP_H - 1 + OFF;
    }
    return p;
}

/* Control point: midway between a and b, thrown sideways for a swoop */
static ufo_pt swoop(ufo_pt a, ufo_pt b, unsigned long *seed)
{
    ufo_pt c;

    c.x = clampi((a.x + b.x) / 2 + (int)(ufo_rand(seed) % (2 * SWOOP + 1)) - SWOOP,
                 EDGE, MAP_W - 1 - EDGE);
    c.y = clampi((a.y + b.y) / 2 + (int)(ufo_rand(seed) % (2 * SWOOP + 1)) - SWOOP,
                 EDGE, MAP_H - 1 - EDGE);
    return c;
}

void ufo_path_init(ufo_path *p, unsigned long *seed)
{
    int from = (int)(ufo_rand(seed) % 4);
    int to = (from + 1 + (int)(ufo_rand(seed) % 3)) % 4;

    p->hover.x = 90 + ufo_rand(seed) % 141;
    p->hover.y = 45 + ufo_rand(seed) % 61;
    p->start = edge_point(from, seed);
    p->end = edge_point(to, seed);
    p->in = swoop(p->start, p->hover, seed);
    p->out = swoop(p->hover, p->end, seed);
}

/* Quadratic Bezier */
static int bez(int a, int c, int b, long u)
{
    long v = U - u;
    long sum = v * v * a + 2 * v * u * c + u * u * b;

    /* round towards minus infinity, so off-map points stay off it */
    return (int)(sum >= 0 ? sum / (U * U) : -((-sum + U * U - 1) / (U * U)));
}

/* Perspective: the saucer only looms large at the end of its approach */
static int size_at(long t)
{
    long s = t * t * UFO_SIZES / (U * U);

    return s >= UFO_SIZES ? UFO_SIZES - 1 : (int)s;
}

static void alien(int k, ufo_pose *o)
{
    if (k < PEEK_AT || k >= GONE_AT)
        return;
    if (k < PEEK_AT + UFO_HEAD_H)
        o->rise = k - PEEK_AT + 1;
    else if (k >= SINK_AT)
        o->rise = UFO_HEAD_H - (k - SINK_AT);
    else
    {
        o->rise = UFO_HEAD_H;
        o->look = k >= LEFT_AT && k < RIGHT_AT ? 1 :
                  k >= RIGHT_AT && k < AHEAD_AT ? 2 : 0;
    }
}

int ufo_path_step(const ufo_path *p, int step, ufo_pose *o)
{
    long t, u;

    o->rise = 0;
    o->look = 0;
    if (step < 0 || step >= UFO_STEPS)
        return 0;

    if (step < UFO_IN)
    {
        t = (long)step * U / (UFO_IN - 1);
        u = U - (U - t) * (U - t) / U;          /* ease out into the hover */
        o->x = bez(p->start.x, p->in.x, p->hover.x, u);
        o->y = bez(p->start.y, p->in.y, p->hover.y, u);
        o->size = size_at(t);
        return 1;
    }

    step -= UFO_IN;
    if (step < UFO_HOVER)
    {
        o->x = p->hover.x;
        o->y = p->hover.y + (int)(geo_sin(step * BOB_STEP) * 3 / (2 * GEO_ONE));
        o->size = UFO_SIZES - 1;
        alien(step, o);
        return 1;
    }

    step -= UFO_HOVER;
    t = (long)step * U / (UFO_OUT - 1);
    u = t * t / U;                              /* ease in: accelerate away */
    o->x = bez(p->hover.x, p->out.x, p->end.x, u);
    o->y = bez(p->hover.y, p->out.y, p->end.y, u);
    o->size = size_at(U - t);
    return 1;
}
