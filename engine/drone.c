/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil (FRASIL's engine/dsp/frasil.c and glint.c), changed for SP-1Drone (2026) */
/* SP-1Drone's sound (drone.h): FRASIL's drone voices, its GLINT and its mix, made to fit the SP-1.
 *
 * A block (64 frames at 24 kHz, the firmware's 128 at 48 kHz): the queued notes; the drift (an Ornstein-Uhlenbeck
 * walk per thing that drifts, a step a block) and the tide; each voice's frequencies, partial amplitudes and filter
 * coefficients for the block; then each voice in chunks of 32 frames (the envelope ramps across a chunk), its layers
 * summed into three buses (the centre and two sides) and placed in the field; then DC out, WARMTH, the glints, the
 * bass, the soft limit, and 2x up to 48 kHz.
 *
 * CHOIR    three PolyBLEP saws, centre (at INTERVAL), -detune left, +detune right, each drifting a few cents, each
 *          minus SHAPE's share of itself a width on (a square at half a turn, then narrower pulses), the width
 *          swaying on its own; two one-poles per side, the cutoff in harmonics of the note; with RESONANCE or TYPE
 *          a state-variable filter (TPT): low, band or high; SOFT integrates the wave toward a sine
 * PARTIALS 6 partials as rotating phasors (a "magic circle"), the fundamental in the centre, the even ones to one
 *          side and the odd ones to the other; each one's amplitude and tuning drift on their own (EVOLVE deeper);
 *          BEAT pulses each at its own pace; a formant (FORMANT, PEAK) lifts the ones near it
 * BREATH   noise (a hiss, or GRAIN's sparse grains; TONE darker or brighter) through three resonators at the note
 *          times ratios (COLOR) times PITCH, Q 2 (air) .. 600 (glass), levelled; AIR, the noise itself beside them
 *
 * Changed from FRASIL: a voice per TRACK (4), its note gliding when the chord or the root changes (no steal, no
 * shedding, no velocity or pressure); the rate 24 kHz (dsp.h), the steps FRASIL made for 44.1 kHz made over for it
 * and the highest frequencies kept under 10.5 kHz; GLINT reduced to three voices of its ICE preset, struck by SPARKLE
 * here in the render; a simple BASS (a sine and its octave, gliding) in place of FRASIL's eight models; no MASTER
 * FILTER, LOFI TAPE, EQ, PUMP or MASTERING, and no CLOUD: the drone comes out dry, for effects of its own. The voices' DSP (voice_block, the chunks) is FRASIL's. */
#include <stddef.h>
#include "dsp.h"
#include "drone.h"

#define NV DRONE_NV
#define NP 6                             /* (FRASIL's 8; 6 is its own first step when its CPU runs short) */
#define NR 3
#define CH 32u                           /* a chunk (24 kHz) */
#define BLK 128u                         /* a block (24 kHz): DRONE_OUT_BLOCK at 48 kHz (5.3 ms, as FRASIL's 256 at 44.1) */
#define VOICE_G 0.2f
#define TAIL_END 0.01f                   /* a voice let go ends here: its gain (env squared) 80 dB down */
/* FRASIL's steps a chunk (64 frames) and a block (256 frames) at 44.1 kHz, in this engine's chunks and blocks */
#define R_CHUNK ((float)CH / 24000.0f / (64.0f / 44100.0f))
#define R_BLOCK ((float)BLK / 24000.0f / (256.0f / 44100.0f))
static float k_at(float k, float r) { return 1.0f - fm_powf(1.0f - k, r); }

/* ---------------------------------------------------------------- events --- */
enum { EV_ON = 1, EV_OFF, EV_BON, EV_BOFF, EV_PANIC };
typedef struct {
    uint8_t type;
    int8_t v;
    int8_t note;
} ev_t;
#define NEV 64u
static ev_t evq[NEV];
static volatile uint32_t ev_w, ev_r;

static void ev_push(uint8_t type, int v, int note)
{
    uint32_t w = ev_w;
    if (w - ev_r >= NEV)
        return;                          /* full: dropped (the render takes them every block) */
    evq[w & (NEV - 1u)].type = type;
    evq[w & (NEV - 1u)].v = (int8_t)v;
    evq[w & (NEV - 1u)].note = (int8_t)note;
    __asm__ volatile("" ::: "memory");   /* the slot before the index */
    ev_w = w + 1u;
}

void drone_voice_on(int v, int note)
{
    if (v >= 0 && v < NV)
        ev_push(EV_ON, v, note);
}
void drone_voice_off(int v)
{
    if (v >= 0 && v < NV)
        ev_push(EV_OFF, v, 0);
}
void drone_bass_on(int note) { ev_push(EV_BON, 0, note); }
void drone_bass_off(void) { ev_push(EV_BOFF, 0, 0); }
void drone_panic(void) { ev_push(EV_PANIC, 0, 0); }

/* ---------------------------------------------------------------- voices --- */
typedef struct {
    int8_t note;                         /* -1: silent */
    uint8_t gate;
    float pitch;                         /* the note sounding (it glides to note) */
    float env;                           /* 0..1; the amplitude is its square */
    float lvl;                           /* the gain at the end of the last chunk (the next ramps from it) */
    float pan;                           /* -1..1 */
    /* drift, each about -1..1 */
    float ou_c[3], ou_cut, ou_pa[NP], ou_pf[NP], ou_r[NR], ou_pan;
    /* CHOIR */
    float cph[3], cdt[3], cid[3];        /* phases, increments, the increments' inverses */
    float cw[3];                         /* SHAPE: how far on the saw taken off it is (the pulse's width) */
    float cpw[3];                        /* the widths' sway, a turn each */
    float csh;                           /* SHAPE: how much of the saw is taken off, 0..1 */
    float sc0, sc1, sc2;                 /* the state-variable's output mix: its input, band and low parts */
    float sp0, sp1, sp2;                 /* the ones before (a chunk's glide to sc) */
    float clp[4];                        /* left two stages, right two */
    float ca;                            /* one-pole coefficient */
    float sa1, sa2, sa3;                 /* the state-variable's */
    float cgain;                         /* SHAPE's and RESONANCE's level made up */
    float cint[6];                       /* SOFT: the integrators, the centre's, the left's, the right's */
    uint8_t csvf;                        /* the filter running: the state-variable (1) or the one-poles */
    /* PARTIALS */
    float px[NP], py[NP], pa[NP];        /* each partial: its oscillator, its level */
    float ps[NP], pc[NP];                /* its half step's sine, its cosine's inverse */
    float pac[NP], pda[NP];              /* BEAT: the level now and its step a sample */
    float pbt[NP];                       /* BEAT: each one's pulse, a turn */
    int np;
    uint32_t pmax;                       /* the shedding's: how many may sound, the top one's share */
    float ptop;
    /* BREATH */
    float r1[NR], r2[NR];                /* two-pole resonators: states, coefficients, gains */
    float rb1[NR], rb2[NR], rg[NR];
    float gl, gr;                        /* its place: left and right gains */
} voice_t;

static voice_t vo[NV];
static uint32_t rng = 0x2545F491u;

static drone_params_t P, T, pend[2];     /* the render's (gliding to T), the target, what the main loop set */
#define NGLIDE (offsetof(drone_params_t, ch_type) / sizeof(float))
static volatile uint32_t pend_seq, pend_ix;
static uint32_t pend_seen;

static volatile int shed;                /* drone_shed: 0 .. 3 */
static float np_cap = (float)NP;         /* the partials the shedding allows, gliding */
static const uint8_t SHED_NP[4] = {NP, 5, 4, 3};
static float tide_ph, tide_v;
static float nlp;                        /* TONE's filter on the noise */
static float dcl, dcr;
static float noise[BLK];
static float mixl[BLK], mixr[BLK];
static float spl[BLK], spr[BLK];
static float bss[BLK];
static float gain_now;
static volatile float lvl_view[NV], glint_view, peak_view;
static float peak;

static const float LOG2K[NP] = {0.0f, 1.0f, 1.5849625f, 2.0f, 2.3219281f, 2.5849625f};
static const float RATIOS[3][NR] = {{1.0f, 2.0f, 3.0f}, {1.0f, 3.0f, 5.0f}, {1.0f, 2.756f, 5.404f}};
static float RATIO_L2[3][NR];

static void voice_start(voice_t *v, int note)
{
    uint32_t k;
    v->note = (int8_t)note;
    v->gate = 1;
    v->pitch = (float)note;
    v->env = 0.0f;
    v->lvl = 0.0f;
    /* a place in the field from the note, so a chord opens out (the drift moves it) */
    v->pan = (float)(((note * 7) % 12) - 6) * (1.0f / 6.0f);
    for (k = 0; k < 3; k++) {
        v->cph[k] = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
        v->cpw[k] = (float)k * 0.33f;
        v->ou_c[k] = br_randf(&rng) * 0.5f;
    }
    for (k = 0; k < 4; k++)
        v->clp[k] = 0.0f;
    for (k = 0; k < 6; k++)
        v->cint[k] = 0.0f;
    v->csvf = 0;
    v->sp0 = v->sp1 = v->sp2 = 0.0f;
    for (k = 0; k < NP; k++) {           /* random phases: no peak where they all start together */
        float ph = (float)(br_rand(&rng) >> 8) * (6.2831853f / 16777216.0f);
        v->px[k] = fm_cosf(ph);
        v->py[k] = fm_sinf(ph);
        v->ps[k] = 0.0f;
        v->pc[k] = 1.0f;
        v->ou_pa[k] = br_randf(&rng) * 0.5f;
        v->ou_pf[k] = br_randf(&rng) * 0.5f;
        v->pac[k] = v->pda[k] = 0.0f;
        v->pbt[k] = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
    }
    for (k = 0; k < NR; k++) {
        v->r1[k] = v->r2[k] = 0.0f;
        v->ou_r[k] = br_randf(&rng) * 0.5f;
    }
    v->ou_cut = br_randf(&rng) * 0.5f;
    v->ou_pan = 0.0f;
}

/* ----------------------------------------------------------------- GLINT --- */
/* FRASIL's GLINT, its ICE preset (a high bell: RATIO 3.5, BRIGHT 70, DECAY 70, PING 68, SPACE 90), three at once,
 * struck by SPARKLE on the notes the drones hold, an octave up (one in three two) */
#define GNV 3
#define TBITS 10u
#define TN (1u << TBITS)
#define IDX_UNIT 2670176.8f              /* the index in 2^-24 turns a radian */
static float sintab[TN + 1u];
typedef struct {
    int8_t on;
    uint32_t pc, pm, dc, dm;
    float amp, ka, idx, idx_end, ki, att, katt, gl, gr;
} glint_t;
static glint_t gv[GNV];
static uint32_t g_rng = 0x6C8E9CF5u;
static float spk_wait = -1.0f;           /* SPARKLE: seconds to the next strike (-1: not armed) */

static inline float tsin(uint32_t ph)
{
    uint32_t i = ph >> (32u - TBITS);
    float f = (float)(ph & ((1u << (32u - TBITS)) - 1u)) * (1.0f / (float)(1u << (32u - TBITS)));
    return sintab[i] + (sintab[i + 1u] - sintab[i]) * f;
}
static float decay_k(float seconds) { return fm_exp2f(-9.965784f / (seconds * BR_FS)); }   /* -60 dB over it */

static void glint_strike(int note, float vel)
{
    glint_t *s = 0;
    uint32_t i;
    float f, pan, det, base;
    for (i = 0; i < GNV && !s; i++)
        if (!gv[i].on)
            s = &gv[i];
    if (!s)                              /* none free: the quietest */
        for (i = 0; i < GNV; i++)
            if (!s || gv[i].amp < s->amp)
                s = &gv[i];
    det = br_randf(&g_rng) * 0.63f * 8.0f;   /* cents */
    pan = br_randf(&g_rng) * 0.63f;
    f = 440.0f * fm_exp2f(((float)note - 69.0f + det * 0.01f) * (1.0f / 12.0f));
    s->on = 1;
    s->dc = (uint32_t)(int32_t)(br_clampf(f * BR_INV_FS, 0.0f, 0.45f) * 4294967296.0f);
    s->dm = (uint32_t)(int32_t)(br_clampf(f * 3.5f * BR_INV_FS, 0.0f, 0.45f) * 4294967296.0f);
    s->pc = s->pm = 0;
    s->amp = (0.35f + 0.65f * vel) * 0.3f;
    s->ka = decay_k(0.86f);
    base = 0.30f * 5.0f * IDX_UNIT;      /* BRIGHT 70: 0.55^2 x 5 */
    s->idx = base * (1.0f + 1.5f * 0.535f);
    s->idx_end = base * (1.0f - 0.85f * 0.535f);
    s->ki = decay_k(0.03f + 0.86f * 0.2f * (1.0f - 0.5f * 0.535f));
    s->att = 0.0f;
    s->katt = 1.0f - fm_expf(-3.0f / (0.002f * BR_FS));
    s->gl = fm_sqrtf(0.5f * (1.0f - pan));
    s->gr = fm_sqrtf(0.5f * (1.0f + pan));
}

/* SPARKLE (FRASIL's core.c sparkle_run, here a block at a time): as often as it is turned (a few a minute .. six a
 * second, at random around that), on a note a drone holds */
static void sparkle(void)
{
    float d = P.sparkle, mean;
    uint32_t i, cnt = 0, pick;
    int note = -1;
    if (d <= 0.004f) {
        spk_wait = -1.0f;
        return;
    }
    mean = 1.0f / (6.0f * d * d + 0.02f);
    if (spk_wait < 0.0f || spk_wait > mean * 1.7f) {   /* just on, or turned up: the next one from now */
        spk_wait = mean * (0.3f + 1.4f * (float)(br_rand(&g_rng) & 1023u) * (1.0f / 1023.0f));
        return;
    }
    spk_wait -= (float)BLK * BR_INV_FS;
    if (spk_wait > 0.0f)
        return;
    spk_wait = mean * (0.3f + 1.4f * (float)(br_rand(&g_rng) & 1023u) * (1.0f / 1023.0f));
    for (i = 0; i < NV; i++)
        cnt += vo[i].note >= 0 && vo[i].gate;
    if (!cnt)
        return;
    pick = br_rand(&g_rng) % cnt;
    for (i = 0; i < NV; i++)
        if (vo[i].note >= 0 && vo[i].gate && pick-- == 0)
            note = vo[i].note;
    note += 12 + (br_rand(&g_rng) % 3u == 0 ? 12 : 0);
    while (note > 84)                    /* (the bell's sidebands under the 24 kHz rate's Nyquist) */
        note -= 12;
    glint_strike(note, 0.22f + 0.4f * (float)(br_rand(&g_rng) & 255u) * (1.0f / 255.0f));
}

static void glint_render(uint32_t n)
{
    uint32_t i, j;
    const float lv = 90.0f / 127.0f;
    float top = 0.0f;
    for (j = 0; j < GNV; j++) {
        glint_t *s = &gv[j];
        uint32_t pc = s->pc, pm = s->pm, dc = s->dc, dm = s->dm;
        float amp = s->amp, att = s->att, katt = s->katt, idx = s->idx, ie = s->idx_end, k = s->ka, ki = s->ki;
        float gl = s->gl * lv, gr = s->gr * lv;
        if (!s->on)
            continue;
        for (i = 0; i < n; i++) {
            float m = tsin(pm), y;
            pm += dm;
            y = tsin(pc + ((uint32_t)(int32_t)(idx * m) << 8)) * amp * att;
            pc += dc;
            att += (1.0f - att) * katt;
            amp *= k;
            idx = ie + (idx - ie) * ki;
            spl[i] += y * gl;
            spr[i] += y * gr;
        }
        s->pc = pc;
        s->pm = pm;
        s->amp = amp;
        s->att = att;
        s->idx = idx;
        if (amp < 1e-5f)
            s->on = 0;
        if (amp * (1.0f / 0.3f) > top)
            top = amp * (1.0f / 0.3f);
    }
    glint_view = top;
}

/* ------------------------------------------------------------------ BASS --- */
/* one deep note: a sine and a little of its octave (heard on a small speaker), gliding from note to note, a slow
 * swell in and a long fade out */
static struct {
    int8_t note;                         /* -1: let go */
    float pitch, ph, env;
} bs = {-1, 36.0f, 0.0f, 0.0f};

static void bass_render(uint32_t n)
{
    float tgt = bs.note >= 0 ? 1.0f : 0.0f, ka = 1.0f - fm_expf(-3.0f / (0.6f * BR_FS)), kr = 1.0f - fm_expf(-3.0f / (2.5f * BR_FS));
    float f, inc, e = bs.env, ph = bs.ph, g = P.bass * 0.55f;
    uint32_t i;
    if (bs.note >= 0)
        bs.pitch += ((float)bs.note - bs.pitch) * k_at(0.12f, R_BLOCK);   /* the glide: some 100 ms */
    f = 440.0f * fm_exp2f((bs.pitch - 69.0f) * (1.0f / 12.0f));
    inc = f * BR_INV_FS;
    if (e < 1e-5f && tgt == 0.0f) {
        bs.env = 0.0f;
        for (i = 0; i < n; i++)
            bss[i] = 0.0f;
        return;
    }
    for (i = 0; i < n; i++) {
        float s;
        e += (tgt - e) * (tgt > e ? ka : kr);
        ph += inc;
        ph -= ph >= 1.0f ? 1.0f : 0.0f;
        s = fm_sin_turns(ph);
        bss[i] = br_soft((s + 0.3f * (2.0f * s * s - 1.0f) * 0.5f) * 1.1f) * e * g;   /* (cos 2x: its octave) */
    }
    bs.env = e;
    bs.ph = ph;
}

static void events(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r & (NEV - 1u)];
        __asm__ volatile("" ::: "memory");
        ev_r = ev_r + 1u;
        switch (e.type) {
        case EV_ON: {
            voice_t *v = &vo[(int)e.v];
            if (v->note >= 0) {          /* sounding: it glides to the new note, its phases go on (no click) */
                v->note = e.note;
                v->gate = 1;
            } else {
                voice_start(v, e.note);
            }
            break;
        }
        case EV_OFF:
            vo[(int)e.v].gate = 0;
            break;
        case EV_BON:
            if (bs.note < 0 && bs.env < 0.01f)
                bs.pitch = (float)e.note;
            bs.note = e.note;
            break;
        case EV_BOFF:
            bs.note = -1;
            break;
        case EV_PANIC: {
            uint32_t i;
            for (i = 0; i < NV; i++)
                vo[i].note = -1;
            for (i = 0; i < GNV; i++)
                gv[i].on = 0;
            bs.note = -1;
            bs.env = 0.0f;
            break;
        }
        }
    }
}

