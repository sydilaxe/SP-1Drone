/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil (FRASIL's engine/dsp/frasil.h), changed for SP-1Drone (2026) */
/* SP-1Drone's sound: FRASIL's drone, made to fit the SP-1.
 *
 * Four drone voices (one per TRACK button), each FRASIL's three layers: CHOIR (three detuned PolyBLEP saws or pulses
 * through a filter that follows the note), PARTIALS (eight additive partials, each drifting on its own) and BREATH
 * (noise or grains through three resonators tuned to the note); their slow drift (MOTION) and the tide on the timbre;
 * WARMTH. Under them a BASS an octave down; over them GLINT's icy FM bells, struck by SPARKLE on the notes the drones
 * hold. Dry: it is made to go through effects of its own (FRASIL's CLOUD reverb is left out).
 *
 * Changed from FRASIL: 4 voices (8 there), no reverb, no MIDI, no sequencers, no LFOs, no master effects; it runs at
 * 24 kHz (dsp.h) and comes out at 48 kHz through a halfband upsampler.
 *
 * Threads: the main loop calls drone_set and the note functions (the notes are queued); the audio thread calls
 * drone_render. */
#pragma once
#include <stdint.h>

#define DRONE_NV 4
#define DRONE_OUT_BLOCK 256u             /* drone_render's frames (48 kHz) are a multiple of this: 5.3 ms */

typedef struct {
    /* these floats glide (no steps, no clicks), in this order, up to ch_type */
    float choir, partials, souffle;      /* the layers' levels 0..1 */
    float timbre;                        /* all three brighter or darker, 0..1 (0.5 neutral) */
    float detune;                        /* CHOIR: the saws' spread, 0 .. 40 cents */
    float cutoff;                        /* CHOIR: low-pass, 0..1: 1 .. 40 harmonics */
    float tilt;                          /* PARTIALS: 0 bright .. 1 dark */
    float odd;                           /* PARTIALS: 0 all harmonics .. 1 odd ones only */
    float stretch;                       /* PARTIALS: 0 harmonic .. 1 bell-like */
    float resonance;                     /* BREATH: 0 air .. 1 glass */
    float color;                         /* BREATH: the resonators' ratios, harmonic .. odd .. bell */
    float attack, release;               /* 0..1: 5 ms .. 30 s; 50 ms .. 60 s */
    float spread;                        /* the voices across the stereo field, 0..1 */
    float motion, motion_rate;           /* drift: depth, speed */
    float tide, tide_rate;               /* the tide on the timbre: depth, speed (one turn 5 min .. 10 s) */
    float drive;                         /* WARMTH, 0..1 */
    float ch_shape;                      /* CHOIR: each saw toward a pulse, 0 saw .. 0.5 square .. 1 narrow */
    float ch_interval;                   /* CHOIR: the centre saw's frequency over the note's (1, 0.5, 1.5, 2) */
    float ch_reso;                       /* CHOIR: the filter's peak, 0 (Q 0.5) .. 1 (Q 20) */
    float ch_soft;                       /* CHOIR: its wave softened, 0 .. 1 (near a sine) */
    float pa_evolve;                     /* PARTIALS: their levels wandering, 0..1 */
    float pa_beat;                       /* PARTIALS: each one pulsing at its own pace, 0..1 */
    float pa_formant;                    /* PARTIALS: a formant's place, log2 of the harmonic, 0 .. 3 */
    float pa_peak;                       /* PARTIALS: how strong, 0 none .. 1 that harmonic alone */
    float br_grain;                      /* BREATH: 0 a hiss .. 1 sparse grains */
    float br_pitch;                      /* BREATH: the resonators' frequencies over the note's (0.5 .. 4) */
    float br_tone;                       /* BREATH: the noise's colour, -1 dark .. 1 bright */
    float br_air;                        /* BREATH: the noise itself beside the resonators, 0..1 */
    float bass;                          /* BASS: its level, 0..1 */
    float sparkle;                       /* SPARKLE: how often a glint strikes, 0 never .. 1 six a second */
    float level;                         /* the whole, 0..1 */
    /* (from here, no glide) */
    int ch_type;                         /* CHOIR: the filter, 0 low-pass, 1 band-pass, 2 high-pass */
} drone_params_t;

void drone_init(void);
void drone_set(const drone_params_t *p);           /* main loop */
void drone_voice_on(int v, int note);              /* main loop: voice v (0..3) to note; already on: it glides there */
void drone_voice_off(int v);                       /* it fades over RELEASE */
void drone_bass_on(int note);                      /* the bass to note (gliding if it sounds) */
void drone_bass_off(void);
void drone_panic(void);                            /* silence now: voices, bass and glints */
/* n frames at 48 kHz (a multiple of DRONE_OUT_BLOCK) into l and r, -1 .. 1 */
void drone_render(float *l, float *r, uint32_t n);

/* the load (FRASIL's shedding): 0 every partial; 1 .. 3 fewer partials (5, 4, 3), each faded out, not cut. From the
 * platform when a render runs late, and back to 0 as the time allows */
void drone_shed(int level);
int drone_shed_level(void);

/* for the lights and the status line (any thread; a block old) */
float drone_voice_level(int v);                    /* the voice's envelope now, 0..1 (0: silent) */
float drone_glint_level(void);                     /* the glints' loudest envelope now, 0..1 */
float drone_peak(void);                            /* the output's recent peak, 0..1 */
