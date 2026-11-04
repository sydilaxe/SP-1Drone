cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sp1drone)
include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)
target_sources(app PRIVATE src/main.c ../engine/drone.c ../engine/sp1drone.c)
target_include_directories(app PRIVATE ../engine)
# The engine is float math in the audio thread: built for speed, as tools/bench_m4.py measures it. The rest stays -Os.
# -ffp-contract=off: on the Cortex-M4 a fused VFMA takes 3 cycles, VMUL + VADD take 2.
set_source_files_properties(../engine/drone.c ../engine/sp1drone.c PROPERTIES COMPILE_OPTIONS
    "-O2;-funroll-loops;-fno-math-errno;-ffp-contract=off;-fno-strict-aliasing")
