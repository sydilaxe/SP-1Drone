/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil (FRASIL's engine/dsp/dsp.h), changed for SP-1Drone (2026) */
/* What every DSP unit shares: the rate, small helpers.
 *
 * Changed from FRASIL's dsp.h: the engine runs at one fixed rate, 24 kHz (the SP-1's 64 MHz Cortex-M4 has a quarter
 * of the FM-1's time for each sample at 48 kHz; at 24 kHz the voices and the reverb get twice as long, and the
 * output is upsampled to the codec's 48 kHz at the end). FRASIL's steps were made for 44.1 kHz: br_k, br_d and br_n
 * make each one over for this rate, as FRASIL does for its own rates. */
#pragma once
#include <stdint.h>
#include "fastmath.h"

#define BR_FS 24000.0f
#define BR_INV_FS (1.0f / 24000.0f)
#define BR_R44 (44100.0f / 24000.0f)     /* a step made for 44.1 kHz, in steps here */
#define BR_NYQ_SAFE 10500.0f             /* the highest a filter or a partial goes (FRASIL's 15 .. 16 kHz, here) */

static inline __attribute__((always_inline)) float br_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float br_lerp(float a, float b, float t) { return a + (b - a) * t; }

/* a one-pole's coefficient, a decay and a count made for 44.1 kHz, here: the same time */
static inline float br_k(float k) { return 1.0f - fm_powf(1.0f - k, BR_R44); }
static inline float br_d(float d) { return fm_powf(d, BR_R44); }
static inline float br_n(float n) { return n / BR_R44; }

/* soft clip: unity slope at 0, x - 4/27 x^3 up to |x| = 1.5, where it reaches +-1 with a flat slope */
static inline __attribute__((always_inline)) float br_soft(float x)
{
    x = br_clampf(x, -1.5f, 1.5f);
    return x - 0.148148148f * x * x * x;
}

/* 32-bit xorshift: noise and drift */
static inline uint32_t br_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static inline float br_randf(uint32_t *s) { return (float)(int32_t)br_rand(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
