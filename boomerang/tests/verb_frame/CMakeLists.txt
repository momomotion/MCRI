# Copyright (c) 2026 MCRI. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# This software is the confidential and proprietary information of
# MCRI. It must not be used, copied, redistributed or disclosed
# without prior written authorisation.

# Unit test for src/ctrl/verb_frame.c.
#
# The `unit_testing` platform compiles the code under test for your LAPTOP, not
# for the board: no Zephyr kernel, no drivers, just the C file and the test
# harness linked into an ordinary executable. That is only possible because
# verb_frame.c has no Zephyr dependencies, which is exactly why it was written
# that way. Pure logic is cheap to test; code entangled with hardware is not.
#
# Note the target here is `testbinary`, not `app`. That is the unit_testing
# platform's convention.

cmake_minimum_required(VERSION 3.20.0)

# COMPONENTS unittest is what selects the host build. Without it you get a
# normal Zephyr application build and the `testbinary` target does not exist.
find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(verb_frame_test)

target_sources(testbinary PRIVATE
	src/main.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/ctrl/verb_frame.c
)

target_include_directories(testbinary PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
