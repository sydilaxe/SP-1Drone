/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 SP-1Drone; the drone presets are FRASIL's (Copyright (C) 2026 Styves Goupil, engine/params.c) */
/* SP-1Drone's brain (sp1drone.h). */
#include "sp1drone.h"
#include "drone.h"
#include "dsp.h"

/* FRASIL's 36 drone presets, as its params.c has them (each value 0..127; INT, TYP and PIT are choices):
 *  CHO PAR SOU WAR DET FIL TIL ODD STR BRES COL SPR ATT REL MOT RAT TID PER TIM SPA SHP INT CRES TYP EVO BEAT
 *  FORM PEAK GRN PIT TONE AIR SOFT */
enum { CHO, PAR, SOU, WAR, DET, FIL, TIL, ODD, STR, BRES, COL, SPR, ATT, REL, MOT, RAT, TID, PER, TIM, SPA, SHP, INT,
       CRES, TYP, EVO, BEAT, FORM, PEAK, GRN, PIT, TONE, AIR, SOFT, NDRONE };
static const char *const NAME[SDB_NPRESET] = {"MIST", "CATHEDRAL", "GLACIER", "ABYSS", "AURORA", "EMBER", "TIDE", "DUST",
    "WIND", "ORGAN", "HOLLOW", "VOWEL", "SHIMMER", "CHANTER", "ENSEMBLE", "BELLS", "NOCTURNE", "HAZE", "PULSE", "RIME",
    "DEEP", "SPECTRAL", "RUST", "STILL", "VELVET", "CHROME", "SWARM", "FOGHORN", "SOLAR", "CAVE", "GLASS HARP", "MONK",
    "STATIC", "LULLABY", "STORM", "PRISM"};