/* ----------------------------------------------------------------- drift --- */
/* one Ornstein-Uhlenbeck step: back towards 0 by th, a kick of s (unit variance: three uniforms) */
static inline float ou(float x, float th, float s)
{
    float r = br_randf(&rng) + br_randf(&rng) + br_randf(&rng);
    return br_clampf(x - th * x + s * r, -1.5f, 1.5f);
}

static void drift(uint32_t n)
{
    float tau = 20.0f * fm_powf(0.025f, P.motion_rate);   /* 20 s .. 0.5 s */
    float th = (float)n * BR_INV_FS / tau, s = 0.6f * fm_sqrtf(2.0f * th);
    float te = th * (1.0f + 3.0f * P.pa_evolve), se = 0.6f * fm_sqrtf(2.0f * te);
    uint32_t i, k;
    for (i = 0; i < NV; i++) {
        voice_t *v = &vo[i];
        if (v->note < 0)
            continue;
        for (k = 0; k < 3; k++)
            v->ou_c[k] = ou(v->ou_c[k], th, s);
        for (k = 0; k < NP; k++) {
            v->ou_pa[k] = ou(v->ou_pa[k], te, se);
            v->ou_pf[k] = ou(v->ou_pf[k], th, s);
        }
        for (k = 0; k < NR; k++)
            v->ou_r[k] = ou(v->ou_r[k], th, s);
        v->ou_cut = ou(v->ou_cut, th, s);
        v->ou_pan = ou(v->ou_pan, th * 0.5f, s * 0.7f);
    }
    /* the tide: one turn in 5 min .. 10 s */
    tide_ph += (float)n * BR_INV_FS / (300.0f * fm_powf(1.0f / 30.0f, P.tide_rate));
    if (tide_ph >= 1.0f)
        tide_ph -= 1.0f;
    tide_v = fm_sinf(6.2831853f * tide_ph);
}

