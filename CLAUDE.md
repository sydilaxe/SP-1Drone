# SP-1Drone

Firmware for the Teenage Engineering SP-1 (nRF52840, Cortex-M4F at 64 MHz, 256 KB RAM, Zephyr): FRASIL's drone
engine (github.com/styvesgoupil/frasil) made to fit. `engine/` is MPL-2.0 and comes from FRASIL's `engine/dsp/`:
keep each file's header saying what it comes from and what changed, and keep it MPL-2.0. The platform code in
`firmware/src/main.c` is SP-1FX's (sydilaxe/SP-1FX-dev), after chattock/sp1-tape-looper (MIT); keep it in step with
SP-1FX's when either changes. Never remove the watchdog feeding or the TRACK 1 + 4 path back to the bootloader.

The engine runs at 24 kHz in blocks of 128 (the firmware's 256 at 48 kHz). After any change to `engine/`, run
`make test` (CI greps its checks) and `make bench` (CI fails the worst preset above 75% of the M4); the firmware
sheds partials (`drone_shed`) when a block runs late, but that is the safety net, not the budget.

# Keeping the docs in step

The user manual is `docs/manual/manual.md`. CI turns it into `SP-1Drone-manual.pdf` (`scripts/build-manual.sh`)
and attaches it to every GitHub release, so it must always describe the firmware as it is.

Whenever a change affects what a player sees, hears or does (a control, a light, a preset or setting, a range or
timing, the status line, the flashing steps, the specs such as CPU or RAM), update in the same commit:

- `docs/manual/manual.md`: the full manual, written for players, not developers;
- `README.md`;
- `.github/release-notes.md`;
- the comment at the top of `firmware/src/main.c`.

# Identity

Commits are authored and pushed as `sydilaxe` (that account's own name and email), never re-authored as Claude.
