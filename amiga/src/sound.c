/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Paula sound effects through audio.device
 *
 * Kickstart 1.3 compatible: one channel is allocated when the device is
 * opened and kept until exit; each effect is a single CMD_WRITE of a
 * sample generated into chip RAM at start-up.
 */

#include <exec/memory.h>
#include <devices/audio.h>
#include <graphics/gfxbase.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include "geo.h"
#include "sound.h"

extern struct GfxBase *GfxBase;

#define PING_RATE  16000L          /* samples per second */
#define PING_HZ    1200L
#define PING_TONE  2400            /* 150 ms of tone ... */
#define PING_LEN   4000            /* ... then 100 ms of silence */
#define QUINDAR_LEN 8              /* one cycle of sine */
#define QUINDAR_MS 250L

static struct MsgPort *port;
static struct IOAudio *io;
static int dev_open;
static int pending;
static int enabled = 1;
static BYTE *ping;
static BYTE *quindar;
static long clock_hz;
static UBYTE channels[] = { 1, 2, 4, 8 };

int sound_open(void)
{
    long i;

    clock_hz = (GfxBase->DisplayFlags & PAL) ? 3546895L : 3579545L;
    ping = AllocMem(PING_LEN + QUINDAR_LEN, MEMF_CHIP | MEMF_CLEAR);
    if (!ping)
        return 0;
    quindar = ping + PING_LEN;

    for (i = 0; i < PING_TONE; i++)
    {
        long left = PING_TONE - i;
        long amp = 110L * left / PING_TONE * left / PING_TONE;
        long ang = i * PING_HZ * 36000L / PING_RATE;

        ping[i] = (BYTE)(amp * geo_sin(ang) / GEO_ONE);
    }
    for (i = 0; i < QUINDAR_LEN; i++)
        quindar[i] = (BYTE)(100L * geo_sin(i * 36000L / QUINDAR_LEN) / GEO_ONE);

    port = CreatePort(0, 0);
    if (!port)
        return 0;
    io = (struct IOAudio *)CreateExtIO(port, sizeof *io);
    if (!io)
        return 0;
    /* Allocating a channel as part of OpenDevice: any one will do */
    io->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    io->ioa_Data = channels;
    io->ioa_Length = sizeof channels;
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)io, 0))
        return 0;
    dev_open = 1;
    return 1;
}

static void stop(void)
{
    if (pending)
    {
        if (!CheckIO((struct IORequest *)io))
            AbortIO((struct IORequest *)io);
        WaitIO((struct IORequest *)io);
        pending = 0;
    }
}

void sound_close(void)
{
    if (dev_open)
    {
        stop();
        CloseDevice((struct IORequest *)io);
        dev_open = 0;
    }
    if (io)
    {
        DeleteExtIO((struct IORequest *)io);
        io = 0;
    }
    if (port)
    {
        DeletePort(port);
        port = 0;
    }
    if (ping)
    {
        FreeMem(ping, PING_LEN + QUINDAR_LEN);
        ping = 0;
    }
}

void sound_enable(int on)
{
    enabled = on;
    if (!on && dev_open)
        stop();
}

static void play(BYTE *data, long len, long rate, int cycles, int volume)
{
    if (!dev_open || !enabled)
        return;
    stop();
    io->ioa_Request.io_Command = CMD_WRITE;
    io->ioa_Request.io_Flags = ADIOF_PERVOL;
    io->ioa_Data = (UBYTE *)data;
    io->ioa_Length = len;
    io->ioa_Period = (UWORD)(clock_hz / rate);
    io->ioa_Volume = volume;
    io->ioa_Cycles = cycles;
    BeginIO((struct IORequest *)io);
    pending = 1;
}

void sound_ping(void)
{
    play(ping, PING_LEN, PING_RATE, 1, 48);
}

void sound_alert(void)
{
    play(ping, PING_LEN, PING_RATE, 3, 64);
}

void sound_quindar(int start)
{
    long hz = start ? 2525L : 2475L;

    play(quindar, QUINDAR_LEN, hz * QUINDAR_LEN,
         (int)(hz * QUINDAR_MS / 1000L), 32);
}