/* -------------------------------------------------- a voice's block setup --- */
static void voice_block(voice_t *v)
{
    float f, m = P.motion;
    float mw = m * (1.0f + 2.0f * m);    /* MOTION's wander: wider toward the top */
    float bright = (P.timbre - 0.5f) + tide_v * P.tide * 0.5f;
    float sum2 = 0.0f, norm, pe, pw, sx, ce, hc, fc, pan, col, q, sb;
    const float *ra, *rb;
    uint32_t j;
    int np;

    v->pitch += ((float)v->note - v->pitch) * k_at(0.1f, R_BLOCK);   /* a new note: a glide of some 150 ms */
    if (v->pitch - (float)v->note < 0.002f && v->pitch - (float)v->note > -0.002f)
        v->pitch = (float)v->note;
    f = 440.0f * fm_exp2f((v->pitch - 69.0f) * (1.0f / 12.0f));

    /* CHOIR: the saws' tunings, SHAPE's widths, the low-pass, RESONANCE's state-variable */
    if (P.choir > 0.001f) {
        static const float SIDE[3] = {0.0f, -1.0f, 1.0f}, PWR[3] = {0.21f, 0.29f, 0.37f};   /* the sways, Hz */
        float dc = P.detune * P.detune * 40.0f, sh = P.ch_shape;
        float a = sh < 0.5f ? 2.0f * sh : 1.0f, w = sh < 0.5f ? 0.5f : 0.5f - 0.8f * (sh - 0.5f);   /* 0.5 .. 0.1 */
        float sw = sh > 0.5f ? 0.03f + 0.18f * (sh - 0.5f) : 0.03f * a, pv;
        for (j = 0; j < 3; j++) {
            v->cdt[j] = f * (j ? 1.0f : P.ch_interval) *
                        fm_exp2f((SIDE[j] * dc + mw * 7.0f * v->ou_c[j]) * (1.0f / 1200.0f)) * BR_INV_FS;
            if (v->cdt[j] > 0.45f)
                v->cdt[j] = 0.45f;
            v->cid[j] = 1.0f / v->cdt[j];
            v->cpw[j] += PWR[j] * (float)BLK * BR_INV_FS;
            v->cpw[j] -= (float)(int32_t)v->cpw[j];
            v->cw[j] = br_clampf(w + sw * fm_sinf(6.2831853f * v->cpw[j]) + 0.1f * mw * v->ou_c[j] * a, 0.04f, 0.96f);
        }
        v->csh = a;
        ce = br_clampf(P.cutoff + bright * 0.6f + mw * 0.2f * v->ou_cut, 0.0f, 1.0f);
        hc = 1.0f + 39.0f * ce * ce;
        fc = br_clampf(f * hc, 20.0f, BR_NYQ_SAFE);
        v->ca = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
        pv = (1.0f + a * a) * (1.0f / 3.0f) - 2.0f * a * (1.0f / 3.0f - 2.0f * w * (1.0f - w));
        v->cgain = fm_sqrtf((1.0f / 3.0f) / (pv > 0.05f ? pv : 0.05f));
        if (P.ch_reso > 0.002f || T.ch_type) {
            float g = fm_tanf(3.14159265f * fc * BR_INV_FS), q = 0.5f * fm_powf(40.0f, P.ch_reso), k = 1.0f / q;
            v->sa1 = 1.0f / (1.0f + g * (g + k));
            v->sa2 = g * v->sa1;
            v->sa3 = g * v->sa2;
            if (T.ch_type == 1) {        /* band: its peak at 1, made up for being narrow */
                v->sc0 = 0.0f;
                v->sc1 = k * (1.4f + 0.12f * fm_sqrtf(hc)) * fm_sqrtf(q);
                v->sc2 = 0.0f;
            } else if (T.ch_type == 2) { /* high: x - k band - low, made up for the low harmonics gone */
                float mk = br_clampf(fm_sqrtf(0.75f * hc), 1.0f, 4.0f);
                v->sc0 = mk;
                v->sc1 = -k * mk;
                v->sc2 = -mk;
            } else {
                v->sc0 = v->sc1 = 0.0f;
                v->sc2 = 1.0f;
            }
            v->cgain *= 1.0f / fm_sqrtf(1.0f + 0.22f * (q - 0.5f));
        }
    }

    /* PARTIALS: frequencies (stretched, drifting), amplitudes (tilt, odd, drift), levelled */
    if (P.partials > 0.001f) {
        pe = br_clampf(P.tilt - bright * 0.7f, 0.0f, 1.0f);
        pw = 0.3f + 2.7f * pe;
        sx = 1.0f + 0.12f * P.stretch;
        np = 0;
        {                                /* the shedding's cap (FRASIL's): the top partial fading out and in as the cap
                                          * glides, no step */
            float top;
            uint32_t nmax = (uint32_t)np_cap;
            top = np_cap - (float)nmax;
            if (top > 0.001f)
                nmax++;
            else
                top = 1.0f;
            v->ptop = top;
            v->pmax = nmax;
        }
        for (j = 0; j < v->pmax; j++) {
            float fk = f * fm_exp2f(sx * LOG2K[j] + mw * 5.0f * v->ou_pf[j] * (1.0f / 1200.0f)), wk, a;
            if (fk > BR_NYQ_SAFE)
                break;
            wk = 6.2831853f * fk * BR_INV_FS;
            {                            /* the oscillator back to its phasor, on the circle, then on with this block's
                                          * step (FRASIL's magic circle, its pitch exact) */
                float c = fm_cosf(wk), sn = fm_sinf(wk), r = fm_sqrtf(c * c + sn * sn), qq = fm_sqrtf(2.0f * r * (r + c));
                float x = v->px[j], y = (v->py[j] - x * v->ps[j]) * v->pc[j], gn = 1.5f - 0.5f * (x * x + y * y);
                float sh = sn / qq, chh = (r + c) / qq;
                x *= gn;
                y *= gn;
                v->ps[j] = sh;
                v->pc[j] = qq / (r + c);
                v->px[j] = x;
                v->py[j] = y * chh + x * sh;
            }
            a = fm_exp2f(-pw * LOG2K[j] + 1.6f * (m + 2.5f * P.pa_evolve) * v->ou_pa[j]);
            if (j & 1u)                  /* the even harmonics (2, 4 ...) */
                a *= 1.0f - P.odd;
            if (P.pa_peak > 0.001f) {    /* the formant */
                float pk = P.pa_peak, d = (sx * LOG2K[j] - P.pa_formant) * (1.5f + 4.0f * pk), fl = 1.0f - 0.97f * pk;
                a *= fl + (1.0f + 7.0f * pk - fl) / (1.0f + d * d);
            }
            if (j + 1u == v->pmax)
                a *= v->ptop;
            v->pa[j] = a;
            sum2 += a * a;
            np++;
        }
        v->np = np;
        norm = 0.6f / fm_sqrtf(sum2 + 1e-9f);
        for (j = 0; j < (uint32_t)np; j++)
            v->pa[j] *= norm;
        if (P.pa_beat > 0.001f) {        /* BEAT: each one pulsing at its own pace (0.15 .. 7 Hz) */
            static const float SPREAD_[NP] = {0.55f, 1.25f, 0.8f, 1.45f, 0.65f, 1.1f};
            float bt = P.pa_beat, dep = bt * 1.4f > 1.0f ? 1.0f : bt * 1.4f, hz = 0.15f + 5.0f * bt * bt;
            for (j = 0; j < (uint32_t)np; j++) {
                float gb;
                v->pbt[j] += hz * SPREAD_[j] * (float)BLK * BR_INV_FS;
                v->pbt[j] -= (float)(int32_t)v->pbt[j];
                gb = (1.0f - dep * (0.5f + 0.5f * fm_sinf(6.2831853f * v->pbt[j]))) / (1.0f - 0.5f * dep);
                v->pa[j] *= gb;
                v->pda[j] = (v->pa[j] - v->pac[j]) * (1.0f / (float)BLK);
            }
        } else {
            for (j = 0; j < (uint32_t)np; j++) {
                v->pac[j] = v->pa[j];
                v->pda[j] = 0.0f;
            }
        }
    }

    /* BREATH: the resonators at the note x ratios, their bandwidth from Q, each one's gain from its noise gain */
    if (P.souffle > 0.001f) {
        col = br_clampf(P.color, 0.0f, 1.0f) * 2.0f;
        ra = RATIO_L2[col < 1.0f ? 0 : 1];
        rb = RATIO_L2[col < 1.0f ? 1 : 2];
        col -= col < 1.0f ? 0.0f : 1.0f;
        q = 2.0f * fm_powf(300.0f, br_clampf(P.resonance + mw * 0.1f * v->ou_r[0], 0.0f, 1.0f));
        sb = br_clampf(0.55f + bright * 0.8f, 0.05f, 1.0f);
        for (j = 0; j < NR; j++) {
            float fr = f * P.br_pitch * fm_exp2f(br_lerp(ra[j], rb[j], col) + mw * 12.0f * v->ou_r[j] * (1.0f / 1200.0f));
            float r, b1, b2, ng;
            fr = br_clampf(fr, 20.0f, BR_NYQ_SAFE);
            r = fm_expf(-3.14159265f * fr / q * BR_INV_FS);
            b1 = 2.0f * r * fm_cosf(6.2831853f * fr * BR_INV_FS);
            b2 = r * r;
            ng = (1.0f + b2) / ((1.0f - b2) * ((1.0f + b2) * (1.0f + b2) - b1 * b1));   /* the sum of h^2 */
            v->rb1[j] = b1;
            v->rb2[j] = b2;
            v->rg[j] = 0.4f * fm_sqrtf(3.0f / ng) * (j ? sb * (j == 2u ? sb : 1.0f) : 1.0f);
        }
    }

    pan = br_clampf((v->pan + mw * 0.4f * v->ou_pan) * P.spread, -1.0f, 1.0f);
    v->gl = fm_sqrtf(0.5f * (1.0f - pan));
    v->gr = fm_sqrtf(0.5f * (1.0f + pan));
}

