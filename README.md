# SP-1Drone

Firmware that turns the Teenage Engineering SP-1 into a drone instrument, on the sound engine of
[FRASIL](https://github.com/styvesgoupil/frasil) (Styves Goupil's drone firmware for the M-VAVE
FM-1). Each TRACK button lights a slow, living drone voice; the faders shape them; the result comes
out of the headphone jack dry, made to go through your own effects.

**Firmware:** `sp1drone.bin`, on each [release](../../releases), or from the latest build's
artifacts (`make firmware` to build it).

**Manual:** `SP-1Drone-manual.pdf` on each [release](../../releases) (source:
[`docs/manual/manual.md`](docs/manual/manual.md))

> **Not yet run on a real SP-1.** It builds, the engine passes its tests, and the platform code
> follows firmware that does run on the SP-1 (see Credits), but this exact binary has not been on
> hardware. Before flashing, read [Flashing and getting back](#flashing-and-getting-back).

## The sound

- **Four voices**, one per TRACK button, playing a chord over a root (FIFTHS, MINOR, MAJOR or SUS,
  root D2 to D4). Each is FRASIL's three layers:
  - **CHOIR:** three detuned saws or pulses through a filter that follows the note;
  - **PARTIALS:** six additive harmonics, each drifting on its own;
  - **BREATH:** noise or grains through three resonators tuned to the note.
- **Drift and tide:** every voice wanders slowly in tuning, timbre and place; a tide swells the
  brightness over minutes or seconds.
- **BASS** an octave under the root; **SPARKLE** strikes FRASIL's icy **GLINT** FM bells on the
  notes the voices hold.
- **FRASIL's 36 drone presets** (MIST, CATHEDRAL, GLACIER ... PRISM).
- **Dry.** FRASIL's reverb is left out: plug the headphone jack into your effects.

## Controls

| | |
|---|---|
| TRACK 1-4 | that voice on (swells in) or off (fades out) |
| PLAY | every voice off (remembered), or back on |
| Faders 1-4 | the page's four settings (pick-up after a page or preset change) |
| FWD (>>) / RWD (<<) | next / previous page |
| VOL + / VOL - | output volume, 3 dB steps |
| FUNCTION + FWD / RWD | next / previous preset |
| FUNCTION + VOL + / - | root up / down a semitone |
| FUNCTION + TRACK 1-4 | chord: FIFTHS, MINOR, MAJOR, SUS |
| FUNCTION, hold 1.5 s | power on |
| FUNCTION, hold 2.5 s (alone) | power off |
| TRACK 1 + TRACK 4, hold 3 s | restart into the bootloader, for flashing |

| Page | Fader 1 | Fader 2 | Fader 3 | Fader 4 |
|---|---|---|---|---|
| 1 MIX | CHOIR | PARTIALS | BREATH | BASS |
| 2 TONE | TIMBRE | MOTION | TIDE | WARMTH |
| 3 SHAPE | SPARKLE | DETUNE | EVOLVE | RELEASE |

Lights: the track lights follow each voice's level; for 1.5 s after a page or preset change they
show the page's settings, blinking until their faders pick them up. The status lights glow with
the output, the glints flashing over it, and for a moment after a change show the page, the
volume, the preset, the root or the chord. Plug a cable into the headphone jack and the speaker
turns off. Nothing is stored: it starts on MIST, FIFTHS on D, page 1, volume -6 dB.

## Flashing and getting back

The SP-1 is flashed with the [Solderless updater](https://solderless.engineering) in Chrome or Edge:

1. Connect the SP-1 with a USB-C cable.
2. Put it in firmware-loading mode: **hold TRACK 1 + TRACK 4 while you plug in USB** (the
   bootloader's own check, so it works whatever firmware is on it), or, with SP-1Drone running,
   hold them for 3 seconds. The four track lights come on.
3. Choose `sp1drone.bin`, flash, unplug, plug back in.

To go back to other firmware, the same steps with that firmware's `.bin`. If anything ever hangs,
the watchdog restarts it within 4 seconds; if it won't start at all, step 2's plug-in hold still
reaches the bootloader.

## Status line

Open the SP-1's USB serial port (any baud rate) for a line a second: the faders, the page, preset,
root, chord and voices, the volume, `blk=` the time one 5.3 ms audio block takes (the CPU check:
it must stay under 5333 us), `over=` blocks that ran late, `shed=` partials faded out to make time,
headphones, and `flt=` a crash from the last boot. If something is wrong on the hardware, this line
is the thing to send.

## How FRASIL fits the SP-1

FRASIL was made for a much faster chip. Unchanged, its engine needs about 2.7 times the SP-1's
64 MHz Cortex-M4 and 350 KB of RAM (the SP-1 has 256 KB). SP-1Drone keeps FRASIL's DSP for the
voices (its CHOIR, PARTIALS and BREATH code, its drift and tide, GLINT's FM bell, its presets) and
makes it fit:

- **4 voices** (FRASIL: 8), one per TRACK button, gliding to new notes when the chord or root changes;
- **6 partials** a voice (FRASIL: 8; 6 is FRASIL's own first step when its CPU runs short);
- **24 kHz** inside, upsampled to the codec's 48 kHz (FRASIL's steps made for 44.1 kHz are made
  over for the rate, as FRASIL does for its own rates);
- **no reverb**, MIDI, sequencers, LFOs or master effects; a simple sine bass in place of FRASIL's
  eight bass models; GLINT as three voices of its ICE preset, struck by SPARKLE;
- **FRASIL's shedding:** when an audio block runs late, the firmware fades out a partial (down to
  three) and brings them back when there is time again, so it never drops audio.

## CPU and memory

- RAM: about 40 KB of 256 KB. Flash: 115 KB.
- CPU, worst case (all four voices, every fader up, the heaviest preset): about 71% of the 64 MHz
  Cortex-M4, estimated by `tools/bench_m4.py`, which runs the engine built for the M4 in an
  emulator and counts cycles from the M4's instruction timings, preset by preset. The estimate
  leaves out cache misses and interrupts; the real figure is the status line's `blk=`, and the
  shedding covers the difference. CI fails the build above 75%.

## Building

```bash
make test        # the engine on your computer: checks, and WAVs of every preset in build/test
make firmware    # sp1drone.bin; needs ZEPHYR_BASE (Zephyr v4.3.1) and the Zephyr SDK 0.17.4
make bench       # the Cortex-M4 cycle estimate (pip install unicorn capstone)
./scripts/build-manual.sh   # the user manual, build/manual/SP-1Drone-manual.pdf (pandoc + Chrome)
```

`.github/workflows/build.yml` does all of these on every push.

| Path | |
|---|---|
| `engine/` | the sound (MPL-2.0, from FRASIL): `drone.c` the voices, GLINT, the bass, the mix; `sp1drone.c` the brain (pages, presets, chords) |
| `test/` | host checks (silence, levels, every preset, glides without clicks, shedding, block-size independence) and renders |
| `firmware/` | the Zephyr application: I2S, codecs, faders, buttons, lights, power, USB serial |
| `boards/` | the SP-1 board definition |
| `tools/` | the CPU estimate |
| `docs/manual/` | the user manual (Markdown); CI builds the PDF and attaches it to every release |

## Licence and credits

- `engine/` is MPL-2.0 (`engine/LICENSE-MPL-2.0`): it is made from
  [FRASIL](https://github.com/styvesgoupil/frasil)'s engine, Copyright (C) 2026 Styves Goupil
  (`engine/fastmath.h`: fm1-base's, Copyright (C) 2026 Charles Vestal). Each file says what it
  comes from and what changed. SP-1Drone is a fork with its own name, as FRASIL asks; it does not
  use FRASIL's name for itself or its logo.
- The SP-1 board definition and platform code follow
  [chattock/sp1-tape-looper](https://github.com/chattock/sp1-tape-looper) (MIT,
  `firmware/LICENSE-sp1-tape-looper`), through SP-1FX.
- [timknapen/SP-1-dev](https://github.com/timknapen/SP-1-dev): the SP-1's pinout, peripherals and
  bootloader documentation.

SP-1Drone is not made or endorsed by Teenage Engineering, M-VAVE, or the author of FRASIL.
