# Copyright (c) 2026 MCRI. All rights reserved.
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# NCS environment activation for macOS / Linux shells (bash or zsh).
# The companion to scripts/ncs-env.ps1: sets PATH and the ZEPHYR_* env vars
# the nRF Connect SDK / Zephyr build needs, so west/cmake/ninja/arm-zephyr-eabi
# work from a plain terminal without the VS Code extension running.
#
# Source it -- do not execute it:
#   source scripts/ncs-env.sh
#   source scripts/ncs-env.sh && west build --sysbuild -p always -d build-joey
#
# The activation applies to the current shell only; a new terminal needs its own.
#
# Layout: the nRF Connect for VS Code extension installs the SDK under
# /opt/nordic/ncs/<version> and the matching toolchain bundle under
# /opt/nordic/ncs/toolchains/<bundle-id>. The bundle id is platform-specific
# (b8b84efebd on the Windows bench, 5c0d382932 on macOS), so it is resolved from
# toolchains.json rather than hardcoded the way the PowerShell version can.
#
# Overrides, if your install differs:
#   NCS_ROOT        install root            (default /opt/nordic/ncs)
#   NCS_VERSION     SDK version directory   (default v3.1.0, tracks west.yml)
#   NCS_TOOLCHAIN   toolchain bundle dir    (default: resolved from NCS_VERSION)
#
# Python note: the toolchain ships its own Python and this script puts it on
# PATH ahead of yours, which is what the Zephyr build expects. If a venv is
# already active when you source this, its bin is re-prepended so `python`
# stays yours -- so either order works in a fresh shell.
#
# THE ONE ORDER THAT BREAKS: sourcing `.venv/bin/activate` again when the venv
# is ALREADY active, after sourcing this file. The venv's activate script does
# `deactivate nondestructive` first, which restores PATH from the
# `_OLD_VIRTUAL_PATH` snapshot taken at the FIRST activation -- before this
# file ran -- so every toolchain entry is silently discarded and you get
# `zsh: command not found: west` despite having just activated NCS. Only
# activate the venv when `(.venv)` is NOT already in your prompt; if it is,
# just source this file. Re-sourcing this file always repairs PATH (the guard
# below makes that safe to do any number of times).

# --- refuse to run as a subprocess (the env would vanish on exit) -------------
_ncs_sourced=0
if [ -n "${ZSH_VERSION:-}" ]; then
    # sourcing pushes "file" onto the context ("cmdarg:file", "toplevel:file");
    # running the script as an argument leaves a bare "toplevel".
    case "${ZSH_EVAL_CONTEXT:-}" in *file*) _ncs_sourced=1 ;; esac
elif [ -n "${BASH_VERSION:-}" ]; then
    [ "${BASH_SOURCE[0]}" != "$0" ] && _ncs_sourced=1
else
    _ncs_sourced=1   # unknown shell; assume the caller knows to source
fi
if [ "$_ncs_sourced" -eq 0 ]; then
    echo "ncs-env.sh sets environment variables, so it must be sourced:" >&2
    echo "    source scripts/ncs-env.sh" >&2
    unset _ncs_sourced
    exit 1
fi
unset _ncs_sourced

# --- resolve the install ------------------------------------------------------
_ncs_root="${NCS_ROOT:-/opt/nordic/ncs}"
_ncs_version="${NCS_VERSION:-v3.1.0}"
_ncs_zephyr="$_ncs_root/$_ncs_version/zephyr"

# environment.json in each bundle lists the PATH entries Nordic prepends. Read
# it so a toolchain update that adds a directory does not silently drop off.
# Newline-separated, not space: zsh does not word-split unquoted parameters, so
# a space-separated list would silently iterate once over the whole string.
_ncs_pathdirs_default='bin
usr/bin
usr/local/bin
opt/bin
opt/nanopb/generator-bin
nrfutil/bin
opt/zephyr-sdk/arm-zephyr-eabi/bin
opt/zephyr-sdk/riscv64-zephyr-elf/bin'

_ncs_toolchain="${NCS_TOOLCHAIN:-}"
_ncs_pathdirs=""

if command -v python3 >/dev/null 2>&1; then
    _ncs_probe="$(python3 - "$_ncs_root" "$_ncs_version" "$_ncs_toolchain" <<'PY' 2>/dev/null
import json, os, sys

root, version, forced = sys.argv[1], sys.argv[2], sys.argv[3]
tc = forced

if not tc:
    # toolchains.json is a list of groups, each with a "toolchains" array whose
    # entries map identifier.bundle_id -> the ncs_versions it can build.
    try:
        with open(os.path.join(root, "toolchains", "toolchains.json")) as fh:
            doc = json.load(fh)
    except Exception:
        doc = []
    for group in (doc if isinstance(doc, list) else [doc]):
        for entry in group.get("toolchains", []):
            if version in entry.get("ncs_versions", []):
                bundle = entry.get("identifier", {}).get("bundle_id")
                if bundle:
                    tc = os.path.join(root, "toolchains", bundle)
                    break
        if tc:
            break

if not tc:
    # Fallback: exactly one bundle installed means there is no ambiguity.
    try:
        cand = [d for d in os.scandir(os.path.join(root, "toolchains")) if d.is_dir()]
    except OSError:
        cand = []
    if len(cand) == 1:
        tc = cand[0].path

if not tc:
    sys.exit(1)

dirs = []
try:
    with open(os.path.join(tc, "environment.json")) as fh:
        for var in json.load(fh).get("env_vars", []):
            if var.get("key") == "PATH":
                dirs = [d for d in var.get("values", []) if d]
