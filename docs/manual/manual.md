---
title: SP-1Drone
subtitle: FRASIL's drones on the SP-1. Four voices, three layers each, a bass and icy glints, for your own effects.
pdf-name: SP-1Drone-manual
---

<!-- Keep this manual in step with the firmware: any change to what a control does, a light shows,
     a range, a timing or the flashing steps belongs here too. CI builds it into
     SP-1Drone-manual.pdf and attaches it to every release. -->

# Before you start

SP-1Drone is replacement firmware for the Teenage Engineering SP-1. It turns the SP-1 into a drone
instrument: each TRACK button lights a slow, living drone voice, the faders shape them, and the
result comes out of the headphone jack and USB-C at the same time, dry, ready for your pedals, a
mixer's effects, another SP-1, or a computer or phone to record it.

The sound is the engine of [FRASIL](https://github.com/styvesgoupil/frasil), Styves Goupil's
drone firmware for the M-VAVE FM-1, made to fit the SP-1: its three-layer drone voices, their
drift and tide, its 36 drone presets, its icy GLINT bells and a bass under them. FRASIL's reverb
is left out on purpose: SP-1Drone is meant to run through external effects.

> **Test build.** This firmware builds, its sound engine passes its tests, and its platform code
> follows firmware that does run on the SP-1, but it has not yet been run on a real SP-1. Read
> [Flashing and getting back](#flashing-and-getting-back) before you install it: the SP-1 can
> always be returned to other firmware.

## What you need

- A Teenage Engineering SP-1.
- Headphones, or a 3.5 mm cable to your effects or mixer; or a USB-C cable to a computer, phone
  or tablet to record it.
- For installing: a USB-C cable and a computer with Chrome or Edge.

## Quick start

1. Flash `sp1drone.bin` (see [Flashing and getting back](#flashing-and-getting-back)).
2. Hold **FUNCTION** for 1.5 seconds to power on.
3. Press **PLAY**. The four voices of a chord on D swell in over a few seconds.
4. Press a **TRACK** button to let that voice fade out or bring it back.
5. Move the faders. Press **>>** for the next page of settings.

# The sound

## Four voices

Each **TRACK** button is one drone voice. The four together play a chord over the **root** (D3 at
first). A voice swells in when you light it and fades out over its RELEASE time when you let it
go; it never changes by itself, but its timbre drifts and breathes slowly, so the drone is never
still.

Each voice has FRASIL's three layers. They all play the voice's note; the faders set how much of
each you hear:

| Layer | What it is |
|---|---|
| **CHOIR** | three detuned saws or pulses through a filter that follows the note |
| **PARTIALS** | six additive harmonics, each drifting on its own in level and tuning |
| **BREATH** | noise or sparse grains through three resonators tuned to the note, from air to glass |

## Bass and glints

Under the voices, a **BASS** holds the root an octave down while any voice sounds, gliding when the
root changes. Over them, **SPARKLE** strikes FRASIL's icy **GLINT** bells now and then on the notes
the voices hold, from a few a minute to several a second.

## Chords and root

Four chords, chosen with **FUNCTION + TRACK 1-4**. Each spreads its four voices wide:

| | Chord | Voices (semitones over the root) |
|---|---|---|
| TRACK 1 | **FIFTHS** | 0, 7, 12, 19 |
| TRACK 2 | **MINOR** | 0, 7, 15, 22 |
| TRACK 3 | **MAJOR** | 0, 7, 16, 23 |
| TRACK 4 | **SUS** | 0, 7, 14, 17 |

**FUNCTION + VOL +** and **FUNCTION + VOL -** move the root a semitone, from D2 to D4. Voices that
are sounding glide to their new notes.

## Presets

FRASIL's 36 drone presets, in its order, chosen with **FUNCTION + >>** and **FUNCTION + <<**:

MIST, CATHEDRAL, GLACIER, ABYSS, AURORA, EMBER, TIDE, DUST, WIND, ORGAN, HOLLOW, VOWEL, SHIMMER,
CHANTER, ENSEMBLE, BELLS, NOCTURNE, HAZE, PULSE, RIME, DEEP, SPECTRAL, RUST, STILL, VELVET, CHROME,
SWARM, FOGHORN, SOLAR, CAVE, GLASS HARP, MONK, STATIC, LULLABY, STORM, PRISM.

A preset sets the voices' whole sound and puts its values on the faders' settings (BASS stays as
you set it). The faders then pick up from there: see [Pages and pick-up](#pages-and-pick-up).

# Controls

| Control | What it does |
|---|---|
| **TRACK 1-4** | that voice on (it swells in) or off (it fades out) |
| **PLAY** | every voice off, remembered; press again and they come back (at first: all four) |
| **Faders 1-4** | the four settings of the page |
| **>> / <<** | next / previous page |
| **VOL + / VOL -** | output volume (jack and speaker), 3 dB steps |
| **FUNCTION + >> / <<** | next / previous preset |
| **FUNCTION + VOL + / -** | root up / down a semitone |
| **FUNCTION + TRACK 1-4** | chord: FIFTHS, MINOR, MAJOR, SUS |
| **FUNCTION**, hold 1.5 s | power on |
| **FUNCTION**, hold 2.5 s alone | power off |
| **TRACK 1 + TRACK 4**, hold 3 s | restart into the bootloader, for flashing |

**FUNCTION** works like a shift key: hold it, then press the other button. A FUNCTION hold that
was used with another button never powers the SP-1 off.

## The pages

| | Fader 1 | Fader 2 | Fader 3 | Fader 4 |
|---|---|---|---|---|
| **Page 1 · MIX** | CHOIR | PARTIALS | BREATH | BASS |
| **Page 2 · TONE** | TIMBRE | MOTION | TIDE | WARMTH |
| **Page 3 · SHAPE** | SPARKLE | DETUNE | EVOLVE | RELEASE |

- **CHOIR, PARTIALS, BREATH:** each layer's level.
- **BASS:** the bass's level.
- **TIMBRE:** all three layers darker (down) or brighter (up).
- **MOTION:** how far the drift wanders: the choir's tuning and filter, the partials' levels and
  tuning, the voices' places in the stereo field.
- **TIDE:** a slow swell of brightness across the whole drone, from minutes long to seconds,
  as the preset has it.
- **WARMTH:** soft saturation.
- **SPARKLE:** how often a glint strikes; down is none.
- **DETUNE:** how far apart the choir's three saws are, up to 40 cents.
- **EVOLVE:** how much the partials' levels wander beyond MOTION, and how fast.
- **RELEASE:** how long a voice takes to fade when you let it go, from 50 ms to a minute.

## Pages and pick-up

After a page change or a preset change, a setting keeps its value until its fader reaches it (or
sweeps past it), so nothing jumps. For 1.5 seconds the track lights show the page's four values,
and the ones still waiting for their fader blink.

## Volume

**VOL +** and **VOL -** change the output volume of the jack and the speaker in 3 dB steps, from
0 dB down to -60 dB. SP-1Drone starts at -6 dB. For a second after a change the status lights show
the volume. USB audio is not affected: it carries the drone at its own level (peaks around -4 dBFS
at most), and the computer or phone sets its recording level.

## Outputs: jack, USB-C and speaker

The drone plays on every output at once; nothing to choose:

- **3.5 mm jack:** always. This is the output for your effects or headphones.
- **USB-C:** connected to a computer, phone or tablet, the SP-1 shows up as a sound source (an
  audio input) called **SP-1Drone**: 48 kHz, stereo, 16-bit, class compliant, so no driver is
  needed. It streams whenever an app records from it, and stops when the app closes it. The jack
  keeps playing meanwhile.
- **Speaker:** only when nothing is in the jack and no computer or phone is connected over USB-C.
  A USB charger does not turn it off.

# Lights

**Track lights** show each voice's level as it swells and fades; a lit voice never goes fully
dark. For 1.5 seconds after a page or preset change they show the page's settings instead.

**Status lights** normally glow with the output's level, the glints flashing over it. For a moment
after a change they show:

- the page (1, 2 or 3 lights) for 0.8 s after >> or <<;
- the volume for 1 s after VOL + or VOL -;
- the preset as a bar (36 steps across the four lights, the last one dimmer for the part between)
  for 1 s after FUNCTION + >> or <<;
- the root as a bar (D2 to D4) for 1 s after FUNCTION + VOL;
- the chord (one light: FIFTHS, MINOR, MAJOR, SUS) for 1 s after FUNCTION + TRACK.

# Power and charging

- **Power on:** hold **FUNCTION** for 1.5 seconds. The status lights fill up as you hold.
- **Power off:** hold **FUNCTION** alone for 2.5 seconds.
- **Charging:** while the SP-1 is off and plugged in, the status lights show the battery level,
  with the top lit light blinking while it charges.
- **Settings:** nothing is stored. Each time you power on, SP-1Drone starts silent, on MIST,
  FIFTHS on D, page 1, with the volume at -6 dB.

If the firmware ever hangs, a watchdog restarts it within 4 seconds and it comes straight back
up.

# Flashing and getting back

The SP-1 is flashed with the [Solderless updater](https://solderless.engineering) in Chrome or
Edge. Download `sp1drone.bin` from the release page first.

1. Connect the SP-1 to the computer with a USB-C cable.
2. Put it in firmware-loading mode: **hold TRACK 1 + TRACK 4 while you plug in USB**. This is the
   bootloader's own check, so it works whatever firmware is on the SP-1. With SP-1Drone (or
   SP-1FX, or the tape looper) already running, you can instead hold TRACK 1 + TRACK 4 for 3
   seconds. The four track lights come on.
3. In the updater, choose `sp1drone.bin` and flash it.
4. Unplug the SP-1 and plug it back in.

**Going back** to other firmware uses the same steps with that firmware's `.bin` file. The SP-1
has no reset button: if it won't start at all, the plug-in hold in step 2 still reaches the
bootloader.

# Troubleshooting

| Problem | Try |
|---|---|
| No sound | Press PLAY or a TRACK button; check the volume (VOL +); check CHOIR, PARTIALS and BREATH aren't all down on page 1. |
| No sound from the speaker | It is off while something is in the jack or a computer or phone is connected over USB-C. |
| The computer doesn't list SP-1Drone as an input | Use a USB-C cable that carries data, not a charge-only one. Power the SP-1 on with FUNCTION. |
| A fader doesn't respond | It is waiting to pick up after a page or preset change: move it to where its value is, or sweep it past. |
| The SP-1 powered off while I used FUNCTION | It powers off only on a FUNCTION hold used alone; press the other button while still holding FUNCTION. |
| A voice sounds thinner for a while | The SP-1 ran short of time and faded out a partial or two (see `shed=` below); they come back on their own. |
| Crackles or dropouts | Send the status line (below). |
| It won't start | Hold TRACK 1 + TRACK 4 while plugging in USB and reflash. |

## The status line

For a detailed report, connect the SP-1 with USB-C and open its USB serial port in any serial
terminal (any baud rate). It prints one line a second:

| Field | Meaning |
|---|---|
| `f=` | fader readings |
| `page= preset= root= chord= voices=` | what is set and which voices are on |
| `vol=` | the volume, dB |
| `blk=last/max us` | how long one 5.3 ms audio block takes; must stay under 5333 |
| `over=` | blocks that took too long (each a possible click) |
| `shed=level/count` | partials faded out to save time (0: none), and how often that happened |
| `hp=` | headphones detected |
| `usb=host/stream` | a USB host connected, and recording |
| `fill= under= uover=` | the USB audio buffer (frames), and how often it ran dry or overflowed |
| `flt=` | a crash from the previous boot, if any |

If something is wrong on the hardware, this line is the thing to send with your report.

# Specifications

| | |
|---|---|
| Audio out | 3.5 mm jack, 48 kHz / 24-bit, stereo; at once USB-C, class-compliant USB audio input (UAC2), 48 kHz / 16-bit, stereo; the speaker when neither is in use |
| Synthesis | 32-bit floating point at 24 kHz, upsampled to 48 kHz |
| Voices | 4 drone voices × 3 layers (CHOIR, PARTIALS, BREATH), a bass, 3 glints |
| Presets | FRASIL's 36 drone presets |
| Effects | none: dry, for your own; a soft limit protects against overs |
| Volume | 0 to -60 dB in 3 dB steps |
| CPU | about 71% of the 64 MHz Cortex-M4 at most (estimated; when a block runs late, partials are faded out until it fits) |
| RAM | about 50 KB of 256 KB |

# Credits

- [FRASIL](https://github.com/styvesgoupil/frasil) by Styves Goupil (MPL-2.0): the sound engine
  this firmware's engine is made from (its drone voices, GLINT, the presets).
- [chattock/sp1-tape-looper](https://github.com/chattock/sp1-tape-looper) (MIT): the SP-1 board
  definition and the platform code this firmware follows, through SP-1FX.
- [timknapen/SP-1-dev](https://github.com/timknapen/SP-1-dev): the SP-1's pinout, peripherals and
  bootloader documentation.

SP-1Drone is not made or endorsed by Teenage Engineering, M-VAVE, or the author of FRASIL.
