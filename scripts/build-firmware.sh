#!/bin/sh
# Build the SP-1Drone firmware: build/firmware/zephyr/zephyr.bin -> sp1drone.bin
#   ZEPHYR_BASE             a Zephyr v4.3.1 tree (west workspace with hal_nordic, cmsis_6)
#   ZEPHYR_SDK_INSTALL_DIR  Zephyr SDK 0.17.4 with the arm-zephyr-eabi toolchain
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
: "${ZEPHYR_BASE:?set ZEPHYR_BASE to a Zephyr v4.3.1 tree}"
# the UAC2 class as the SP-1 runs it (16- and 24-bit streams), as SP-1FX's; see firmware/zephyr-uac2-backport
for f in usbd_uac2.c usbd_uac2_macros.h Kconfig.uac2; do
	cp "$here/firmware/zephyr-uac2-backport/$f" "$ZEPHYR_BASE/subsys/usb/device_next/class/$f"
done
west build -p always -b stem_player -d "$here/build/firmware" "$here/firmware" -- -DBOARD_ROOT="$here"
cp "$here/build/firmware/zephyr/zephyr.bin" "$here/sp1drone.bin"
ls -l "$here/sp1drone.bin"
