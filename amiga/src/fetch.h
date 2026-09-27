/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose HTTP fetch through fujinet-nio
 */

#ifndef FETCH_H
#define FETCH_H

#define ISS_NOW_URL "http://api.open-notify.org/iss-now.json"
#define ASTROS_URL  "http://api.open-notify.org/astros.json"

typedef struct
{
    long lat_h;           /* hundredths of a degree */
    long lon_h;
    unsigned long ts;     /* Unix time */
} iss_pos;

/* GET url into buf (NUL terminated).  Returns an FN_* code; *len receives
 * the body length. */
unsigned char fetch_url(const char *url, char *buf, unsigned short max,
                        unsigned short *len);

/* Extra return codes beside the FN_* ones */
#define FETCH_ERR_NODEVICE 0xFD   /* fujinet-nio.device is not loaded */
#define FETCH_ERR_PARSE    0xFE   /* response was not understood */

/* Fetch and parse the current ISS position. Returns an FN_* or FETCH_ERR_*
 * code. */
unsigned char fetch_iss(iss_pos *pos);

/* Human readable text for a fetch_* return code. */
const char *fetch_error(unsigned char err);

/* Release the fujinet-nio transport, if it was opened. */
void fetch_shutdown(void);

#endif /* FETCH_H */
