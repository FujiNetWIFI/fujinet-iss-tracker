/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Minimal JSON value extraction
 */

#include <string.h>
#include "json.h"

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;
    return p;
}

/* Return the position just after the ':' following "key", or 0. */
static const char *find_key(const char *p, const char *end, const char *key)
{
    int klen = (int)strlen(key);

    while (p + klen + 2 <= end)
    {
        if (*p == '"' && p[klen + 1] == '"' && !memcmp(p + 1, key, klen))
        {
            const char *q = skip_ws(p + klen + 2, end);

            if (q < end && *q == ':')
                return skip_ws(q + 1, end);
        }
        p++;
    }
    return 0;
}

int json_get(const char *start, const char *end, const char *key,
             char *out, int outlen)
{
    const char *p = find_key(start, end, key);
    int n = 0;

    if (!p || p >= end || outlen < 1)
        return 0;

    if (*p == '"')
    {
        p++;
        while (p < end && *p != '"')
        {
            char c = *p++;

            if (c == '\\' && p < end)
            {
                c = *p++;
                if (c == 'u')
                {
                    /* no charset mapping on a stock Amiga font: skip hex */
                    int i;
                    for (i = 0; i < 4 && p < end; i++)
                        p++;
                    c = '?';
                }
                else if (c == 'n' || c == 't' || c == 'r')
                    c = ' ';
            }
            if (n < outlen - 1)
                out[n++] = c;
        }
    }
    else
    {
        while (p < end && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\r' && *p != '\n')
        {
            if (n < outlen - 1)
                out[n++] = *p;
            p++;
        }
    }
    out[n] = 0;
    return 1;
}

int json_next_object(const char *start, const char *end, const char *key,
                     const char **cursor, const char **obj,
                     const char **obj_end)
{
    const char *p = *cursor;
    int depth = 0;
    int in_str = 0;

    if (!p)
    {
        p = find_key(start, end, key);
        if (!p || p >= end || *p != '[')
            return 0;
        p++;
    }

    p = skip_ws(p, end);
    if (p < end && *p == ',')
        p = skip_ws(p + 1, end);
    if (p >= end || *p != '{')
        return 0;

    *obj = p;
    for (; p < end; p++)
    {
        if (in_str)
        {
            if (*p == '\\')
                p++;
            else if (*p == '"')
                in_str = 0;
        }
        else if (*p == '"')
            in_str = 1;
        else if (*p == '{')
            depth++;
        else if (*p == '}' && --depth == 0)
        {
            *obj_end = p + 1;
            *cursor = p + 1;
            return 1;
        }
    }
    return 0;
}
