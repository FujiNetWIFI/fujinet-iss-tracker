/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose HTTP fetch through fujinet-nio
 *
 * fujinet-nio-lib talks to the resident fujinet-nio.device broker, which
 * must already be loaded (see README.md). JSON is parsed on the Amiga.
 */

#include <stdlib.h>
#include <proto/dos.h>
#include "fujinet-nio.h"
#include "fetch.h"
#include "geo.h"
#include "json.h"

/* ~10 s of FN_ERR_NOT_READY/BUSY polling at 1/50 s per Delay(1) */
#define MAX_WAITS 500

static unsigned char transport_up;

unsigned char fetch_url(const char *url, char *buf, unsigned short max,
                        unsigned short *len)
{
    fn_handle_t h;
    unsigned short total = 0;
    unsigned short n;
    unsigned char flags;
    unsigned char err;
    int waits = 0;

    *len = 0;
    buf[0] = 0;

    if (!transport_up)
    {
        err = fn_init();
        if (err == FN_ERR_NOT_FOUND)
            return FETCH_ERR_NODEVICE;
        if (err != FN_OK)
            return err;
        transport_up = 1;
    }

    err = fn_open(&h, FN_METHOD_GET, url, 0);
    if (err != FN_OK)
        goto fail;

    while (total < max - 1)
    {
        n = 0;
        flags = 0;
        err = fn_read(h, total, (unsigned char *)buf + total,
                      max - 1 - total, &n, &flags);
        if (err == FN_ERR_NOT_READY || err == FN_ERR_BUSY)
        {
            if (++waits > MAX_WAITS)
            {
                err = FN_ERR_TIMEOUT;
                break;
            }
            Delay(1);
            continue;
        }
        if (err != FN_OK)
            break;
        total += n;
        if ((flags & FN_READ_EOF) || n == 0)
            break;
    }

    fn_close(h);
    buf[total] = 0;
    *len = total;
    if (err == FN_OK)
        return FN_OK;

fail:
    /* A transport failure leaves the session state unknown: start over
     * next time rather than reusing it. */
    if (err == FN_ERR_TRANSPORT || err == FN_ERR_IO)
        fetch_shutdown();
    return err;
}

unsigned char fetch_iss(iss_pos *pos)
{
    static char buf[512];
    char val[24];
    unsigned short len;
    const char *end;
    unsigned char err;
    iss_pos p;

    err = fetch_url(ISS_NOW_URL, buf, sizeof buf, &len);
    if (err != FN_OK)
        return err;

    end = buf + len;
    if (!json_get(buf, end, "latitude", val, sizeof val) ||
        !geo_parse_hundredths(val, &p.lat_h))
        return FETCH_ERR_PARSE;
    if (!json_get(buf, end, "longitude", val, sizeof val) ||
        !geo_parse_hundredths(val, &p.lon_h))
        return FETCH_ERR_PARSE;
    if (!json_get(buf, end, "timestamp", val, sizeof val))
        return FETCH_ERR_PARSE;
    p.ts = strtoul(val, 0, 10);
    if (p.lat_h < -9000 || p.lat_h > 9000 ||
        p.lon_h < -18000 || p.lon_h > 18000 || p.ts == 0)
        return FETCH_ERR_PARSE;

    *pos = p;
    return FN_OK;
}

const char *fetch_error(unsigned char err)
{
    switch (err)
    {
    case FN_OK:
        return "OK";
    case FETCH_ERR_PARSE:
        return "Unexpected server reply";
    case FETCH_ERR_NODEVICE:
        return "NIO driver not loaded";
    case FN_ERR_TRANSPORT:
        return "No reply from FujiNet";
    case FN_ERR_TIMEOUT:
        return "FujiNet timed out";
    case FN_ERR_NOT_FOUND:
        return "Server not found";
    case FN_ERR_IO:
        return "Network I/O error";
    default:
        return fn_error_string(err);
    }
}

void fetch_shutdown(void)
{
    if (transport_up)
    {
        fn_shutdown();
        transport_up = 0;
    }
}
