#!/usr/bin/env bash
# Copyright (c) 2026 MCRI. All rights reserved.
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# flash.sh -- build, flash over SWD, and leave an RTT log running.
#
#   ./scripts/flash.sh                 build and flash, then stream RTT
#   ./scripts/flash.sh --no-build      flash what is already built
#   ./scripts/flash.sh --no-rtt        flash only, no logger
#
# Requires the toolchain on PATH:  source scripts/ncs-env.sh
#
# WHY THERE IS A SCRIPT AT ALL, rather than just `west flash`:
#
#  1. The J-Link probe is a single shared resource. An RTT logger left running
#     holds it, and the next flash fails with "J-Link busy". This frees it
#     first, every time, so that failure simply cannot happen to you.
#
#  2. `west flash --erase` must never run on this board. The XIAO carries the
#     Adafruit UF2 bootloader in the first 0x27000 bytes of flash, plus the
#     MBR and the UICR configuration; a chip erase takes all of it, and the
#     board then has no bootloader, no USB, and no way back except reflashing
#     the bootloader over SWD. This script refuses the flag.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BUILD_DIR="$ROOT/build-dongle"
BOARD="xiao_ble/nrf52840"
RTT_DIR="$ROOT/.rtt"

DO_BUILD=1
DO_RTT=1

for arg in "$@"; do
	case "$arg" in
	--no-build) DO_BUILD=0 ;;
	--no-rtt) DO_RTT=0 ;;
	--erase | --recover)
		echo "REFUSED: $arg would erase the UF2 bootloader and the UICR." >&2
		echo "There is no situation in this project that needs it. If you" >&2
		echo "genuinely have a bricked board, ask before running it." >&2
		exit 2
		;;
	-h | --help)
		sed -n '5,12p' "$0"
		exit 0
		;;
	*)
		echo "unknown argument: $arg" >&2
		exit 2
		;;
	esac
done

if ! command -v west >/dev/null 2>&1; then
	echo "west is not on PATH. Run:  source scripts/ncs-env.sh" >&2
	exit 1
fi

# Free the probe: any RTT logger still holding it would block the flash.
pkill -f JLinkRTTLogger >/dev/null 2>&1 || true

if [ "$DO_BUILD" -eq 1 ]; then
	echo "== building =="
	west build -b "$BOARD" -d "$BUILD_DIR" "$ROOT"
fi

echo "== flashing =="
west flash -d "$BUILD_DIR" -r jlink

if [ "$DO_RTT" -eq 0 ]; then
	exit 0
fi

mkdir -p "$RTT_DIR"
LOG="$RTT_DIR/rtt-$(date +%Y%m%d-%H%M%S).log"

if command -v JLinkRTTLogger >/dev/null 2>&1; then
	echo "== RTT log: $LOG =="
	echo "   (Ctrl-C to stop; the log keeps its contents)"
	JLinkRTTLogger -Device NRF52840_XXAA -If SWD -Speed 4000 -RTTChannel 0 "$LOG"
else
	echo "JLinkRTTLogger not found. Install the SEGGER J-Link Software and" >&2
	echo "Documentation Pack, or read logs with: JLinkRTTClient" >&2
fi
