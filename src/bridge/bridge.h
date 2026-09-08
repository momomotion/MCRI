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
 * @file bridge.h
 * @brief The pump in the middle: host bytes out to Roo, Roo bytes back.
 *
 * This is the heart of the device and it is deliberately small. Everything
 * clever lives at the edges (USB, UART, BLE); the bridge only decides which
 * way bytes go and counts what it could not carry.
 *
 *      cdc_link_read()  --> [intercept?] --> uart_link_write()
 *                                        or  ble_central_write()
 *
 *      bridge_from_roo() --------------------> cdc_link_write()
 *
 * THE RULE THIS MODULE EXISTS TO ENFORCE: never silently drop. A bridge that
 * quietly thins a data stream produces a recording that looks complete and is
 * not, and nobody finds out until someone tries to analyse it months later.
 * Losing bytes under overload is acceptable and sometimes unavoidable; hiding
 * it is not. So every discarded byte is counted, and the counts go back to the
 * host in every status reply along with a sticky "this link has been lossy"
 * flag.
 */

#ifndef CBPM_DONGLE_BRIDGE_BRIDGE_H_
#define CBPM_DONGLE_BRIDGE_BRIDGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Byte and loss counts, both directions, since boot. */
struct bridge_counters {
	uint32_t bytes_to_roo;
	uint32_t bytes_to_host;
	uint32_t drops_to_roo;
	uint32_t drops_to_host;
};

/** @brief Start the bridge threads. Call after the links are initialised. */
void bridge_init(void);

/**
 * @brief Hand bytes that arrived from Roo to the host path.
 *
 * SAFE TO CALL FROM INTERRUPT CONTEXT, and it has to be: the UART DMA callback
 * and the Bluetooth notification callback both use it. That constraint is why
 * it copies into a buffer and returns rather than doing anything clever.
 *
 * @return the number of bytes accepted. Anything less than @p len has been
 *         dropped and counted.
 */
size_t bridge_from_roo(const uint8_t *data, size_t len);

/** @brief Snapshot the counters. */
void bridge_counters_get(struct bridge_counters *out);

/** @brief True if anything has ever been dropped. Sticky until reset. */
bool bridge_lossy(void);

#endif /* CBPM_DONGLE_BRIDGE_BRIDGE_H_ */
