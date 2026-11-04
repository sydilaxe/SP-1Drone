/* Cortex-M4 cycle estimate for the engine: built for the M4F as the firmware builds it, run in an emulator by
 * tools/bench_m4.py. BENCH_PRESET picks the drone preset (-1: every preset in turn is the caller's job); the four voices
 * on, the bass and SPARKLE up, every fader at the top: the most the engine does at once with that preset. */
#include <stddef.h>
#include "drone.h"
#include "sp1drone.h"

void *memset(void *d, int c, size_t n)
{
    unsigned char *p = d;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

static float L[256], R[256];

void bench_init(void)
{
    sdb_init();
    sdb_preset(BENCH_PRESET);
    for (int i = 0; i < SDB_NSET; i++)
        if (i != SDB_WARMTH)
            sdb_set(i, 1.0f);
    sdb_set(SDB_WARMTH, 0.6f);
    sdb_play();
    sdb_apply();
}

void bench_block(void)   /* one firmware block: 256 frames at 48 kHz */
{
    sdb_apply();
    drone_render(L, R, 256);
}
