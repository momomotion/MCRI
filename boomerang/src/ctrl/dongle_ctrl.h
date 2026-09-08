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
 * @file dongle_ctrl.h
 * @brief The dongle's own control channel: status and mode, in band.
 *
 * The host has one serial port and it is busy carrying traffic for Roo. So how
 * does it ask the DONGLE a question?
 *
 * The answer this design takes is a reserved band of verb ids (0xD0..0xDF).
 * The dongle watches the head of each burst from the host; a verb in its own
 * band it answers itself and never forwards, and everything else is bridged
 * untouched. No second USB port, no escape sequences, no mode-switch character
 * that could appear in sensor data.
 *
 * Alternatives that were considered and rejected, because knowing why a design
 * is not something is as useful as knowing what it is:
 *
 *   - A second CDC-ACM port for control. Clean, and it doubles the host's
 *     port-hunting problem: which of the two /dev/cu.usbmodem entries is the
 *     control one? Deferred, not forbidden.
 *   - An escape sequence in the byte stream. Requires the dongle to inspect
 *     every byte and to escape any that collide, which is the transparency
 *     the UART bridge exists to provide.
 */

#ifndef CBPM_DONGLE_CTRL_DONGLE_CTRL_H_
#define CBPM_DONGLE_CTRL_DONGLE_CTRL_H_

#include <stddef.h>
#include <stdint.h>

#include "app.h"

/** @brief Initialise the control channel. Call before the bridge starts. */
void dongle_ctrl_init(void);

/**
 * @brief Offer the head of a host burst to the control channel.
 *
 * If @p chunk begins with a complete, valid frame carrying a dongle-band verb,
 * this handles it (including sending the reply) and reports how many bytes it
 * swallowed. Otherwise it returns 0 and the caller forwards everything.
 *
 * @return bytes consumed; 0 means "not mine, forward it all".
 */
size_t dongle_ctrl_intercept(const uint8_t *chunk, size_t len);

/** @brief Which link the bridge should currently use. */
enum dongle_mode dongle_ctrl_mode(void);

/**
 * @brief Change mode, tearing down the link being left.
 *
 * Exposed separately from the verb handler so that start-up code and tests can
 * set the mode without synthesising a frame.
 */
void dongle_ctrl_set_mode(enum dongle_mode mode);

/** @brief Assemble the current status. Used by the verb handler and by the
 *         periodic log line in main().
 */
void dongle_ctrl_fill_status(struct dongle_status *out);

#endif /* CBPM_DONGLE_CTRL_DONGLE_CTRL_H_ */