except Exception:
    pass

# Line 1 is the bundle dir; every later line is one PATH entry.
print(tc)
for d in dirs:
    print(d)
PY
)"
    if [ -n "$_ncs_probe" ]; then
        _ncs_toolchain="$(printf '%s\n' "$_ncs_probe" | sed -n 1p)"
        _ncs_pathdirs="$(printf '%s\n' "$_ncs_probe" | sed -n '2,$p')"
    fi
    unset _ncs_probe
fi

if [ -z "$_ncs_toolchain" ]; then
    # No python3, or the JSON did not parse: fall back to a lone bundle dir.
    for _ncs_cand in "$_ncs_root"/toolchains/*/; do
        [ -d "$_ncs_cand" ] || continue
        if [ -n "$_ncs_toolchain" ]; then
            _ncs_toolchain=""      # ambiguous, force the user to pick
            break
        fi
        _ncs_toolchain="${_ncs_cand%/}"
    done
    unset _ncs_cand
fi
[ -n "$_ncs_pathdirs" ] || _ncs_pathdirs="$_ncs_pathdirs_default"

# --- guards -------------------------------------------------------------------
if [ -z "$_ncs_toolchain" ] || [ ! -x "$_ncs_toolchain/bin/west" ]; then
    echo "ncs-env.sh: no NCS toolchain for $_ncs_version under $_ncs_root/toolchains" >&2
    echo "  Install it from the nRF Connect for VS Code extension (Manage toolchains)," >&2
    echo "  or set NCS_ROOT / NCS_VERSION / NCS_TOOLCHAIN to match your install." >&2
    unset _ncs_root _ncs_version _ncs_zephyr _ncs_toolchain _ncs_pathdirs _ncs_pathdirs_default
    return 1
fi
if [ ! -d "$_ncs_zephyr" ]; then
    echo "ncs-env.sh: zephyr source tree not found at $_ncs_zephyr" >&2
    echo "  The SDK for $_ncs_version is not installed under $_ncs_root." >&2
    unset _ncs_root _ncs_version _ncs_zephyr _ncs_toolchain _ncs_pathdirs _ncs_pathdirs_default
    return 1
fi

# --- apply --------------------------------------------------------------------
# Build the prefix in manifest order, then prepend once, so PATH ends up in the
# same order the extension's own environment uses. Entries already present are
# skipped, which makes re-sourcing idempotent -- worth having, because
# re-sourcing is exactly the cure for a venv re-activation having wiped these
# (see the Python note in the header) and PATH should not grow each time.
_ncs_prefix=""
while IFS= read -r _ncs_dir; do
    [ -n "$_ncs_dir" ] || continue
    [ -d "$_ncs_toolchain/$_ncs_dir" ] || continue
    case ":$PATH:" in
        *":$_ncs_toolchain/$_ncs_dir:"*) continue ;;   # already there
    esac
    _ncs_prefix="$_ncs_prefix$_ncs_toolchain/$_ncs_dir:"
done <<EOF
$_ncs_pathdirs
EOF
PATH="$_ncs_prefix$PATH"
unset _ncs_dir _ncs_prefix

# Do NOT put a venv on PATH here, and warn if one is active: an activated venv
# breaks `west build`.
#
# The mechanism is not PATH order -- it is CMake. Zephyr's per-image CMake calls
# find_package(Python3), and CMake's default Python3_FIND_VIRTUALENV=FIRST makes
# it prefer $VIRTUAL_ENV over everything else, whatever PATH says. Verified
# 2026-08-17 with a minimal project: with VIRTUAL_ENV set it picks the venv even
# with the toolchain's bin first on PATH; unset, it does not. The bench venv has
# none of Zephyr's build-time imports, so the sub-image configure dies with:
#
#   -- Found Python3: /path/to/.venv/bin/python (3.14.7)
#   ModuleNotFoundError: No module named 'pykwalify'
#
# The top-level build survives because west passes -DWEST_PYTHON explicitly;
# mcuboot is where it surfaces. An earlier revision of this script re-prepended
# $VIRTUAL_ENV/bin to "keep python yours" -- that neither caused nor could fix
# this, and is deliberately gone.
#
# So: `deactivate` before building. The host bench scripts do not need the venv
# on PATH at all -- call its interpreter by path:
#
#   .venv/bin/python scripts/host_decoder.py --selftest
#   .venv/bin/python .rtt/drain_probe.py /dev/cu.usbmodemXXXX
#
# which also means one terminal can do both.
if [ -n "${VIRTUAL_ENV:-}" ]; then
    echo "WARNING: a venv is active ($VIRTUAL_ENV)." >&2
    echo "  west build will FAIL: CMake prefers \$VIRTUAL_ENV for find_package(Python3)," >&2
    echo "  and that interpreter lacks Zephyr's build deps (pykwalify, ...). Run:" >&2
    echo "      deactivate && source scripts/ncs-env.sh" >&2
    echo "  Bench scripts do not need it activated: .venv/bin/python <script>" >&2
fi

export PATH
export ZEPHYR_BASE="$_ncs_zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$_ncs_toolchain/opt/zephyr-sdk"
export NRFUTIL_HOME="$_ncs_toolchain/nrfutil/home"

echo "NCS $_ncs_version activated (toolchain $(basename "$_ncs_toolchain"))"

unset _ncs_root _ncs_version _ncs_zephyr _ncs_toolchain _ncs_pathdirs _ncs_pathdirs_default
