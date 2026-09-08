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
 * @file uart_link.h
 * @brief The wired link to Roo: UARTE0 at 1 Mbaud, driven by DMA.
 *
 * Two wires and a common ground, running at a megabit. There is no flow
 * control on this link -- if you stop reading, bytes are simply gone -- which
 * is why the receive path uses DMA into alternating buffers and why M4 spends
 * its time on what happens when you cannot keep up.
 *
 * Ownership: this module owns uart0 and its buffers. Received bytes are handed
 * to the bridge by calling bridge_from_roo() from the driver callback.
 */

#ifndef CBPM_DONGLE_UART_UART_LINK_H_
#define CBPM_DONGLE_UART_UART_LINK_H_

#include <stddef.h>
#include <stdint.h>

#include "app.h"

/**
 * @brief Bring up the UART and start receiving.
 *
 * @return 0 on success, negative errno otherwise.
 */
int uart_link_init(void);

/**
 * @brief Send bytes to Roo.
 *
 * Blocks until the DMA transfer completes or @p timeout_ms expires, because
 * the async API owns the caller's buffer for the duration of a transfer and
 * returning early would let the caller overwrite bytes still in flight.
 *
 * @return the number of bytes sent, or a negative errno.
 */
int uart_link_write(const uint8_t *data, size_t len, int32_t timeout_ms);

/** @brief Link state for the status reply. */
enum dongle_uart_state uart_link_state(void);

#endif /* CBPM_DONGLE_UART_UART_LINK_H_ */
