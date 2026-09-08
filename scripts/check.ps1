# Copyright (c) 2026 MCRI. All rights reserved.
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# check.ps1 -- the "where am I?" button. Windows companion to check.sh.
#
#   .\scripts\check.ps1
#   .\scripts\check.ps1 -Port COM7
#
# One row per milestone. Rows below the milestone you are working on should
# pass; rows at or above it are expected to fail.
#
# Requires the toolchain in this shell:  . .\scripts\ncs-env.ps1

[CmdletBinding()]
param(
    [string]$Port = '',
    [string]$Python = 'python'
)

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build-dongle'
$board = 'xiao_ble/nrf52840'
$probe = Join-Path $root 'tools\dongle_probe.py'

$script:pass = 0
$script:fail = 0
$script:skip = 0

function Row([string]$id, [string]$verdict, [string]$detail) {
    '  {0,-4} {1,-6} {2}' -f $id, $verdict, $detail | Write-Host
    if ($verdict -eq 'PASS') { $script:pass++ }
    if ($verdict -eq 'FAIL') { $script:fail++ }
    if ($verdict -eq 'SKIP') { $script:skip++ }
}

Write-Host ''
Write-Host 'CBPM dongle checks'
Write-Host '=================='
Write-Host ''

$haveWest = [bool](Get-Command west -ErrorAction SilentlyContinue)

# --- M0: does it build? ------------------------------------------------------
if (-not $haveWest) {
    Row 'M0' 'SKIP' 'west not on PATH -- run: . .\scripts\ncs-env.ps1'
} else {
    $buildLog = Join-Path $root '.check-build.log'
    west build -b $board -d $buildDir $root *> $buildLog
    if ($LASTEXITCODE -eq 0) {
        $hex = Join-Path $buildDir 'zephyr\zephyr.hex'
        $size = 0
        if (Test-Path $hex) { $size = (Get-Item $hex).Length }
        Row 'M0' 'PASS' "builds clean (zephyr.hex, $size bytes)"
    } else {
        Row 'M0' 'FAIL' 'build failed -- see .check-build.log'
        Get-Content $buildLog -Tail 15 | ForEach-Object { '        ' + $_ }
    }
}

# --- probe self-test ---------------------------------------------------------
& $Python $probe --selftest *> $null
if ($LASTEXITCODE -eq 0) {
    Row 'host' 'PASS' 'dongle_probe.py agrees with the reference framing'
} else {
    Row 'host' 'FAIL' 'python tools\dongle_probe.py --selftest'
}

# --- M2: unit tests ----------------------------------------------------------
if (-not $haveWest) {
    Row 'M2' 'SKIP' 'needs west'
} else {
    $twLog = Join-Path $root '.check-twister.log'
    west twister -T (Join-Path $root 'tests') --platform unit_testing --inline-logs `
        -O (Join-Path $root 'twister-out') *> $twLog
    if ($LASTEXITCODE -eq 0) {
        Row 'M2' 'PASS' 'verb framing unit tests green'
    } else {
        $txt = Get-Content $twLog -Raw
        if ($txt -match 'no such file|cannot find -l|host compiler') {
            Row 'M2' 'SKIP' 'no usable host compiler for unit_testing (see docs\hardware.md)'
        } else {
            Row 'M2' 'FAIL' 'west twister -T tests --platform unit_testing --inline-logs'
        }
    }
}

# --- hardware rows -----------------------------------------------------------
if ($Port -eq '') {
    Row 'M1' 'SKIP' 'pass -Port to run the on-board checks'
    Row 'M3' 'SKIP' 'manual: dongle_probe.py --loopback 65536 (needs a D6-D7 jumper)'
    Row 'M4' 'SKIP' 'manual: dongle_probe.py --flood'
    Row 'M5' 'SKIP' 'manual: dongle_probe.py --status, with a Roo powered nearby'
    Row 'M6' 'SKIP' 'manual: dongle_probe.py --wedge'
} else {
    & $Python $probe --port $Port --echo *> $null
    if ($LASTEXITCODE -eq 0) { Row 'M1' 'PASS' 'CDC echo round trip' }
    else { Row 'M1' 'FAIL' "dongle_probe.py --port $Port --echo" }

    & $Python $probe --port $Port --status *> $null
    if ($LASTEXITCODE -eq 0) { Row 'M2' 'PASS' 'device answered DONGLE_GET_STATUS' }
    else { Row 'M2' 'FAIL' "dongle_probe.py --port $Port --status" }

    Row 'M3' 'SKIP' "manual: dongle_probe.py --port $Port --loopback 65536"
    Row 'M4' 'SKIP' "manual: dongle_probe.py --port $Port --flood"
    Row 'M5' 'SKIP' "manual: dongle_probe.py --port $Port --status (Roo powered)"
    Row 'M6' 'SKIP' "manual: dongle_probe.py --port $Port --wedge"
}

Write-Host ''
Write-Host ("  {0} passed, {1} failed, {2} skipped" -f $script:pass, $script:fail, $script:skip)
Write-Host ''
Write-Host '  Next: docs\MILESTONES.md'
Write-Host ''

if ($script:fail -gt 0) { exit 1 }
exit 0