/* ---------------------------------------------------- a voice's chunk --- */
#define CH_EPS 1e-5f
static inline uint32_t ch_run(float p, float d, float id, uint32_t m)
{
    float r = (1.0f - CH_EPS - d - p) * id;
    return r <= 0.0f ? 0u : r < (float)m ? (uint32_t)r : m;
}

static float ch_x0[CH], ch_x1[CH], ch_xc[CH];

static __attribute__((noinline)) void ch_svf(float *b, const float *x, const float *c, float *s, const float *o,
                                             const float *e)
{
    float a1 = c[0], a2 = c[1], a3 = c[2], s1 = s[0], s2 = s[1], o0 = o[0], o1 = o[1], o2 = o[2];
    float e0 = e[0], e1 = e[1], e2 = e[2];
    uint32_t i;
    for (i = 0; i < CH; i++) {
        float v = x[i] - s2, t1 = a1 * s1 + a2 * v, t2 = s2 + a2 * s1 + a3 * v;
        s1 = 2.0f * t1 - s1;
        s2 = 2.0f * t2 - s2;
        b[i] += o0 * x[i] + o1 * t1 + o2 * t2;
        o0 += e0;
        o1 += e1;
        o2 += e2;
    }
    s[0] = s1;
    s[1] = s2;
}

