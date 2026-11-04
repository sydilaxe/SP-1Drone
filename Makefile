# SP-1Drone
#   make test       the engine on the host: checks, and WAVs in build/test
#   make firmware   sp1drone.bin (needs ZEPHYR_BASE and the Zephyr SDK; see README)
#   make bench      Cortex-M4 cycle estimate (needs the SDK's gcc, pip install unicorn capstone)
CC ?= cc
CFLAGS := -O2 -funroll-loops -std=gnu11 -Wall -Wextra -fno-math-errno -ffp-contract=off
ENGINE := engine/drone.c engine/sp1drone.c
HDRS := $(wildcard engine/*.h)

test: build/drone_test
	mkdir -p build/test
	./build/drone_test build/test

build/drone_test: test/drone_test.c $(ENGINE) $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) -Iengine -o $@ test/drone_test.c $(ENGINE) -lm

firmware:
	./scripts/build-firmware.sh

bench:
	python3 tools/bench_m4.py -v

.PHONY: test firmware bench
