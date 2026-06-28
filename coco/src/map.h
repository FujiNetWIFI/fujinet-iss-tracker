/**
 * @brief   ISS Tracker
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Background (the world map)
 */

#ifndef MAP_H
#define MAP_H

/**
 * @brief Copy map onto pmode3 screen
 */
void map(void);

/**
 * @brief restore the map under the OSD region, erasing the fetching message (CoCo 1/2)
 */
void map_clear_osd(void);

#endif /* MAP_H */
