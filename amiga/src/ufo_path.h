/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose UFO sighting: flight path and artwork (portable, integer only)
 *
 * A sighting is a fixed number of frames: the saucer swoops in from a map
 * edge, growing as it nears, hovers while its pilot pokes its head out for
 * a look around, then shrinks away towards another edge.
 */

#ifndef UFO_PATH_H
#define UFO_PATH_H

#define UFO_SIZES 4              /* distant speck ... full-size saucer */
#define UFO_LOOKS 3              /* the pilot looks ahead, left, right */
#define UFO_HEAD_W 7
#define UFO_HEAD_H 6

#define UFO_IN    50             /* frames swooping in */
#define UFO_HOVER 100            /* frames hovering */
#define UFO_OUT   45             /* frames flying off */
#define UFO_STEPS (UFO_IN + UFO_HOVER + UFO_OUT)

/* Artwork rows: '.' see-through, 'k' outline, 'g' hull, 'w' highlight,
 * 'c' glass, 'L' rim light (colour cycled), 'a' the alien */
typedef struct
{
    int w, h;
    const char *const *rows;
} ufo_art;

extern const ufo_art ufo_body[UFO_SIZES];
extern const ufo_art ufo_head[UFO_LOOKS];

typedef struct
{
    short x, y;
} ufo_pt;

/* Two quadratic curves: start -> hover (via in), hover -> end (via out) */
typedef struct
{
    ufo_pt start, in, hover, out, end;
} ufo_path;

typedef struct
{
    int x, y;                    /* saucer centre, map pixels */
    int size;                    /* 0 .. UFO_SIZES - 1 */
    int rise;                    /* head rows showing above the dome */
    int look;                    /* 0 .. UFO_LOOKS - 1 */
} ufo_pose;

/* Small LCG, 0..32767 */
unsigned long ufo_rand(unsigned long *seed);

/* Pick a random flight: in from one map edge, out towards another. */
void ufo_path_init(ufo_path *p, unsigned long *seed);

/* Pose for frame step; returns 0 once the flight is over. */
int ufo_path_step(const ufo_path *p, int step, ufo_pose *out);

#endif /* UFO_PATH_H */
