/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Minimal JSON value extraction
 *
 * FujiNet-side JSON queries are not available through fujinet-nio on the
 * Amiga, so the two small Open Notify responses are scanned here.  This is
 * not a general parser: keys are matched by name anywhere within the range.
 */

#ifndef JSON_H
#define JSON_H

/* Copy the value of "key" found in [start, end) into out (NUL terminated).
 * Strings are unquoted and unescaped; numbers and literals are copied as
 * text.  Returns 1 if found. */
int json_get(const char *start, const char *end, const char *key,
             char *out, int outlen);

/* Iterate the objects of the array named key.  Pass *cursor == 0 first;
 * each call sets [*obj, *obj_end) to the next object and returns 1, or
 * returns 0 when the array is exhausted. */
int json_next_object(const char *start, const char *end, const char *key,
                     const char **cursor, const char **obj,
                     const char **obj_end);

#endif /* JSON_H */