static const uint8_t PRESET[SDB_NPRESET][NDRONE] = {
    {76, 89, 32, 19, 51, 44, 57, 25, 0, 70, 0, 89, 89, 83, 44, 51, 25, 38, 64, 57, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0, 0},
    {110, 50, 20, 25, 85, 55, 60, 30, 0, 60, 0, 110, 100, 100, 60, 40, 20, 50, 60, 95, 0, 1, 20, 0, 10, 0, 85, 0, 0, 1, 64, 0, 0},
    {20, 100, 70, 5, 30, 60, 35, 50, 60, 115, 127, 100, 80, 110, 40, 30, 30, 40, 80, 90, 0, 0, 0, 0, 40, 15, 100, 30, 20, 2, 80, 0, 0},
    {90, 70, 25, 50, 70, 20, 90, 40, 0, 50, 0, 70, 100, 105, 50, 25, 50, 25, 40, 70, 30, 1, 40, 0, 20, 0, 60, 0, 0, 0, 30, 0, 0},
    {60, 105, 40, 15, 45, 65, 40, 15, 10, 85, 60, 115, 90, 105, 55, 45, 45, 50, 75, 95, 0, 0, 0, 0, 70, 25, 90, 35, 0, 2, 75, 10, 0},
    {100, 55, 20, 80, 40, 40, 70, 60, 0, 50, 30, 80, 70, 80, 35, 40, 20, 40, 55, 50, 64, 0, 50, 0, 10, 0, 70, 0, 0, 1, 50, 0, 0},
    {80, 80, 45, 25, 60, 45, 55, 30, 10, 75, 40, 100, 95, 100, 70, 30, 110, 60, 60, 70, 0, 0, 30, 0, 30, 0, 85, 0, 0, 1, 64, 0, 0},
    {25, 40, 85, 20, 40, 50, 60, 30, 20, 100, 60, 120, 60, 90, 50, 50, 30, 40, 60, 75, 0, 0, 0, 0, 30, 0, 85, 0, 90, 2, 85, 40, 0},
    {0, 20, 85, 0, 40, 50, 60, 30, 0, 20, 30, 127, 80, 95, 90, 80, 50, 70, 64, 80, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 75, 50, 0},
    {40, 115, 10, 30, 10, 50, 45, 0, 0, 60, 0, 60, 40, 60, 15, 40, 10, 40, 64, 50, 0, 2, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0, 0},
    {105, 80, 25, 20, 35, 50, 50, 110, 0, 70, 64, 90, 80, 90, 40, 45, 25, 40, 60, 65, 64, 0, 40, 1, 15, 0, 85, 0, 0, 1, 60, 0, 0},
    {65, 125, 15, 10, 30, 55, 30, 20, 0, 70, 0, 85, 90, 95, 45, 35, 60, 45, 64, 70, 0, 0, 60, 1, 20, 0, 95, 105, 0, 1, 64, 0, 0},
    {45, 100, 50, 10, 55, 75, 25, 20, 15, 100, 90, 120, 85, 110, 50, 55, 30, 40, 85, 100, 0, 3, 0, 0, 40, 50, 100, 20, 10, 3, 90, 20, 0},
    {105, 45, 15, 40, 30, 55, 60, 70, 0, 60, 30, 70, 50, 70, 30, 45, 15, 40, 60, 45, 110, 0, 35, 1, 0, 0, 85, 0, 0, 1, 64, 0, 0},
    {115, 40, 20, 30, 75, 60, 55, 25, 0, 60, 0, 115, 95, 90, 55, 55, 20, 40, 65, 75, 90, 0, 15, 0, 0, 0, 85, 0, 0, 1, 64, 0, 0},
    {15, 115, 45, 5, 25, 50, 30, 20, 95, 110, 127, 95, 40, 110, 30, 40, 20, 40, 75, 85, 0, 0, 0, 0, 30, 30, 85, 0, 0, 2, 80, 0, 0},
    {90, 60, 30, 35, 55, 25, 85, 30, 0, 55, 0, 90, 110, 115, 45, 20, 40, 20, 45, 85, 20, 1, 70, 0, 25, 0, 70, 0, 0, 0, 35, 0, 0},
    {85, 50, 60, 10, 60, 70, 50, 25, 0, 40, 30, 120, 100, 105, 60, 50, 30, 50, 70, 90, 0, 3, 20, 2, 20, 0, 85, 0, 0, 2, 80, 30, 0},
    {50, 95, 25, 20, 30, 50, 45, 30, 0, 70, 20, 100, 60, 80, 30, 50, 20, 40, 64, 65, 0, 0, 0, 0, 15, 100, 85, 0, 0, 1, 64, 0, 0},
    {30, 110, 70, 0, 20, 80, 20, 40, 30, 120, 100, 110, 70, 100, 35, 60, 20, 40, 90, 90, 0, 3, 0, 2, 30, 20, 110, 40, 40, 4, 100, 15, 0},
    {100, 100, 0, 40, 25, 25, 100, 20, 0, 50, 0, 60, 90, 95, 25, 30, 20, 30, 40, 45, 0, 1, 10, 0, 0, 0, 40, 0, 0, 0, 30, 0, 0},
    {20, 120, 15, 5, 20, 50, 20, 10, 0, 70, 0, 90, 90, 100, 50, 40, 80, 50, 64, 80, 0, 0, 0, 0, 50, 10, 85, 127, 0, 1, 64, 0, 0},
    {80, 50, 40, 100, 50, 45, 65, 60, 20, 80, 40, 80, 60, 80, 40, 55, 25, 40, 55, 55, 70, 0, 80, 1, 20, 15, 75, 0, 60, 1, 45, 20, 0},
    {0, 125, 0, 0, 0, 50, 127, 0, 0, 50, 0, 60, 100, 100, 5, 20, 0, 40, 50, 70, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0, 0},
    {95, 45, 10, 15, 35, 40, 85, 20, 0, 50, 0, 100, 105, 110, 35, 30, 20, 45, 52, 70, 0, 0, 10, 0, 0, 0, 85, 0, 0, 1, 50, 0, 120},
    {35, 115, 15, 35, 15, 75, 15, 120, 45, 90, 80, 85, 60, 90, 35, 55, 15, 40, 85, 60, 64, 3, 85, 2, 20, 35, 110, 70, 0, 3, 95, 0, 0},
    {120, 75, 30, 30, 120, 60, 40, 30, 0, 60, 20, 127, 70, 85, 110, 100, 40, 15, 70, 60, 120, 0, 40, 1, 40, 70, 85, 0, 30, 2, 70, 20, 0},
    {127, 60, 15, 70, 20, 22, 95, 90, 0, 40, 0, 50, 120, 120, 20, 20, 60, 70, 35, 65, 64, 1, 60, 0, 0, 0, 40, 30, 0, 0, 30, 0, 30},
    {70, 105, 45, 20, 55, 90, 15, 15, 5, 80, 30, 120, 100, 110, 60, 50, 70, 35, 100, 90, 10, 3, 15, 2, 60, 25, 120, 40, 0, 3, 110, 30, 0},
    {55, 40, 95, 30, 30, 18, 110, 30, 0, 100, 60, 70, 110, 120, 30, 20, 45, 60, 30, 100, 0, 1, 50, 0, 10, 0, 40, 0, 50, 0, 18, 5, 60},
    {10, 120, 20, 0, 12, 60, 30, 10, 25, 100, 90, 110, 50, 120, 25, 30, 15, 55, 90, 95, 0, 3, 0, 0, 45, 40, 120, 110, 0, 4, 100, 0, 127},
    {110, 85, 10, 25, 30, 38, 70, 40, 0, 60, 0, 70, 100, 100, 30, 25, 30, 65, 50, 80, 0, 1, 55, 1, 15, 0, 62, 90, 0, 1, 50, 0, 90},
    {35, 40, 127, 15, 40, 70, 40, 50, 0, 110, 90, 127, 40, 70, 80, 110, 20, 20, 80, 55, 30, 0, 30, 2, 0, 0, 85, 0, 85, 3, 100, 40, 0},
    {85, 70, 8, 10, 22, 45, 100, 10, 0, 60, 0, 80, 110, 120, 20, 15, 35, 80, 55, 85, 0, 2, 0, 0, 10, 8, 85, 20, 0, 1, 64, 0, 110},
    {80, 40, 110, 90, 70, 28, 90, 40, 0, 70, 40, 127, 90, 110, 100, 85, 120, 25, 40, 75, 30, 1, 70, 0, 50, 20, 60, 0, 70, 0, 35, 60, 20},
    {50, 120, 25, 10, 40, 65, 25, 60, 15, 90, 50, 120, 85, 105, 60, 45, 50, 40, 80, 90, 0, 3, 20, 1, 100, 30, 105, 80, 0, 2, 85, 10, 40},
};
/* SP-1Drone's own for each preset (FRASIL keeps these in its scenes, not its drone presets): (CLOUD's DECAY and
 * SHIMMER, not used: no reverb here), SPARKLE */