/* the band or the low alone (TYPE as it stays), both sides at once */
static __attribute__((noinline)) void ch_svf2(float *ba, float *bb, const float *c, float *s, float o, int low)
{
    float a1 = c[0], a2 = c[1], a3 = c[2], l1 = s[0], l2 = s[1], r1 = s[2], r2 = s[3];
    uint32_t i;
    for (i = 0; i < CH; i++) {
        float vl = ch_x0[i] - l2, vr = ch_x1[i] - r2;
        float tl1 = a1 * l1 + a2 * vl, tr1 = a1 * r1 + a2 * vr;
        float tl2 = l2 + a2 * l1 + a3 * vl, tr2 = r2 + a2 * r1 + a3 * vr;
        l1 = 2.0f * tl1 - l1;
        r1 = 2.0f * tr1 - r1;
        l2 = 2.0f * tl2 - l2;
        r2 = 2.0f * tr2 - r2;
        ba[i] += o * (low ? tl2 : tl1);
        bb[i] += o * (low ? tr2 : tr1);
    }
    s[0] = l1;
    s[1] = l2;
    s[2] = r1;
    s[3] = r2;
}

/* the low-passes, two one-poles a side, both sides at once */
static __attribute__((noinline)) void ch_lp(float *ba, float *bb, float a, float g, float *s)
{
    float l1 = s[0], l2 = s[1], r1 = s[2], r2 = s[3];
    uint32_t i;
    for (i = 0; i < CH; i++) {
        l1 += a * (ch_x0[i] - l1);
        l2 += a * (l1 - l2);
        r1 += a * (ch_x1[i] - r1);
        r2 += a * (r1 - r2);
        ba[i] += l2 * g;
        bb[i] += r2 * g;
    }
    s[0] = l1;
    s[1] = l2;
    s[2] = r1;
    s[3] = r2;
}

