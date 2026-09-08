#!/usr/bin/env bash
# Copyright (c) 2026 MCRI. All rights reserved.
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# check.sh -- the "where am I?" button.
#
#   ./scripts/check.sh                     everything that needs no hardware
#   ./scripts/check.sh --port /dev/cu.usbmodemXXXX   also the on-board checks
#
# Prints one row per milestone. Rows BELOW the milestone you are working on
# should pass; rows at or above it are expected to fail, and that is what
# progress looks like. Nothing here is clever: it runs the same commands you
# could type yourself, and shows them when they fail so you can.
#
# Requires the toolchain on PATH:  source scripts/ncs-env.sh

set -uo pipefail # deliberately NOT -e: a failing check must not stop the run

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BUILD_DIR="$ROOT/build-dongle"
BOARD="xiao_ble/nrf52840"
PORT=""
PY="${PYTHON:-python3}"

while [ $# -gt 0 ]; do
	case "$1" in
	--port)
		PORT="$2"
		shift 2
		;;
	-h | --help)
		sed -n '5,15p' "$0"
		exit 0
		;;
	*)
		echo "unknown argument: $1" >&2
		exit 2
		;;
	esac
done

pass=0
fail=0
skip=0

row() { # row <id> <verdict> <detail>
	printf '  %-4s %-6s %s\n' "$1" "$2" "$3"
	case "$2" in
	PASS) pass=$((pass + 1)) ;;
	FAIL) fail=$((fail + 1)) ;;
	SKIP) skip=$((skip + 1)) ;;
	esac
}

echo
echo "CBPM dongle checks"
echo "=================="
echo

# --- M0: does it build? ------------------------------------------------------
if ! command -v west >/dev/null 2>&1; then
	row "M0" "SKIP" "west not on PATH -- run: source scripts/ncs-env.sh"
else
	if west build -b "$BOARD" -d "$BUILD_DIR" "$ROOT" >"$ROOT/.check-build.log" 2>&1; then
		size=$(wc -c <"$BUILD_DIR/zephyr/zephyr.hex" 2>/dev/null || echo 0)
		row "M0" "PASS" "builds clean (zephyr.hex, $size bytes)"
	else
		row "M0" "FAIL" "build failed -- see .check-build.log"
		tail -n 15 "$ROOT/.check-build.log" | sed 's/^/        /'
	fi
fi

# --- probe self-test: does the host tool agree with the reference? -----------
if $PY "$ROOT/tools/dongle_probe.py" --selftest >/dev/null 2>&1; then
	row "host" "PASS" "dongle_probe.py agrees with the reference framing"
else
	row "host" "FAIL" "python tools/dongle_probe.py --selftest"
fi

# --- M2: the unit tests ------------------------------------------------------
if ! command -v west >/dev/null 2>&1; then
	row "M2" "SKIP" "needs west"
elif west twister -T "$ROOT/tests" --platform unit_testing --inline-logs \
	-O "$ROOT/twister-out" >"$ROOT/.check-twister.log" 2>&1; then
	row "M2" "PASS" "verb framing unit tests green"
else
	if grep -qiE "no such file|cannot find -l|unrecognized command|host compiler" \
		"$ROOT/.check-twister.log"; then
		row "M2" "SKIP" "no usable host compiler for unit_testing (see docs/hardware.md)"
	else
		row "M2" "FAIL" "west twister -T tests --platform unit_testing --inline-logs"
	fi
fi

# --- hardware rows -----------------------------------------------------------
if [ -z "$PORT" ]; then
	row "M1" "SKIP" "pass --port to run the on-board checks"
	row "M3" "SKIP" "manual: dongle_probe.py --loopback 65536 (needs a D6-D7 jumper)"
	row "M4" "SKIP" "manual: dongle_probe.py --flood"
	row "M5" "SKIP" "manual: dongle_probe.py --status, with a Roo powered nearby"
	row "M6" "SKIP" "manual: dongle_probe.py --wedge"
else
	if $PY "$ROOT/tools/dongle_probe.py" --port "$PORT" --echo >/dev/null 2>&1; then
		row "M1" "PASS" "CDC echo round trip"
	else
		row "M1" "FAIL" "dongle_probe.py --port $PORT --echo"
	fi

	if $PY "$ROOT/tools/dongle_probe.py" --port "$PORT" --status >/dev/null 2>&1; then
		row "M2" "PASS" "device answered DONGLE_GET_STATUS"
	else
		row "M2" "FAIL" "dongle_probe.py --port $PORT --status"
	fi

	row "M3" "SKIP" "manual: dongle_probe.py --port $PORT --loopback 65536"
	row "M4" "SKIP" "manual: dongle_probe.py --port $PORT --flood"
	row "M5" "SKIP" "manual: dongle_probe.py --port $PORT --status (Roo powered)"
	row "M6" "SKIP" "manual: dongle_probe.py --port $PORT --wedge"
fi

echo
echo "  $pass passed, $fail failed, $skip skipped"
echo
echo "  Next: docs/MILESTONES.md"
echo

[ "$fail" -eq 0 ]