static const uint8_t EXTRA[SDB_NPRESET][3] = {
    {80, 0, 0}, {105, 10, 0}, {95, 30, 45}, {85, 0, 0}, {95, 25, 20}, {70, 0, 0}, {90, 0, 0}, {80, 0, 10},
    {85, 0, 0}, {75, 0, 0}, {75, 0, 0}, {85, 0, 0}, {100, 70, 25}, {65, 0, 0}, {85, 0, 0}, {95, 15, 40},
    {95, 0, 0}, {90, 20, 0}, {75, 0, 0}, {100, 40, 50}, {80, 0, 0}, {90, 20, 0}, {70, 0, 0}, {85, 0, 0},
    {90, 0, 0}, {80, 25, 15}, {75, 0, 0}, {90, 0, 0}, {95, 35, 20}, {110, 0, 0}, {105, 45, 30}, {95, 0, 0},
    {70, 0, 0}, {95, 15, 15}, {90, 0, 0}, {95, 30, 20},
};
static const int8_t CHORD[SDB_NCHORD][4] = {
    {0, 7, 12, 19},                      /* FIFTHS: root, fifth, octave, the fifth over it */
    {0, 7, 15, 22},                      /* MINOR: root, fifth, a minor tenth, a minor fourteenth (open m7) */
    {0, 7, 16, 23},                      /* MAJOR: root, fifth, a tenth, a major fourteenth (open maj7) */
    {0, 7, 14, 17},                      /* SUS: root, fifth, ninth, eleventh */
};
static const float CH_IV[4] = {1.0f, 0.5f, 1.4983071f, 2.0f}, BR_IV[5] = {0.5f, 1.0f, 2.0f, 2.9966142f, 4.0f};

static struct {
    float set[SDB_NSET];
    int preset, chord, root;
    uint8_t on, kept;                    /* the voices on; what PLAY let go */
    float level;
} st;

static float u7(uint8_t x) { return (float)x * (1.0f / 127.0f); }

void sdb_init(void)
{
    drone_init();
    st.chord = 0;
    st.root = 50;                        /* D3 */
    st.on = 0;
    st.kept = 0x0F;
    st.level = 0.7f;
    st.set[SDB_BASS] = 0.35f;
    sdb_preset(0);
}

float sdb_get(int s) { return s >= 0 && s < SDB_NSET ? st.set[s] : 0.0f; }
void sdb_set(int s, float x)
{
    if (s >= 0 && s < SDB_NSET)
        st.set[s] = br_clampf(x, 0.0f, 1.0f);
}

static int note_of(int v) { return st.root + CHORD[st.chord][v]; }

static void bass_follow(void)
{
    if (st.on)
        drone_bass_on(st.root - 12);
    else
        drone_bass_off();
}

void sdb_track(int v)
{
    if (v < 0 || v > 3)
        return;
    st.on ^= (uint8_t)(1u << v);
    if (st.on & (1u << v))
        drone_voice_on(v, note_of(v));
    else
        drone_voice_off(v);
    bass_follow();
}

