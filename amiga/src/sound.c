/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Paula sound effects through audio.device
 *
 * One channel is held from start to exit; each effect is one CMD_WRITE of
 * a sample generated into chip RAM at start-up (Kickstart 1.3 safe).
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
#define UFO_RATE   6000L           /* samples per second */
#define UFO_LEN    3000            /* half a second, looped seamlessly: */
#define UFO_HZ     700L            /* whole cycles of the tone, */
#define UFO_SWEEP  150L            /* one slow swoop up and down, */
#define UFO_WOBBLE 12L             /* three theremin wobbles */
#define UFO_LOOPS  20              /* longer than a whole sighting */
#define SAMPLE_BYTES (PING_LEN + QUINDAR_LEN + UFO_LEN)

static struct MsgPort *port;
static struct IOAudio *io;
static int dev_open;
static int pending;
static int enabled = 1;
static BYTE *ping;
static BYTE *quindar;
static BYTE *ufo;
static BYTE *playing;              /* sample of the last effect started */
static long clock_hz;
static UBYTE channels[] = { 1, 2, 4, 8 };

int sound_open(void)
{
    long i;

    clock_hz = (GfxBase->DisplayFlags & PAL) ? 3546895L : 3579545L;
    ping = AllocMem(SAMPLE_BYTES, MEMF_CHIP | MEMF_CLEAR);
    if (!ping)
        return 0;
    quindar = ping + PING_LEN;
    ufo = quindar + QUINDAR_LEN;

    for (i = 0; i < PING_TONE; i++)
    {
        long left = PING_TONE - i;
        long amp = 110L * left / PING_TONE * left / PING_TONE;
        long ang = i * PING_HZ * 36000L / PING_RATE;

        ping[i] = (BYTE)(amp * geo_sin(ang) / GEO_ONE);
    }
    for (i = 0; i < QUINDAR_LEN; i++)
        quindar[i] = (BYTE)(100L * geo_sin(i * 36000L / QUINDAR_LEN) / GEO_ONE);
    {
        /* Phase in 1/16ths of a hundredth of a degree, so the gliding
         * frequency adds up to whole cycles over the loop */
        long phase = 0;

        for (i = 0; i < UFO_LEN; i++)
        {
            long hz16 = UFO_HZ * 16 +
                UFO_SWEEP * 16 * geo_sin(i * 36000L / UFO_LEN) / GEO_ONE +
                UFO_WOBBLE * 16 * geo_sin(i * 3 * 36000L / UFO_LEN) / GEO_ONE;

            ufo[i] = (BYTE)(90L * geo_sin(phase / 16) / GEO_ONE);
            phase = (phase + hz16 * 36000L / UFO_RATE) % (36000L * 16);
        }
    }

    port = CreatePort(0, 0);
    if (!port)
        return 0;
    io = (struct IOAudio *)CreateExtIO(port, sizeof *io);
    if (!io)
        return 0;
    /* allocate any one channel while opening */
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
        FreeMem(ping, SAMPLE_BYTES);
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
    playing = data;
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

void sound_ufo(int on)
{
    if (!dev_open)
        return;
    if (!on)
    {
        if (playing == ufo)
            stop();
    }
    else if (!pending || CheckIO((struct IORequest *)io))
        play(ufo, UFO_LEN, UFO_RATE, UFO_LOOPS, 40);   /* idle: (re)start */
}
