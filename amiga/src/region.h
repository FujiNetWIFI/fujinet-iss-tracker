/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose "What's below": country or ocean at a position
 */

#ifndef REGION_LOOKUP_H
#define REGION_LOOKUP_H

/* Name of the country or ocean at (lat, lon) in hundredths of a degree. */
const char *region_name(long lat_h, long lon_h);

#endif /* REGION_LOOKUP_H */
