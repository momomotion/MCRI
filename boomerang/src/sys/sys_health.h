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
 * @file sys_health.h
 * @brief Why did we reset, what were we doing, and are we still alive?
 *
 * Three small mechanisms that together turn "the dongle rebooted at some point
 * and nobody knows why" into an answerable question:
 *
 *   1. RESET CAUSE. The chip records why it last started: power-on, pin reset,
 *      watchdog, software request. Read it at boot, report it forever.
 *   2. BREADCRUMB. A few bytes of RAM that deliberately survive a reset,
 *      holding what we were doing when it happened.
 *   3. WATCHDOG. A timer that reboots the device if a thread stops checking
 *      in. The point is not to prevent the bug; it is to guarantee the device
 *      comes back and tells you it happened.
 *
 * The first two are M0 and cheap. The third is M6, and is the one that makes
 * the difference between a device that wedges silently on someone's desk and
 * one that recovers and leaves evidence.
 */

#ifndef CBPM_DONGLE_SYS_SYS_HEALTH_H_
#define CBPM_DONGLE_SYS_SYS_HEALTH_H_

#include <stdbool.h>
#include <stdint.h>

#include "app.h"

/**
 * @brief Read the reset cause, recover the breadcrumb, log both.
 *
 * Call this FIRST in main(), before anything else can overwrite the
 * breadcrumb.
 */
void sys_health_init(void);

/** @brief The hwinfo reset-cause bitmask captured at boot. */
uint32_t sys_health_reset_cause(void);

/** @brief True if the last reset came from the watchdog. */
bool sys_health_was_watchdog_reset(void);

/** @brief The mode recorded before the last reset, or 0xFF if unknown. */
uint8_t sys_health_prev_mode(void);

/** @brief The fault code recorded before the last reset, or 0 if none. */
uint8_t sys_health_prev_fault(void);

/** @brief Record the current mode in the breadcrumb. Cheap; call on change. */
void sys_health_breadcrumb_mode(enum dongle_mode mode);

/** @brief Record a fault code in the breadcrumb before a deliberate reset. */
void sys_health_breadcrumb_fault(uint8_t fault);

/**
 * @brief Register the calling thread with the task watchdog. (M6)
 *
 * @param period_ms how long this thread may go without checking in.
 * @return a channel id to pass to sys_health_wdt_feed(), or negative errno.
 */
int sys_health_wdt_register(uint32_t period_ms);

/** @brief Tell the watchdog this thread is still making progress. (M6) */
void sys_health_wdt_feed(int channel);

#endif /* CBPM_DONGLE_SYS_SYS_HEALTH_H_ */
