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
 * MILESTONES M3 (make it carry bytes) and M4 (make it honest about loss).
 *
 * Check M3:  python tools/dongle_probe.py --port <port> --loopback 65536
 * Check M4:  python tools/dongle_probe.py --port <port> --flood
 *
 * The flood test deliberately sends faster than the far end can take, then
 * reads the status reply and asserts that the drop counter moved and the lossy
 * flag is set. In other words it checks that you FAILED CORRECTLY, which is a
 * kind of test worth getting used to writing.
 */

#include "bridge/bridge.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app.h"
#include "cdc/cdc_link.h"
#include "ctrl/dongle_ctrl.h"
#include "uart/uart_link.h"

#ifdef CONFIG_CBPM_DONGLE_BLE
#include "ble/ble_central.h"
#endif

LOG_MODULE_REGISTER(bridge, LOG_LEVEL_INF);

/*
 * Counters.
 *
 * These are written from interrupt context (bridge_from_roo) and read from a
 * thread (the status reply). On a 32-bit Cortex-M, a naturally aligned 32-bit
 * load or store is a single instruction and cannot be torn, so each individual
 * counter is safe without a lock. What is NOT safe is reading all four and
 * assuming they describe the same instant -- an interrupt can land between the
 * reads. For a diagnostic counter that is fine and the alternative (disabling
 * interrupts around the snapshot) costs more than it buys. Knowing WHICH of
 * those two statements applies to your data is most of embedded concurrency.
 */
static struct bridge_counters counters;
static bool lossy;

/* Host -> Roo pump. Priority 5: a cooperative-ish middle ground -- high enough
 * that the link keeps up, low enough that it cannot starve the system work
 * queue. Stack 1536 bytes covers the chunk buffer below plus call depth. */
#define PUMP_STACK 1536
#define PUMP_PRIO 5

static void pump_to_roo(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	/*
	 * A local chunk buffer. 256 bytes is a compromise: big enough that one
	 * read covers a typical USB packet, small enough to sit on a thread
	 * stack without a second thought.
	 */
	uint8_t chunk[256];

	while (true) {
		size_t n = cdc_link_read(chunk, sizeof(chunk), K_FOREVER);

		if (n == 0) {
			continue;
		}

		/*
		 * TODO(M2): give the dongle's own control channel first refusal.
		 *
		 *   size_t taken = dongle_ctrl_intercept(chunk, n);
		 *   if (taken > 0) {
		 *       // The dongle answered it. Do not forward those bytes.
		 *       if (taken == n) {
		 *           continue;
		 *       }
		 *       memmove(chunk, chunk + taken, n - taken);
		 *       n -= taken;
		 *   }
		 *
		 * Why only at the head of a chunk, and not by scanning the
		 * whole stream? Because scanning means understanding every byte
		 * that passes, and in UART mode this link carries a raw sensor
		 * stream in which any byte value can occur. A 0xC0 in the middle
		 * of sample data is not a command. The contract's answer is that
		 * control verbs are operator actions -- the human clicks a
		 * button while the stream is idle -- so recognising them at the
		 * start of a burst is enough, and pure transparency is worth
		 * more than catching the rare mid-stream case.
		 *
		 * Write that limitation down where the host author will see it.
		 * An undocumented "usually works" is a trap for the next person.
		 */

		/*
		 * TODO(M3): forward the rest to whichever link the mode selects.
		 *
		 *   int sent;
		 *   if (dongle_ctrl_mode() == DONGLE_MODE_UART) {
		 *       sent = uart_link_write(chunk, n, 100);
		 *   } else {
		 *       sent = ble_central_write(chunk, n);   // M5
		 *   }
		 *   if (sent > 0) {
		 *       counters.bytes_to_roo += sent;
		 *   }
		 *   if (sent < (int)n) {
		 *       counters.drops_to_roo += n - MAX(sent, 0);
		 *       lossy = true;
		 *   }
		 *
		 * Note the shape: count what went, count what did not, set the
		 * flag. Three lines, and they are the difference between a
		 * bridge you can trust and one you cannot.
		 */
		ARG_UNUSED(n);
	}
}

K_THREAD_DEFINE(pump_to_roo_tid, PUMP_STACK, pump_to_roo, NULL, NULL, NULL,
		PUMP_PRIO, 0, /* delay_ms = */ SYS_FOREVER_MS);
/*
 * The last argument is the start delay in milliseconds. SYS_FOREVER_MS means
 * the thread is CREATED but never started on its own; bridge_init() starts it
 * explicitly, so it cannot run before the links it depends on exist. Threads
 * that start themselves at boot are a common source of "works on the second
 * run" bugs -- pass 0 there and this one would begin reading from a CDC port
 * that has not been initialised yet.
 */

size_t bridge_from_roo(const uint8_t *data, size_t len)
{
	/*
	 * TODO(M3/M4): push towards the host and account for what does not fit.
	 *
	 *   size_t accepted = cdc_link_write(data, len);
	 *   counters.bytes_to_host += accepted;
	 *   if (accepted < len) {
	 *       counters.drops_to_host += len - accepted;
	 *       lossy = true;
	 *   }
	 *   return accepted;
	 *
	 * Do not log here. This runs in interrupt context, and the situation
	 * where it drops bytes is exactly the situation where it is being
	 * called thousands of times a second. Logging would turn a small data
	 * loss into a wedged device -- a failure mode with a name in this
	 * project's history.
	 */
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	return 0;
}

void bridge_counters_get(struct bridge_counters *out)
{
	*out = counters;
}

bool bridge_lossy(void)
{
	return lossy;
}

void bridge_init(void)
{
	k_thread_name_set(pump_to_roo_tid, "pump_to_roo");
	k_thread_start(pump_to_roo_tid);
	LOG_INF("bridge started");
}