int sdb_voice_on(int v) { return (st.on >> v) & 1; }
int sdb_playing(void) { return st.on != 0; }

void sdb_play(void)
{
    int v;
    if (st.on) {
        st.kept = st.on;
        st.on = 0;
        for (v = 0; v < 4; v++)
            drone_voice_off(v);
    } else {
        st.on = st.kept ? st.kept : 0x0F;
        for (v = 0; v < 4; v++)
            if (st.on & (1u << v))
                drone_voice_on(v, note_of(v));
    }
    bass_follow();
}

void sdb_preset(int i)
{
    const uint8_t *p, *x;
    if (i < 0)
        i = SDB_NPRESET - 1;
    if (i >= SDB_NPRESET)
        i = 0;
    st.preset = i;
    p = PRESET[i];
    x = EXTRA[i];
    st.set[SDB_CHOIR] = u7(p[CHO]);
    st.set[SDB_PARTIALS] = u7(p[PAR]);
    st.set[SDB_BREATH] = u7(p[SOU]);
    st.set[SDB_TIMBRE] = u7(p[TIM]);
    st.set[SDB_MOTION] = u7(p[MOT]);
    st.set[SDB_TIDE] = u7(p[TID]);
    st.set[SDB_WARMTH] = u7(p[WAR]);
    st.set[SDB_SPARKLE] = u7(x[2]);
    st.set[SDB_DETUNE] = u7(p[DET]);
    st.set[SDB_EVOLVE] = u7(p[EVO]);
    st.set[SDB_RELEASE] = u7(p[REL]);
}

int sdb_preset_now(void) { return st.preset; }
const char *sdb_preset_name(int i) { return i >= 0 && i < SDB_NPRESET ? NAME[i] : "?"; }

void sdb_chord(int c)
{
    int v;
    if (c < 0 || c >= SDB_NCHORD)
        return;
    st.chord = c;
    for (v = 0; v < 4; v++)
        if (st.on & (1u << v))
            drone_voice_on(v, note_of(v));   /* (sounding: it glides there) */
}
int sdb_chord_now(void) { return st.chord; }

void sdb_root(int d)
{
    int v, r = st.root + d;
    if (r < 38 || r > 62)
        return;
    st.root = r;
    for (v = 0; v < 4; v++)
        if (st.on & (1u << v))
            drone_voice_on(v, note_of(v));
    bass_follow();
}
int sdb_root_now(void) { return st.root; }
void sdb_level(float g) { st.level = br_clampf(g, 0.0f, 1.0f); }

void sdb_apply(void)
{
    const uint8_t *p = PRESET[st.preset];
    drone_params_t d;
    float tn = (float)p[TONE];
    d.choir = st.set[SDB_CHOIR];
    d.partials = st.set[SDB_PARTIALS];
    d.souffle = st.set[SDB_BREATH];
    d.timbre = st.set[SDB_TIMBRE];
    d.motion = st.set[SDB_MOTION];
    d.tide = st.set[SDB_TIDE];
    d.drive = st.set[SDB_WARMTH];
    d.bass = st.set[SDB_BASS];
    d.sparkle = st.set[SDB_SPARKLE];
    d.level = st.level;
    d.detune = st.set[SDB_DETUNE];
    d.cutoff = u7(p[FIL]);
    d.tilt = u7(p[TIL]);
    d.odd = u7(p[ODD]);
    d.stretch = u7(p[STR]);
    d.resonance = u7(p[BRES]);
    d.color = u7(p[COL]);
    d.spread = u7(p[SPR]);
    d.attack = u7(p[ATT]);
    d.release = st.set[SDB_RELEASE];
    d.motion_rate = u7(p[RAT]);
    d.tide_rate = u7(p[PER]);
    d.ch_shape = u7(p[SHP]);
    d.ch_interval = CH_IV[p[INT] % 4u];
    d.ch_reso = u7(p[CRES]);
    d.ch_soft = u7(p[SOFT]);
    d.ch_type = p[TYP] % 3u;
    d.pa_evolve = st.set[SDB_EVOLVE];
    d.pa_beat = u7(p[BEAT]);
    d.pa_formant = 3.0f * u7(p[FORM]);
    d.pa_peak = u7(p[PEAK]);
    d.br_grain = u7(p[GRN]);
    d.br_pitch = BR_IV[p[PIT] % 5u];
    d.br_tone = tn >= 126.5f ? 1.0f : (tn - 64.0f) * (1.0f / 63.0f);
    d.br_air = u7(p[AIR]);
    drone_set(&d);
}
