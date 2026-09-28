/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Settings from icon ToolTypes or Shell arguments
 *
 * Both use KEY=value, keys in any case:
 *   HOMELAT=40.71   HOMELON=-74.01   home location, decimal degrees
 *   SAVER=10        minutes idle before the screen saver (0 = never)
 *   SOUND=OFF       start with sound effects off
 *   CIRCLE=OFF      start with the viewing circle hidden
 * A ToolType in brackets, e.g. (HOMELAT=40.71), is ignored as usual.
 */

#ifndef CONFIG_H
#define CONFIG_H

typedef struct
{
    int have_lat;
    int have_lon;
    long home_lat;       /* hundredths of a degree */
    long home_lon;
    int saver_minutes;
    int sound;
    int circle;
} config;

void config_defaults(config *c);

/* Apply one KEY=value setting. Returns 1 if it was recognised. */
int config_arg(config *c, const char *arg);

/* Both halves of a valid home location were given. */
int config_has_home(const config *c);

#endif /* CONFIG_H */
