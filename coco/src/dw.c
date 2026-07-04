/**
 * @brief DriveWire Functions
 */

#include "dw.h"

/**
 * @brief Read string to s from DriveWire with expected length l
 * @param s pointer to string buffer
 * @param l expected length of string (0-65535 bytes)
 * @return 1 = read successful, 0 = not successful
 */
byte dwread(byte *s, int l)
{
    asm
    {
        pshs x,y
            ldx :s
            ldy :l
#ifdef DRAGON
        jsr [0xF9FE]
#else        
        jsr [0xD93F]
#endif
            puls y,x
            tfr cc,b
            lsrb
            lsrb
            andb #$01
            }
}

/**
 * @brief Write string at s to DriveWire with length l
 * @param s pointer to string buffer
 * @param l length of string (0-65535 bytes)
 * @return error code
 */
byte dwwrite(byte *s, int l)
{
    asm
    {
        pshs x,y
            ldx :s
            ldy :l
#ifdef DRAGON            
            jsr [0xFA00]
#else
            jsr [0xD941]
#endif
            tfr cc,d
            puls y,x
            }
}
