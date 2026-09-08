# Copyright (c) 2026 MCRI. All rights reserved.
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# flash.ps1 -- build, flash over SWD, and leave an RTT log running.
# Windows companion to flash.sh; see that file for the reasoning.
#
#   .\scripts\flash.ps1
#   .\scripts\flash.ps1 -NoBuild
#   .\scripts\flash.ps1 -NoRtt
#
# Requires the toolchain in this shell:  . .\scripts\ncs-env.ps1

[CmdletBinding()]
param(
    [switch]$NoBuild,
    [switch]$NoRtt
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build-dongle'
$rttDir = Join-Path $root '.rtt'
$board = 'xiao_ble/nrf52840'

if (-not (Get-Command west -ErrorAction SilentlyContinue)) {
    Write-Error 'west is not on PATH. Run:  . .\scripts\ncs-env.ps1'
}

# Free the J-Link: an RTT logger still holding the probe blocks the flash.
Get-Process JLinkRTTLogger -ErrorAction SilentlyContinue | Stop-Process -Force

if (-not $NoBuild) {
    Write-Host '== building =='
    west build -b $board -d $buildDir $root
    if ($LASTEXITCODE -ne 0) { Write-Error 'build failed' }
}

Write-Host '== flashing =='
# NOTE: never add --erase here. The XIAO carries the Adafruit UF2 bootloader,
# the MBR and the UICR in flash, and a chip erase takes all three; recovering
# needs the bootloader reflashed over SWD.
west flash -d $buildDir -r jlink
if ($LASTEXITCODE -ne 0) { Write-Error 'flash failed' }

if ($NoRtt) { return }

if (-not (Test-Path $rttDir)) { New-Item -ItemType Directory -Path $rttDir | Out-Null }
$log = Join-Path $rttDir ("rtt-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))

if (Get-Command JLinkRTTLogger -ErrorAction SilentlyContinue) {
    Write-Host "== RTT log: $log =="
    Write-Host '   (Ctrl-C to stop; the log keeps its contents)'
    JLinkRTTLogger -Device NRF52840_XXAA -If SWD -Speed 4000 -RTTChannel 0 $log
} else {
    Write-Warning 'JLinkRTTLogger not found. Install the SEGGER J-Link pack.'
}
