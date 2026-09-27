/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Day/night shading
 */

#ifndef NIGHT_H
#define NIGHT_H

/* Fill a MAP_W x MAP_H bitplane with the night mask for time ts: set bits
 * select the darker night twin (colour + 16) of each map pixel. A one
 * pixel checkerboard softens the terminator. */
void night_fill(unsigned char *plane, int bytes_per_row, unsigned long ts);

#endif /* NIGHT_H */
