#!/usr/bin/env python3
"""Cortex-M4 cycle estimate for SP-1Drone's engine, preset by preset.

Builds tools/bench_m4.c with the Zephyr SDK's arm-zephyr-eabi-gcc as the firmware builds the engine (-O2 -funroll-loops,
-ffp-contract=off, Cortex-M4F hard float) and runs tools/m4cycles.py on it. Prints each preset's estimate and the
worst; with -v, where the worst one's time goes.

  tools/bench_m4.py [-v] [PRESET ...]       (presets by number, 0..35; default: all)
"""
import os, re, subprocess, sys, tempfile

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
gcc = os.environ.get("GCC") or os.path.join(os.environ.get("ZEPHYR_SDK_INSTALL_DIR", ""),
                                            "arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc")
verbose = "-v" in sys.argv
presets = [int(a) for a in sys.argv[1:] if a.lstrip("-").isdigit()] or list(range(36))
tmp = tempfile.mkdtemp()
src = [os.path.join(here, "bench_m4.c")] + [os.path.join(root, "engine", f) for f in ("drone.c", "sp1drone.c")]
env = dict(os.environ, GCC=gcc, WARM="20", BLOCKS="6", FRAMES="256")
worst = (0, None, "")
for p in presets:
    elf = os.path.join(tmp, f"b{p}.elf")
    subprocess.check_call([gcc, "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16", "-O2", "-funroll-loops",
                           "-fno-math-errno", "-ffp-contract=off", "-ffreestanding", "-nostdlib", f"-DBENCH_PRESET={p}",
                           "-Wl,-e,bench_block", "-Wl,-Ttext=0x10000", "-Wl,-Tdata=0x20000000",
                           "-I", os.path.join(root, "engine")] + src + ["-lgcc", "-o", elf])
    out = subprocess.check_output([sys.executable, os.path.join(here, "m4cycles.py"), elf, "-v"], env=env).decode()
    pct = int(re.search(r"\((\d+)% of the 64 MHz M4\)", out).group(1))
    print(f"preset {p:2d}: {pct}%")
    if pct > worst[0]:
        worst = (pct, p, out)
print(f"worst case: preset {worst[1]}: {worst[0]}% of one core")
if verbose:
    print(worst[2])
