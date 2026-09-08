/*
 * Copyright (c) 2026 MCRI. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Proprietary
 *
 * This software is the confidential and proprietary information of
 * MCRI. It must not be used, copied, redistributed or disclosed
 * without prior written authorisation.
 */

/**
 * @file app.h
 * @brief Types shared across the whole dongle application.
 *
 * Everything in here is either (a) a value that appears on the wire and
 * therefore cannot be changed without changing the host too, or (b) a small
 * enum that more than one module needs. Nothing else belongs here: a type used
 * by exactly one module lives in that module's own header.
 */

#ifndef CBPM_DONGLE_APP_H_
#define CBPM_DONGLE_APP_H_

#include <stdint.h>
#include <zephyr/toolchain.h>	/* __packed, BUILD_ASSERT */
#include <zephyr/sys/util.h>	/* BIT() */

/** Firmware version, reported in every status reply. Bump the minor when you
 *  finish a milestone -- it makes "which build is on the board?" answerable.
 */
#define DONGLE_FW_MAJOR 0
#define DONGLE_FW_MINOR 1

/**
 * @brief Which link carries host traffic to Roo.
 *
 * The host always talks to the same USB serial port; this is what the dongle
 * does with the bytes. The contract makes MODE_BLE the power-up default,
 * because that is the everyday operator path. The milestone ladder builds
 * MODE_UART first (it is far easier to debug a wire than a radio), so until
 * M5 the power-up default in dongle_ctrl.c is deliberately the wrong one.
 * Fixing that is part of M5.
 */
enum dongle_mode {
	DONGLE_MODE_BLE = 0,
	DONGLE_MODE_UART = 1,
};

/** @brief State of the radio link to Roo, as reported to the host. */
enum dongle_ble_state {
	DONGLE_BLE_DISABLED = 0,     /**< built without CONFIG_CBPM_DONGLE_BLE */
	DONGLE_BLE_IDLE = 1,         /**< stack up, not looking for anything */
	DONGLE_BLE_SCANNING = 2,
	DONGLE_BLE_CONNECTING = 3,
	DONGLE_BLE_CONNECTED = 4,    /**< connected AND subscribed to TX */
};

/** @brief State of the wired link to Roo, as reported to the host. */
enum dongle_uart_state {
	DONGLE_UART_DOWN = 0,        /**< driver not ready */
	DONGLE_UART_READY = 1,       /**< receiving, no errors seen recently */
	DONGLE_UART_ERRORS = 2,      /**< framing/overrun errors seen recently */
};

/** Bit flags in dongle_status.flags. */
#define DONGLE_FLAG_BRIDGE_LOSSY BIT(0) /**< a buffer overflowed since boot */
#define DONGLE_FLAG_WDT_RESET    BIT(1) /**< last reset was the watchdog */

/**
 * @brief The payload of a DONGLE_GET_STATUS reply. THIS IS A WIRE FORMAT.
 *
 * Rules that apply to every wire struct you will ever write, and that this one
 * follows:
 *
 *   - Fixed-width types only. `int` is whatever the compiler feels like.
 *   - Explicitly packed, so the compiler inserts no padding of its own.
 *   - Little-endian, matching every other CBPM structure and both hosts.
 *   - A version byte FIRST, so an old host meeting a new dongle can say "I do
 *     not understand version 2" instead of silently misreading the fields.
 *   - Reserved bytes at the end, zero-filled, so a later field costs no
 *     version bump.
 *   - A BUILD_ASSERT on the size, so that the day someone adds a field without
 *     thinking, the build fails here rather than the data going quietly wrong
 *     on a bench three weeks later.
 *
 * tools/dongle_probe.py unpacks exactly this layout. If you change one side
 * you must change the other, and the probe's own self-test will tell you.
 */
struct dongle_status {
	uint8_t status_version;   /**< 1 for this layout */
	uint8_t mode;             /**< enum dongle_mode */
	uint8_t ble_state;        /**< enum dongle_ble_state */
	int8_t ble_rssi_dbm;      /**< 0 unless connected */
	uint8_t uart_state;       /**< enum dongle_uart_state */
	uint8_t flags;            /**< DONGLE_FLAG_* */
	uint8_t fw_major;
	uint8_t fw_minor;
	uint32_t bytes_to_roo;    /**< host -> Roo, since boot */
	uint32_t bytes_to_host;   /**< Roo -> host, since boot */
	uint32_t drops_to_roo;    /**< bytes discarded on overflow, host -> Roo */
	uint32_t drops_to_host;   /**< bytes discarded on overflow, Roo -> host */
	uint32_t reset_cause;     /**< hwinfo reset-cause bitmask */
	uint8_t breadcrumb_mode;  /**< mode before the last reset */
	uint8_t breadcrumb_fault; /**< fault code before the last reset */
	uint16_t reserved;        /**< zero */
} __packed;

#define DONGLE_STATUS_VERSION 1

BUILD_ASSERT(sizeof(struct dongle_status) == 32,
	     "dongle_status is a wire format: 32 bytes, no padding. If you "
	     "added a field, take it out of the reserved space and update "
	     "tools/dongle_probe.py to match.");

#endif /* CBPM_DONGLE_APP_H_ */
