SP-1Drone test build for the Teenage Engineering SP-1: a drone instrument on the sound engine of
[FRASIL](https://github.com/styvesgoupil/frasil). Out on the 3.5 mm jack (48 kHz / 24-bit) and at the same time over
USB-C as a class-compliant sound source (48 kHz / 16-bit) a computer or phone records; the speaker only with neither in
use. Dry: made to go through your own effects.

**Not yet tried on a real SP-1.** It builds, the engine passes its tests, and the platform code follows firmware that
runs on the SP-1, but this binary has not been on hardware yet.

**Flash:** open the [Solderless updater](https://solderless.engineering) in Chrome or Edge, hold **TRACK 1 + TRACK 4**
while you plug the SP-1 in (the four track lights come on), choose `sp1drone.bin`, flash, then unplug and plug back
in. The same plug-in hold always gets back to the bootloader, whatever firmware is on it.

**The sound:** four drone voices, one per TRACK button, playing a chord (FIFTHS, MINOR, MAJOR, SUS) over a root (D2 to
D4). Each is FRASIL's three layers: CHOIR (detuned saws or pulses), PARTIALS (drifting harmonics) and BREATH (noise
through resonators). They drift and breathe slowly; a BASS holds the root underneath and SPARKLE strikes icy GLINT
bells over them. FRASIL's 36 drone presets.

**Controls:** TRACK 1-4 light or release a voice, PLAY releases everything or brings it back. Faders on three pages
(>> / <<): MIX (CHOIR, PARTIALS, BREATH, BASS), TONE (TIMBRE, MOTION, TIDE, WARMTH), SHAPE (SPARKLE, DETUNE, EVOLVE,
RELEASE), with pick-up so nothing jumps. Hold FUNCTION and press >> / << for the presets, VOL + / - for the root,
TRACK 1-4 for the chord. VOL + / - set the volume. Hold FUNCTION to power on (1.5 s) or off (2.5 s, alone); hold
TRACK 1 + 4 for 3 s to restart into the bootloader.

**If something is wrong**, open the SP-1's USB serial port: a status line a second shows the faders, what plays, the
audio load (`blk=`, microseconds per 5.3 ms block), partials shed to make time, and the last crash.

**The full user manual** is attached below as `SP-1Drone-manual.pdf`.