static inline float blepi(float t, float dt, float idt)
{
    if (t < dt) {
        t *= idt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) * idt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/* CHOIR's saws into the sides' inputs, in runs between the wraps (FRASIL's choir_run), then SOFT, then the filter */
static inline __attribute__((always_inline)) void choir_run(voice_t *v, float *ba, float *bb, float g, const int sq,
                                                            const int svf)
{
    float p0 = v->cph[0], p1 = v->cph[1], p2 = v->cph[2], d0 = v->cdt[0], d1 = v->cdt[1], d2 = v->cdt[2];
    float id0 = v->cid[0], id1 = v->cid[1], id2 = v->cid[2];
    float sh = v->csh, w0 = v->cw[0], w1 = v->cw[1], w2 = v->cw[2];
    const int soft = P.ch_soft > 0.002f;
    uint32_t i = 0;
    while (i < CH) {
        uint32_t m = ch_run(p0, d0, id0, CH - i), e;
        float A = 2.0f, k0 = -2.0f, k1 = -2.0f, b0 = -1.0f, b1 = -1.0f, b2 = -1.0f;
        m = ch_run(p1, d1, id1, m);
        m = ch_run(p2, d2, id2, m);
        if (sq) {
            float q0 = p0 + w0, q1 = p1 + w1, q2 = p2 + w2, c0, c1, c2;
            c0 = q0 >= 1.0f ? 1.0f : 0.0f;
            c1 = q1 >= 1.0f ? 1.0f : 0.0f;
            c2 = q2 >= 1.0f ? 1.0f : 0.0f;
            q0 -= c0;
            q1 -= c1;
            q2 -= c2;
            if (q0 < CH_EPS || q1 < CH_EPS || q2 < CH_EPS)
                m = 0;
            m = ch_run(q0, d0, id0, m);
            m = ch_run(q1, d1, id1, m);
            m = ch_run(q2, d2, id2, m);
            A = 2.0f - 2.0f * sh;
            b0 = sh - 1.0f - 2.0f * sh * (w0 - c0);
            b1 = sh - 1.0f - 2.0f * sh * (w1 - c1);
            b2 = sh - 1.0f - 2.0f * sh * (w2 - c2);
            k0 = b0 + b1;
            k1 = b0 + b2;
        }
        if (soft)
            for (e = i + m; i < e; i++) {
                p0 += d0;
                p1 += d1;
                p2 += d2;
                ch_xc[i] = A * p0 + b0;
                ch_x0[i] = A * p1 + b1;
                ch_x1[i] = A * p2 + b2;
            }
        else
            for (e = i + m; i < e; i++) {
                p0 += d0;
                p1 += d1;
                p2 += d2;
                ch_x0[i] = A * (p0 + p1) + k0;
                ch_x1[i] = A * (p0 + p2) + k1;
            }
        if (i < CH) {                    /* a saw at its wrap: the sample as it was, its bleps */
            float s0, s1, s2;
            p0 += d0;
            p1 += d1;
            p2 += d2;
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            if (p1 >= 1.0f)
                p1 -= 1.0f;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            s0 = 2.0f * p0 - 1.0f - blepi(p0, d0, id0);
            s1 = 2.0f * p1 - 1.0f - blepi(p1, d1, id1);
            s2 = 2.0f * p2 - 1.0f - blepi(p2, d2, id2);
            if (sq) {
                float q0 = p0 + w0, q1 = p1 + w1, q2 = p2 + w2;
                if (q0 >= 1.0f)
                    q0 -= 1.0f;
                if (q1 >= 1.0f)
                    q1 -= 1.0f;
                if (q2 >= 1.0f)
                    q2 -= 1.0f;
                s0 -= sh * (2.0f * q0 - 1.0f - blepi(q0, d0, id0));
                s1 -= sh * (2.0f * q1 - 1.0f - blepi(q1, d1, id1));
                s2 -= sh * (2.0f * q2 - 1.0f - blepi(q2, d2, id2));
            }
            if (soft) {
                ch_xc[i] = s0;
                ch_x0[i] = s1;
                ch_x1[i] = s2;
            } else {
                ch_x0[i] = s0 + s1;
                ch_x1[i] = s0 + s2;
            }
            i++;
        }
    }
    v->cph[0] = p0;
    v->cph[1] = p1;
    v->cph[2] = p2;
    if (soft) {                          /* SOFT: integrated once (the square a triangle), then twice (near a sine),
                                          * each saw at its own gain, a leak at a sixth of it; crossfaded by quarter
                                          * turns: it keeps its level */
        float kc = 6.2831853f * d0, ks = 6.2831853f * d1, lc = 1.0f - 0.15f * kc, ls = 1.0f - 0.15f * ks;
        float u = P.ch_soft, a0, a1, a2, c1, c2, s1, s2;
        float cA = v->cint[0], cB = v->cint[1], l1 = v->cint[2], l2 = v->cint[3], r1 = v->cint[4], r2 = v->cint[5];
        uint32_t j;
        if (u < 0.5f) {
            a0 = fm_cosf(3.14159265f * u);
            a1 = fm_sinf(3.14159265f * u);
            a2 = 0.0f;
        } else {
            a0 = 0.0f;
            a1 = fm_cosf(3.14159265f * (u - 0.5f));
            a2 = fm_sinf(3.14159265f * (u - 0.5f));
        }
        c1 = a1 * kc;
        c2 = a2 * kc * kc;
        s1 = a1 * ks;
        s2 = a2 * ks * ks;
        for (j = 0; j < CH; j++) {
            float xc = ch_xc[j], xl = ch_x0[j], xr = ch_x1[j], c;
            cA = lc * cA + xc;
            cB = lc * cB + cA;
            l1 = ls * l1 + xl;
            l2 = ls * l2 + l1;
            r1 = ls * r1 + xr;
            r2 = ls * r2 + r1;
            c = a0 * xc + c1 * cA + c2 * cB;
            ch_x0[j] = c + a0 * xl + s1 * l1 + s2 * l2;
            ch_x1[j] = c + a0 * xr + s1 * r1 + s2 * r2;
        }
        if (!(cB > -1e9f && cB < 1e9f && l2 > -1e9f && l2 < 1e9f && r2 > -1e9f && r2 < 1e9f))
            cA = cB = l1 = l2 = r1 = r2 = 0.0f;
        v->cint[0] = cA;
        v->cint[1] = cB;
        v->cint[2] = l1;
        v->cint[3] = l2;
        v->cint[4] = r1;
        v->cint[5] = r2;
    } else {
        v->cint[0] = v->cint[1] = v->cint[2] = v->cint[3] = v->cint[4] = v->cint[5] = 0.0f;
    }
    if (svf) {                           /* the type's mix with the gain in it, gliding to sc over the chunk */
        float cf[3], o[3], e[3];
        cf[0] = v->sa1;
        cf[1] = v->sa2;
        cf[2] = v->sa3;
        o[0] = v->sp0 * g;
        o[1] = v->sp1 * g;
        o[2] = v->sp2 * g;
        e[0] = (v->sc0 - v->sp0) * g * (1.0f / (float)CH);
        e[1] = (v->sc1 - v->sp1) * g * (1.0f / (float)CH);
        e[2] = (v->sc2 - v->sp2) * g * (1.0f / (float)CH);
        if (e[0] == 0.0f && e[1] == 0.0f && e[2] == 0.0f && o[0] == 0.0f && (o[1] == 0.0f || o[2] == 0.0f)) {
            ch_svf2(ba, bb, cf, v->clp, o[1] == 0.0f ? o[2] : o[1], o[1] == 0.0f);
        } else {
            ch_svf(ba, ch_x0, cf, v->clp, o, e);
            ch_svf(bb, ch_x1, cf, v->clp + 2, o, e);
        }
    } else {
        ch_lp(ba, bb, v->ca, g, v->clp);
    }
    v->sp0 = v->sc0;
    v->sp1 = v->sc1;
    v->sp2 = v->sc2;
}

static void choir_chunk(voice_t *v, float *ba, float *bb, float g)
{
    int svf = P.ch_reso > 0.002f || T.ch_type, sq = P.ch_shape > 0.002f;
    if (svf != v->csvf) {                /* the other filter: the low-pass's state carried over */
        v->clp[0] = svf ? 0.0f : v->clp[1];
        v->clp[2] = svf ? 0.0f : v->clp[3];
        v->csvf = (uint8_t)svf;
    }
    g *= v->cgain;
    if (sq && svf)
        choir_run(v, ba, bb, g, 1, 1);
    else if (sq)
        choir_run(v, ba, bb, g, 1, 0);
    else if (svf)
        choir_run(v, ba, bb, g, 0, 1);
    else
        choir_run(v, ba, bb, g, 0, 0);
}

/* partials j and k (k < 0: j alone) into one bus, two at a time; with BEAT their levels ramp */
static __attribute__((noinline)) void partial_ramp(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], e1 = 2.0f * v->ps[j], a1 = v->pac[j] * g, d1 = v->pda[j] * g;
    uint32_t i;
    if (k < 0) {
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            y1 += e1 * x1;
            b[i] += a1 * x1;
            a1 += d1;
        }
    } else {
        float x2 = v->px[k], y2 = v->py[k], e2 = 2.0f * v->ps[k], a2 = v->pac[k] * g, d2 = v->pda[k] * g;
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            x2 -= e2 * y2;
            y1 += e1 * x1;
            y2 += e2 * x2;
            b[i] += a1 * x1 + a2 * x2;
            a1 += d1;
            a2 += d2;
        }
        v->px[k] = x2;
        v->py[k] = y2;
        v->pac[k] += v->pda[k] * (float)CH;
    }
    v->px[j] = x1;
    v->py[j] = y1;
    v->pac[j] += v->pda[j] * (float)CH;
}

