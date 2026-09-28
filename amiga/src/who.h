/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose "Who's in space" crew window
 */

#ifndef WHO_H
#define WHO_H

/* Fetch the crew list and show it in a window over the map until the
 * user closes it. Returns 0 (FN_OK) or the fetch error. */
unsigned char who_show(void);

#endif /* WHO_H */
