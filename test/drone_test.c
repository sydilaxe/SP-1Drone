/* SP-1Drone: the engine on the host. Checks, and WAVs of what it plays in build/test.
 *   drone_test OUTDIR */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drone.h"
#include "sp1drone.h"


#define FS 48000
#define BLK 256
static int fails;
static const char *outdir = "build/test";

static void check(int ok, const char *what, const char *detail)
{
    printf("%s %s %s\n", what, detail, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
}

/* a WAV being written, 16-bit stereo */
typedef struct {
    FILE *f;
    uint32_t n;
} wav_t;
static void wr32(FILE *f, uint32_t x) { fputc(x & 255, f); fputc((x >> 8) & 255, f); fputc((x >> 16) & 255, f); fputc(x >> 24, f); }
static void wr16(FILE *f, uint32_t x) { fputc(x & 255, f); fputc((x >> 8) & 255, f); }
static void wav_open(wav_t *w, const char *name)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", outdir, name);
    w->f = fopen(p, "wb");
    w->n = 0;
    if (!w->f)
        return;
    fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, w->f);
    wr32(w->f, 16); wr16(w->f, 1); wr16(w->f, 2); wr32(w->f, FS); wr32(w->f, FS * 4); wr16(w->f, 4); wr16(w->f, 16);
    fwrite("data\0\0\0\0", 1, 8, w->f);
}
static void wav_put(wav_t *w, const float *l, const float *r, int n)
{
    int i;
    if (!w || !w->f)
        return;
    for (i = 0; i < n; i++) {
        float a = l[i] * 32767.0f, b = r[i] * 32767.0f;
        wr16(w->f, (uint16_t)(int16_t)(a > 32767 ? 32767 : a < -32768 ? -32768 : a));
        wr16(w->f, (uint16_t)(int16_t)(b > 32767 ? 32767 : b < -32768 ? -32768 : b));
    }
    w->n += (uint32_t)n;
}
static void wav_close(wav_t *w)
{
    if (!w->f)
        return;
    fseek(w->f, 4, SEEK_SET);
    wr32(w->f, 36 + w->n * 4);
    fseek(w->f, 40, SEEK_SET);
    wr32(w->f, w->n * 4);
    fclose(w->f);
}

/* what a stretch of rendering did */
typedef struct {
    double sum2;
    long n;
    float peak, maxstep;
    int bad;
} stat_t;
static float lastl, lastr;

static void run(double seconds, stat_t *s, wav_t *w)
{
    static float l[BLK], r[BLK];
    long k, blocks = (long)(seconds * FS / BLK);
    if (s)
        memset(s, 0, sizeof *s);
    for (k = 0; k < blocks; k++) {
        int i;
        sdb_apply();
        drone_render(l, r, BLK);
        for (i = 0; i < BLK; i++) {
            float a = fabsf(l[i]) > fabsf(r[i]) ? fabsf(l[i]) : fabsf(r[i]);
            float st = fabsf(l[i] - lastl) > fabsf(r[i] - lastr) ? fabsf(l[i] - lastl) : fabsf(r[i] - lastr);
            if (!isfinite(l[i]) || !isfinite(r[i]))
                s ? s->bad++ : 0;
            if (s) {
                s->sum2 += (double)l[i] * l[i] + (double)r[i] * r[i];
                s->n += 2;
                if (a > s->peak) s->peak = a;
                if (st > s->maxstep) s->maxstep = st;
            }
            lastl = l[i];
            lastr = r[i];
        }
        wav_put(w, l, r, BLK);
    }
}
static double rms_db(const stat_t *s) { return s->n ? 10.0 * log10(s->sum2 / (double)s->n + 1e-20) : -200.0; }

static void fresh(void)
{
    sdb_init();
    lastl = lastr = 0.0f;
}