static __attribute__((noinline)) void partial_pair(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], e1 = 2.0f * v->ps[j], a1 = v->pa[j] * g;
    uint32_t i;
    if (k < 0) {
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            y1 += e1 * x1;
            b[i] += a1 * x1;
        }
    } else {
        float x2 = v->px[k], y2 = v->py[k], e2 = 2.0f * v->ps[k], a2 = v->pa[k] * g;
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            x2 -= e2 * y2;
            y1 += e1 * x1;
            y2 += e2 * x2;
            b[i] += a1 * x1 + a2 * x2;
        }
        v->px[k] = x2;
        v->py[k] = y2;
    }
    v->px[j] = x1;
    v->py[j] = y1;
}

/* the fundamental in the centre; harmonics 2, 4, 6 to one side, 3, 5 to the other */
static void partials_chunk(voice_t *v, float *bc, float *ba, float *bb, float g)
{
    int np = v->np;
    void (*f)(voice_t *, uint32_t, int, float *, float) = P.pa_beat > 0.001f ? partial_ramp : partial_pair;
    if (np > 0)
        f(v, 0, -1, bc, g);
    if (np > 1)
        f(v, 1, np > 3 ? 3 : -1, ba, g);
    if (np > 2)
        f(v, 2, np > 4 ? 4 : -1, bb, g);
    if (np > 5)
        f(v, 5, -1, ba, g);
}

static void breath_chunk(voice_t *v, const float *nz, float *bc, float *ba, float *bb, float g)
{
    uint32_t i;
    {                                    /* the first in the centre, with AIR (the noise itself) */
        float y1 = v->r1[0], y2 = v->r2[0], b1 = v->rb1[0], b2 = v->rb2[0], gg = v->rg[0] * g, ga = P.br_air * 0.7f * g;
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2;
            y2 = y1;
            y1 = y;
            bc[i] += y + ga * nz[i];
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        v->r1[0] = y1;
        v->r2[0] = y2;
    }
    {                                    /* the other two together */
        float y1 = v->r1[1], y2 = v->r2[1], b1 = v->rb1[1], b2 = v->rb2[1], gg = v->rg[1] * g;
        float z1 = v->r1[2], z2 = v->r2[2], c1 = v->rb1[2], c2 = v->rb2[2], gh = v->rg[2] * g;
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2, z = gh * nz[i] + c1 * z1 - c2 * z2;
            y2 = y1;
            y1 = y;
            z2 = z1;
            z1 = z;
            ba[i] += y;
            bb[i] += z;
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        if (z1 > -1e-20f && z1 < 1e-20f && z2 > -1e-20f && z2 < 1e-20f)
            z1 = z2 = 0.0f;
        v->r1[1] = y1;
        v->r2[1] = y2;
        v->r1[2] = z1;
        v->r2[2] = z2;
    }
}

/* BREATH's source for the block, the same for every voice: a hiss, or GRAIN's grains; TONE darker or brighter */
static void breath_noise(uint32_t n)
{
    uint32_t i;
    if (P.br_grain < 0.002f) {
        for (i = 0; i < n; i++)
            noise[i] = br_randf(&rng);
    } else {
        float p = fm_exp2f(-13.0f * P.br_grain) * BR_R44, amp = fm_sqrtf(1.0f / (3.0f * 0.5833f * p));
        uint32_t th = (uint32_t)(br_clampf(p, 0.0f, 1.0f) * 16777215.0f);
        if (amp > 16.0f)
            amp = 16.0f;
        for (i = 0; i < n; i++) {
            uint32_t r = br_rand(&rng);
            float a = amp * (0.5f + (float)(r & 255u) * (0.5f / 255.0f));
            noise[i] = (r >> 8) < th ? (r & 256u ? a : -a) : 0.0f;
        }
    }
    if (P.br_tone < -0.01f || P.br_tone > 0.01f) {
        float t = P.br_tone, fc, a, gn;
        if (t < 0.0f) {
            fc = BR_NYQ_SAFE * fm_powf(300.0f / BR_NYQ_SAFE, -t);
            a = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
            gn = fm_sqrtf((2.0f - a) / a);
            for (i = 0; i < n; i++) {
                nlp += a * (noise[i] - nlp);
                noise[i] = nlp * gn;
            }
        } else {
            fc = 20.0f * fm_powf(150.0f, t);
            a = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
            gn = 1.0f / fm_sqrtf(1.0f + a / (2.0f - a) - 2.0f * a);
            for (i = 0; i < n; i++) {
                nlp += a * (noise[i] - nlp);
                noise[i] = (noise[i] - nlp) * gn;
            }
        }
        nlp = fm_flush(nlp);
    }
}

/* ------------------------------------------------------------ the voices --- */
static void voices(float *ol, float *or_)
{
    float ka, kr, sl, sr;
    uint32_t i, c, j;
    for (i = 0; i < BLK; i++)
        ol[i] = or_[i] = 0.0f;
    ka = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.005f * fm_powf(6000.0f, P.attack)));
    kr = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.05f * fm_powf(1200.0f, P.release)));
    sl = 0.5f + 0.4f * P.spread;         /* a side's share on its own side */
    sr = 1.0f - sl;
    for (j = 0; j < NV; j++) {
        voice_t *v = &vo[j];
        if (v->note < 0) {
            lvl_view[j] = 0.0f;
            continue;
        }
        voice_block(v);
        for (c = 0; c < BLK; c += CH) {
            float bc[CH], ba[CH], bb[CH], g0 = v->lvl, g1, dg, al, ar, bl, br;
            v->env += ((v->gate ? 1.0f : 0.0f) - v->env) * (v->gate ? ka : kr);
            g1 = !v->gate && v->env < TAIL_END ? 0.0f : 0.8f * VOICE_G * v->env * v->env;
            v->lvl = g1;
            dg = (g1 - g0) * (1.0f / (float)CH);
            for (i = 0; i < CH; i++)
                bc[i] = ba[i] = bb[i] = 0.0f;
            if (P.choir > 0.001f)
                choir_chunk(v, ba, bb, 0.35f * P.choir);
            if (P.partials > 0.001f)
                partials_chunk(v, bc, ba, bb, P.partials);
            if (P.souffle > 0.001f)
                breath_chunk(v, noise + c, bc, ba, bb, P.souffle);
            {                            /* a guard: a voice gone wild never reaches the mix; it starts again */
                float z = bc[CH - 1] + ba[CH - 1] + bb[CH - 1] + bc[CH / 2] + ba[CH / 2] + bb[CH / 2];
                if (!(z > -1e4f && z < 1e4f)) {
                    for (i = 0; i < CH; i++)
                        bc[i] = ba[i] = bb[i] = 0.0f;
                    voice_start(v, v->note);
                    v->gate = 1;
                }
            }
            al = v->gl * sl;
            ar = v->gr * sr;
            bl = v->gl * sr;
            br = v->gr * sl;
            for (i = 0; i < CH; i++) {
                g0 += dg;
                ol[c + i] += (bc[i] * v->gl + ba[i] * al + bb[i] * bl) * g0;
                or_[c + i] += (bc[i] * v->gr + ba[i] * ar + bb[i] * br) * g0;
            }
            if (g1 == 0.0f && !v->gate && v->env < TAIL_END)
                break;
        }
        lvl_view[j] = v->env;
        if (!v->gate && v->env < TAIL_END) {
            v->note = -1;                /* faded out: free */
            lvl_view[j] = 0.0f;
        }
    }
}

