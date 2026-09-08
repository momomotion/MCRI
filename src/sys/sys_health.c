/*
 * Copyright (c) 2026 MCRI. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Proprietary
 *
 * This software is the confidential and proprietary information of
 * MCRI. It must not be used, copied, redistributed or disclosed
 * without prior written authorisation.
 */

/*
 * MILESTONE M6. Survive, and leave evidence.
 *
 * Check:  python tools/dongle_probe.py --port <port> --wedge
 *         The probe asks the firmware to stall a thread on purpose. The board
 *         should reset within the watchdog period, come back, and report a
 *         watchdog reset cause with the breadcrumb intact.
 *
 * The reset-cause and breadcrumb halves work from M0 -- they cost almost
 * nothing and every later milestone is easier when the board can tell you why
 * it restarted.
 */

#include "sys/sys_health.h"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_TASK_WDT
#include <zephyr/task_wdt/task_wdt.h>
#endif

LOG_MODULE_REGISTER(sys_health, LOG_LEVEL_INF);

/*
 * THE BREADCRUMB.
 *
 * __noinit places a variable in a RAM section the C start-up code does not
 * clear. Ordinary globals are zeroed (.bss) or loaded from flash (.data) on
 * every boot; a __noinit variable simply holds whatever the RAM held a moment
 * earlier. Since a soft reset does not cut power to the SRAM, that is the
 * value from before the reset.
 *
 * Uninitialised memory is also what you get after a POWER-ON reset, and it is
 * garbage. That is what the magic word is for: if it does not match, the rest
 * of the structure is meaningless and must be ignored. Without that check you
 * will one day report a "previous fault" that is really a random byte.
 *
 * A XIAO-SPECIFIC WARNING. This module runs behind the Adafruit UF2
 * bootloader, which uses the top of RAM for itself and keeps its double-tap
 * marker at 0x20007F7C. A breadcrumb placed in that region is not yours. The
 * linker puts __noinit in the ordinary application RAM, well clear of it, so
 * the default here is safe -- but if you ever move the breadcrumb to a fixed
 * address, check it against the bootloader's territory first. The sibling Roo
 * board carries an open question about exactly this, and "unverified" is the
 * honest word for it there.
 */
#define BREADCRUMB_MAGIC 0x0D0E5AFEu

struct breadcrumb {
	uint32_t magic;
	uint8_t mode;
	uint8_t fault;
	uint16_t boot_count;
};

static __noinit struct breadcrumb crumb;

/* Snapshot taken at boot, before we overwrite the live breadcrumb. */
static struct breadcrumb crumb_at_boot;
static uint32_t reset_cause;

void sys_health_init(void)
{
	bool valid;

	/*
	 * hwinfo_get_reset_cause() returns a bitmask of RESET_* flags. On the
	 * nRF52 it reads the RESETREAS register. Two things to know:
	 *
	 *   - The bits are STICKY. The hardware ORs new causes in and never
	 *     clears them, so without hwinfo_clear_reset_cause() you see the
	 *     union of every reset since power-on, which is useless. Read then
	 *     clear, every boot.
	 *   - A power-on reset often reports NOTHING (all bits zero) rather
	 *     than a POR flag. Zero is a legitimate answer meaning "cold
	 *     start", not a failure.
	 */
	if (hwinfo_get_reset_cause(&reset_cause) != 0) {
		reset_cause = 0;
	}
	(void)hwinfo_clear_reset_cause();

	valid = (crumb.magic == BREADCRUMB_MAGIC);
	crumb_at_boot = crumb;

	if (!valid) {
		/* Cold start, or a corrupted crumb. Start a fresh one. */
		crumb_at_boot.mode = 0xFF;
		crumb_at_boot.fault = 0;
		crumb.magic = BREADCRUMB_MAGIC;
		crumb.mode = 0xFF;
		crumb.fault = 0;
		crumb.boot_count = 0;
	} else {
		crumb.boot_count++;
		/* Clear the fault so it is not re-reported after the next clean
		 * reboot: it describes the boot we just recovered from. */
		crumb.fault = 0;
	}

	LOG_INF("boot %u, reset cause 0x%08x%s, previous mode %u, fault %u",
		crumb.boot_count, reset_cause,
		sys_health_was_watchdog_reset() ? " (WATCHDOG)" : "",
		crumb_at_boot.mode, crumb_at_boot.fault);
}

uint32_t sys_health_reset_cause(void)
{
	return reset_cause;
}

bool sys_health_was_watchdog_reset(void)
{
	return (reset_cause & RESET_WATCHDOG) != 0;
}

uint8_t sys_health_prev_mode(void)
{
	return crumb_at_boot.mode;
}

uint8_t sys_health_prev_fault(void)
{
	return crumb_at_boot.fault;
}

void sys_health_breadcrumb_mode(enum dongle_mode mode)
{
	crumb.mode = (uint8_t)mode;
}

void sys_health_breadcrumb_fault(uint8_t fault)
{
	crumb.fault = fault;
}

int sys_health_wdt_register(uint32_t period_ms)
{
	/*
	 * TODO(M6): register a task-watchdog channel.
	 *
	 *   return task_wdt_add(period_ms, NULL, NULL);
	 *
	 * Zephyr's task watchdog multiplexes many software channels onto the
	 * one hardware watchdog: the hardware timer is fed only while EVERY
	 * registered channel is being fed. That is what makes it useful for a
	 * device with several threads -- one stuck thread reboots the box even
	 * though the others are healthy.
	 *
	 * Before you pick a period, decide what "stuck" means for that thread.
	 * The bridge pump blocks in cdc_link_read() waiting for host traffic,
	 * which can legitimately be silent for minutes, so a period based on
	 * "how often do bytes arrive" would reboot an idle, healthy dongle. A
	 * watchdog that fires on correct behaviour is worse than none: people
	 * turn it off. Think about feeding it on each loop iteration INCLUDING
	 * the timeout path, and giving the read a bounded timeout so the loop
	 * always turns over.
	 *
	 * That exact mistake -- a heartbeat that only advanced on success, so
	 * that back-pressure read as death -- cost this project a fortnight on
	 * the sensor node. You are being handed the answer; the reason it is
	 * worth remembering is that it is not obvious until it bites.
	 */
	ARG_UNUSED(period_ms);
	return -ENOSYS;
}

void sys_health_wdt_feed(int channel)
{
	/*
	 * TODO(M6):
	 *
	 *   if (channel >= 0) {
	 *       task_wdt_feed(channel);
	 *   }
	 */
	ARG_UNUSED(channel);
}
