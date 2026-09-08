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
 * @file cdc_link.h
 * @brief The USB CDC-ACM link: the one serial port the host computer sees.
 *
 * CDC-ACM is the USB class that makes a device appear as a serial port
 * (/dev/cu.usbmodemXXXX on macOS, COMn on Windows). Zephyr implements it as a
 * UART driver, so the API you use is the ordinary interrupt-driven UART API
 * even though there is no UART involved. That is a genuinely useful thing to
 * know: a "serial port" on a modern machine is nearly always a USB device
 * pretending.
 *
 * Ownership: this module owns the USB stack, the CDC device, and a ring buffer
 * in each direction. Nothing else touches them. Other modules move bytes with
 * cdc_link_write() and cdc_link_read().
 */

#ifndef CBPM_DONGLE_CDC_CDC_LINK_H_
#define CBPM_DONGLE_CDC_CDC_LINK_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>

/**
 * @brief Bring up USB and the CDC-ACM port.
 *
 * @return 0 on success, a negative errno on failure.
 */
int cdc_link_init(void);

/**
 * @brief Queue bytes for the host.
 *
 * Non-blocking. If the outbound buffer is full the excess is DISCARDED and the
 * shortfall is visible in the return value -- the caller decides whether that
 * matters and counts the loss. Never blocks, because callers include the
 * Bluetooth receive callback, and blocking there stalls the radio.
 *
 * @return the number of bytes accepted, which may be fewer than @p len.
 */
size_t cdc_link_write(const uint8_t *data, size_t len);

/**
 * @brief Take bytes that arrived from the host.
 *
 * @param buf      destination.
 * @param cap      capacity of @p buf.
 * @param timeout  how long to wait for at least one byte.
 *
 * @return the number of bytes copied, 0 if the timeout expired first.
 */
size_t cdc_link_read(uint8_t *buf, size_t cap, k_timeout_t timeout);

/**
 * @brief Has a host program opened the port?
 *
 * True once the host asserts DTR. Useful because writing to a CDC port that
 * nobody has opened simply fills your buffer and then discards, which looks
 * exactly like a firmware bug when it is in fact a host that is not listening.
 */
bool cdc_link_host_present(void);

#endif /* CBPM_DONGLE_CDC_CDC_LINK_H_ */
