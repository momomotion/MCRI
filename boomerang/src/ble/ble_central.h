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
 * @file ble_central.h
 * @brief The radio path to Roo. The dongle is the CENTRAL; Roo is the
 *        peripheral.
 *
 * Vocabulary first, because BLE has a lot of it and half of early confusion is
 * words rather than concepts:
 *
 *   - PERIPHERAL: advertises, waits to be connected to. Roo.
 *   - CENTRAL: scans, chooses, connects. The dongle (and, today, the laptop's
 *     own radio -- the dongle is taking that job over).
 *   - GATT: the data model on top of the connection. A SERVICE contains
 *     CHARACTERISTICS, each identified by a UUID.
 *   - NOTIFY: the peripheral pushes a characteristic's value to the central
 *     when it changes. This is how the sensor stream flows. You must SUBSCRIBE
 *     first, which writes to a small descriptor (the CCC) attached to the
 *     characteristic.
 *   - WRITE: the central pushes a value to the peripheral. This is how verbs
 *     travel.
 *
 * So the whole radio job is: find "CBPM-Roo", connect, find the service, turn
 * on notifications for TX, and write verbs to RX. Every byte in either
 * direction goes straight to the bridge unchanged -- the dongle re-frames
 * nothing.
 *
 * Compiled only when CONFIG_CBPM_DONGLE_BLE is set. See Kconfig.
 */

#ifndef CBPM_DONGLE_BLE_BLE_CENTRAL_H_
#define CBPM_DONGLE_BLE_BLE_CENTRAL_H_

#include <stddef.h>
#include <stdint.h>

#include "app.h"

/** @brief Start the Bluetooth stack and begin looking for Roo. */
int ble_central_init(void);

/**
 * @brief Write bytes to Roo's RX characteristic.
 *
 * @return bytes written, or a negative errno if there is no connection.
 */
int ble_central_write(const uint8_t *data, size_t len);

/** @brief Current link state, for the status reply. */
enum dongle_ble_state ble_central_state(void);

/** @brief Last measured RSSI in dBm, or 0 when not connected. */
int8_t ble_central_rssi(void);

#endif /* CBPM_DONGLE_BLE_BLE_CENTRAL_H_ */
