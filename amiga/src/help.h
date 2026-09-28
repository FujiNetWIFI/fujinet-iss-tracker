/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Help window: the keys and what they do
 */

#ifndef HELP_H
#define HELP_H

/* Show the key help, with the current setting of each toggle, in a window
 * over the map until the user closes it. */
void help_show(int trail, int night, int circle, int sound);

#endif /* HELP_H */
