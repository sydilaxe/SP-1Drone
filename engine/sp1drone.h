/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 SP-1Drone; the drone presets are FRASIL's (Copyright (C) 2026 Styves Goupil, engine/params.c) */
/* SP-1Drone's brain: what the SP-1's controls mean for the sound (drone.h). No platform here: the firmware and the
 * host tests both drive it. Call everything from one thread (the firmware's main loop); drone_render runs on the
 * audio thread. */
#pragma once
#include <stdint.h>

#define SDB_PAGES 3
#define SDB_NPRESET 36
#define SDB_NCHORD 4

/* the faders' settings, page by page (0..1 each) */
enum {
    SDB_CHOIR, SDB_PARTIALS, SDB_BREATH, SDB_BASS,      /* page 1, MIX: the three layers and the bass */
    SDB_TIMBRE, SDB_MOTION, SDB_TIDE, SDB_WARMTH,       /* page 2, TONE */
    SDB_SPARKLE, SDB_DETUNE, SDB_EVOLVE, SDB_RELEASE,   /* page 3, SHAPE */
    SDB_NSET
};

void sdb_init(void);                     /* also drone_init; the first preset (MIST), nothing playing */
float sdb_get(int s);                    /* a setting, 0..1 */
void sdb_set(int s, float x);            /* a fader moved it */
void sdb_track(int v);                   /* TRACK n: drone voice n on (it swells in) or off (it fades) */
int sdb_voice_on(int v);
void sdb_play(void);                     /* PLAY: everything sounding lets go (and is remembered); with nothing
                                          * sounding, the remembered voices (at first all four) come back */
int sdb_playing(void);                   /* a voice is on */
void sdb_preset(int i);                  /* a drone preset, 0 .. SDB_NPRESET-1: its sound, its settings on the faders */
int sdb_preset_now(void);
const char *sdb_preset_name(int i);
void sdb_chord(int c);                   /* 0 FIFTHS, 1 MINOR, 2 MAJOR, 3 SUS: the four voices' notes over the root */
int sdb_chord_now(void);
void sdb_root(int d);                    /* the root a semitone up (+1) or down (-1), D2 .. D4 */
int sdb_root_now(void);                  /* its MIDI note */
void sdb_level(float g);                 /* the output's level, 0..1 */
void sdb_apply(void);                    /* the settings to the sound (after any of the above) */