int main(int argc, char **argv)
{
    stat_t s;
    wav_t w;
    char d[200];
    int i;
    if (argc > 1)
        outdir = argv[1];

    /* silence until PLAY */
    fresh();
    run(2.0, &s, 0);
    check(s.peak == 0.0f, "start: silent until PLAY", "");

    /* the default preset (MIST), PLAY: the four voices swell in; a tour of the controls */
    fresh();
    wav_open(&w, "session.wav");
    sdb_play();
    run(6.0, &s, &w);
    snprintf(d, sizeof d, "(%.1f dBFS rms, peak %.2f)", rms_db(&s), s.peak);
    check(s.bad == 0 && rms_db(&s) > -40.0 && s.peak < 1.0f, "play: MIST sounds, under full scale", d);
    sdb_chord(1);
    run(4.0, &s, &w);
    snprintf(d, sizeof d, "(largest step %.3f)", s.maxstep);
    check(s.maxstep < 0.08f, "chord change: the voices glide, no click", d);
    sdb_root(5);
    sdb_root(-3);
    sdb_set(SDB_SPARKLE, 0.7f);
    run(6.0, &s, &w);
    check(s.bad == 0 && s.peak < 1.0f, "sparkle: in range", "");
    for (i = 0; i < SDB_NSET; i++)       /* every fader to the top, then the bottom, across 4 s */
        sdb_set(i, 1.0f);
    run(4.0, &s, &w);
    snprintf(d, sizeof d, "(peak %.2f)", s.peak);
    check(s.bad == 0 && s.peak < 1.0f, "every fader up: in range", d);   /* (bright breath, driven: noise, so no
                                                                          * step check here) */
    sdb_preset(0);
    run(3.0, &s, &w);
    sdb_play();                          /* let go: it fades over RELEASE, then silence */
    run(25.0, &s, &w);
    run(5.0, &s, &w);
    snprintf(d, sizeof d, "(%.1f dBFS rms)", rms_db(&s));
    check(rms_db(&s) < -60.0, "play again: everything lets go and dies away", d);
    wav_close(&w);

    /* each track on its own */
    fresh();
    sdb_track(2);
    run(4.0, &s, 0);
    check(sdb_voice_on(2) && !sdb_voice_on(0) && drone_voice_level(2) > 0.5f && drone_voice_level(0) == 0.0f,
          "track 3: its voice alone", "");

    /* the shedding: at its deepest the partials fade down to three, smoothly, and come back */
    fresh();
    sdb_preset(15);                      /* BELLS: partials first */
    sdb_play();
    run(4.0, &s, 0);
    {
        double before = rms_db(&s);
        drone_shed(3);
        run(1.0, &s, 0);
        snprintf(d, sizeof d, "(largest step %.3f)", s.maxstep);
        check(s.bad == 0 && s.maxstep < 0.05f && drone_shed_level() == 3, "shedding: the partials fade, no click", d);
        run(2.0, &s, 0);
        snprintf(d, sizeof d, "(%.1f dBFS before, %.1f shed)", before, rms_db(&s));
        check(rms_db(&s) > before - 6.0, "shedding: the drone sounds on", d);
        drone_shed(0);
        run(1.0, &s, 0);
        check(s.bad == 0 && s.maxstep < 0.05f, "shedding: back, no click", "");
    }

    /* every preset: sounds, in range, no NaN */
    {
        int ok = 1;
        float worst = 0.0f;
        for (i = 0; i < SDB_NPRESET; i++) {
            char name[80];
            fresh();
            sdb_preset(i);
            sdb_play();
            snprintf(name, sizeof name, "preset-%02d-%s.wav", i + 1, sdb_preset_name(i));
            for (char *c = name; *c; c++)
                if (*c == ' ')
                    *c = '_';
            wav_open(&w, name);
            run(8.0, &s, &w);
            wav_close(&w);
            if (s.bad || s.peak >= 1.0f || rms_db(&s) < -45.0) {
                printf("  preset %s: rms %.1f peak %.2f bad %d\n", sdb_preset_name(i), rms_db(&s), s.peak, s.bad);
                ok = 0;
            }
            if (s.peak > worst)
                worst = s.peak;
        }
        snprintf(d, sizeof d, "(highest peak %.2f)", worst);
        check(ok, "presets: all 36 sound, in range", d);
    }

    /* the same output whatever the frames asked at a time */
    {
        static float a[1024 * 90], b[1024 * 90], tl[1024], tr[1024];
        int n = 1024 * 90, k;
        fresh();
        sdb_play();
        sdb_apply();
        for (k = 0; k < n; k += BLK) {
            drone_render(a + k, tl, BLK);
        }
        fresh();
        sdb_play();
        sdb_apply();
        for (k = 0; k < n; k += 1024) {
            drone_render(tl, tr, 1024);
            memcpy(b + k, tl, sizeof tl);
        }
        check(memcmp(a, b, sizeof a) == 0, "blocks of 256 and of 1024: same output", "");
    }

    printf("PASS: %d failed\n", fails);
    return fails != 0;
}
