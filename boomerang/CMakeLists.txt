# Copyright (c) 2026 MCRI. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# This software is the confidential and proprietary information of
# MCRI. It must not be used, copied, redistributed or disclosed
# without prior written authorisation.

# Freestanding Zephyr application: it is built against an INSTALLED nRF Connect
# SDK rather than living inside a west workspace of its own. `find_package`
# below locates that SDK through the ZEPHYR_BASE environment variable, which
# scripts/ncs-env.sh (or .ps1) exports. That is the entire reason you have to
# source the activation script before building.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cbpm_dongle LANGUAGES C VERSION 0.1.0)

# `app` is the CMake target Zephyr creates for your application code. Adding a
# new .c file means adding it here -- there is no automatic globbing, on
# purpose: a build should never change because a stray file appeared.
target_sources(app PRIVATE
	src/main.c
	src/cdc/cdc_link.c
	src/uart/uart_link.c
	src/bridge/bridge.c
	src/ctrl/verb_frame.c
	src/ctrl/dongle_ctrl.c
	src/sys/sys_health.c
)

# The BLE path is compiled in only when CONFIG_CBPM_DONGLE_BLE is set (see
# Kconfig). Until milestone M5 you build without the Bluetooth stack, which
# keeps the image small and the build fast. `target_sources_ifdef` is Zephyr's
# helper for exactly this.
target_sources_ifdef(CONFIG_CBPM_DONGLE_BLE app PRIVATE src/ble/ble_central.c)

# So that #include "ctrl/verb_frame.h" works from anywhere in src/.
target_include_directories(app PRIVATE src)