/* ----------------------------------------------------- 2x up to 48 kHz --- */
/* a halfband interpolator: the even outputs are the input (delayed to the filter's centre), the odd ones a
 * 12-tap symmetric FIR (a Blackman-windowed sinc) between them */
#define UPN 12
static float up_c[UPN / 2];
static float up_hl[2 * UPN], up_hr[2 * UPN];   /* the last UPN inputs, twice over (a ring read as one run) */
static uint32_t up_i;                         /* where the newest is */

static void up_init(void)
{
    float s = 0.0f;
    uint32_t j;
    for (j = 0; j < UPN / 2; j++) {
        float t = (float)j + 0.5f, x = 3.14159265f * t, wn = (t + (float)UPN / 2) / (float)UPN;   /* 0..1 across */
        float win = 0.42f - 0.5f * fm_cosf(6.2831853f * wn) + 0.08f * fm_cosf(12.566371f * wn);
        up_c[j] = fm_sinf(x) / x * win;
        s += 2.0f * up_c[j];
    }
    for (j = 0; j < UPN / 2; j++)
        up_c[j] /= s;
    for (j = 0; j < 2 * UPN; j++)
        up_hl[j] = up_hr[j] = 0.0f;
    up_i = 0;
}

static void upsample(const float *l, const float *r, float *ol, float *or_, uint32_t n)
{
    const float c0 = up_c[0], c1 = up_c[1], c2 = up_c[2], c3 = up_c[3], c4 = up_c[4], c5 = up_c[5];
    uint32_t i, k = up_i;
    for (i = 0; i < n; i++) {
        const float *hl, *hr;
        k = k ? k - 1u : UPN - 1u;
        up_hl[k] = up_hl[k + UPN] = l[i];
        up_hr[k] = up_hr[k + UPN] = r[i];
        hl = up_hl + k;                  /* newest first */
        hr = up_hr + k;
        ol[2 * i] = hl[6];
        or_[2 * i] = hr[6];
        ol[2 * i + 1] = c0 * (hl[5] + hl[6]) + c1 * (hl[4] + hl[7]) + c2 * (hl[3] + hl[8]) + c3 * (hl[2] + hl[9]) +
                        c4 * (hl[1] + hl[10]) + c5 * (hl[0] + hl[11]);
        or_[2 * i + 1] = c0 * (hr[5] + hr[6]) + c1 * (hr[4] + hr[7]) + c2 * (hr[3] + hr[8]) + c3 * (hr[2] + hr[9]) +
                         c4 * (hr[1] + hr[10]) + c5 * (hr[0] + hr[11]);
    }
    up_i = k;
}

/* ---------------------------------------------------------------- render --- */
void drone_set(const drone_params_t *p)
{
    uint32_t ix = pend_ix ^ 1u;          /* the copy the render is not reading */
    pend[ix] = *p;
    __asm__ volatile("" ::: "memory");
    pend_ix = ix;
    pend_seq = pend_seq + 1u;
}

static void block(float *ol, float *or_)
{
    uint32_t i;
    float dk, dmake, gain, dg;
    const float kdc = br_k(0.0014f), pkd = 0.97f;
    if (pend_seen != pend_seq) {
        pend_seen = pend_seq;
        T = pend[pend_ix];
    }
    {
        float *pf = (float *)&P, gk = k_at(0.2f, R_BLOCK);
        const float *tf = (const float *)&T;
        for (i = 0; i < NGLIDE; i++)     /* ~25 ms */
            pf[i] += (tf[i] - pf[i]) * gk;
        P.ch_type = T.ch_type;
    }
    {                                    /* the shedding: the cap glides a partial in some 50 ms */
        float t = (float)SHED_NP[shed] - np_cap, st = 0.12f * BR_R44 * R_BLOCK;
        np_cap += t > st ? st : t < -st ? -st : t;
    }
    events();
    drift(BLK);
    breath_noise(BLK);
    voices(mixl, mixr);
    /* DC out, warmth */
    /* WARMTH: FRASIL's tanh, here the soft clip's cubic (its shape near enough, a tenth of the time) */
    dk = 1.0f + 5.0f * P.drive;
    dmake = 0.35f / br_soft(dk * 0.35f);
    for (i = 0; i < BLK; i++) {
        dcl += kdc * (mixl[i] - dcl);
        dcr += kdc * (mixr[i] - dcr);
        mixl[i] -= dcl;
        mixr[i] -= dcr;
        spl[i] = spr[i] = 0.0f;
    }
    if (P.drive > 0.001f)
        for (i = 0; i < BLK; i++) {
            mixl[i] = br_soft(mixl[i] * dk) * dmake;
            mixr[i] = br_soft(mixr[i] * dk) * dmake;
        }
    sparkle();
    glint_render(BLK);
    bass_render(BLK);
    /* the drones, the glints and the bass; the soft limit; the level */
    gain = P.level;
    dg = (gain - gain_now) * (1.0f / (float)BLK);
    for (i = 0; i < BLK; i++) {
        float l = br_soft(mixl[i] + spl[i] + bss[i]);
        float r = br_soft(mixr[i] + spr[i] + bss[i]);
        float a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
        peak = a > peak ? a : peak;
        gain_now += dg;
        mixl[i] = l * gain_now;
        mixr[i] = r * gain_now;
    }
    gain_now = gain;
    dcl = fm_flush(dcl);
    dcr = fm_flush(dcr);
    peak_view = peak;
    peak *= pkd;
    upsample(mixl, mixr, ol, or_, BLK);
}

void drone_render(float *l, float *r, uint32_t n)
{
    uint32_t o;
    for (o = 0; o + 2u * BLK <= n; o += 2u * BLK)
        block(l + o, r + o);
}

void drone_shed(int level) { shed = level < 0 ? 0 : level > 3 ? 3 : level; }
int drone_shed_level(void) { return shed; }

float drone_voice_level(int v) { return v >= 0 && v < NV ? lvl_view[v] : 0.0f; }
float drone_glint_level(void) { return glint_view; }
float drone_peak(void) { return peak_view; }

void drone_init(void)
{
    uint32_t i, j;
    {                                    /* everything as at power-on: the same start, the same sound */
        static const drone_params_t zp;
        static const voice_t zv;
        static const glint_t zg;
        P = T = pend[0] = pend[1] = zp;
        for (i = 0; i < NV; i++)
            vo[i] = zv;
        for (i = 0; i < GNV; i++)
            gv[i] = zg;
    }
    rng = 0x2545F491u;
    g_rng = 0x6C8E9CF5u;
    bs.pitch = 36.0f;
    bs.ph = 0.0f;
    for (i = 0; i < 3u; i++)
        for (j = 0; j < NR; j++)
            RATIO_L2[i][j] = fm_log2f(RATIOS[i][j]);
    for (i = 0; i <= TN; i++)
        sintab[i] = fm_sinf(6.2831853f * (float)i / (float)TN);
    for (i = 0; i < NV; i++) {
        vo[i].note = -1;
        lvl_view[i] = 0.0f;
    }
    for (i = 0; i < GNV; i++)
        gv[i].on = 0;
    bs.note = -1;
    bs.env = 0.0f;
    ev_r = ev_w = 0;
    tide_ph = tide_v = nlp = dcl = dcr = peak = gain_now = 0.0f;
    spk_wait = -1.0f;
    shed = 0;
    np_cap = (float)NP;
    up_init();
    pend_seen = pend_seq;
}
