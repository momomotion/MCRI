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
 * ===========================================================================
 * START HERE.
 * ===========================================================================
 *
 * WHAT THIS DEVICE DOES
 *
 * A laptop needs to talk to the Roo control box. Roo is either across the room
 * (Bluetooth) or on the end of a wire (UART). Rather than make the laptop care,
 * this dongle presents ONE USB serial port and carries the bytes whichever way
 * is currently selected. From the laptop's point of view there is a serial
 * port, and Roo is on the other end of it. That is the entire product.
 *
 *      PC / Mac  --USB-CDC-->  DONGLE  --BLE GATT-->  Roo
 *                                      --UART------>  Roo
 *
 * WHAT RUNS, AND WHEN
 *
 * Zephyr boots, initialises drivers, and calls main(). main() brings the
 * subsystems up in dependency order and then does almost nothing -- the work
 * happens in threads and interrupt callbacks:
 *
 *   main               (this file)  init, then a slow status heartbeat
 *   pump_to_roo        (bridge.c)   host bytes -> intercept -> UART or BLE
 *   CDC-ACM ISR        (cdc_link.c) USB bytes <-> ring buffers
 *   UARTE DMA callback (uart_link.c) wire bytes -> bridge_from_roo()
 *   BT RX thread       (ble_central.c) notifications -> bridge_from_roo()
 *
 * Two rules keep that manageable, and they are worth internalising early:
 *
 *   1. Interrupt callbacks copy bytes and return. No logging, no blocking, no
 *      waiting on a lock a thread might hold.
 *   2. Anything that can block lives in a thread with a bounded buffer in
 *      front of it.
 *
 * ORDER OF INITIALISATION, AND WHY IT IS THIS ORDER
 *
 *   sys_health   first, so the reset cause and the breadcrumb are read before
 *                anything else can disturb them
 *   cdc_link     the host port: bring it up early so that failures after this
 *                point can be reported to a laptop
 *   uart_link    the wire to Roo
 *   ble_central  the radio (M5, compiled in only when enabled)
 *   dongle_ctrl  the control channel, which needs the links to report on
 *   bridge       LAST, because the moment it starts, bytes begin to move, and
 *                you do not want traffic flowing into a half-built system
 *
 * WHERE TO GO NEXT: docs/MILESTONES.md. Do not start editing files at random;
 * the ladder exists because each rung makes the next one debuggable.
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app.h"
#include "bridge/bridge.h"
#include "cdc/cdc_link.h"
#include "ctrl/dongle_ctrl.h"
#include "sys/sys_health.h"
#include "uart/uart_link.h"

#ifdef CONFIG_CBPM_DONGLE_BLE
#include "ble/ble_central.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*
 * The board's red LED, via the devicetree alias led0. GPIO_DT_SPEC_GET_OR
 * yields an empty spec when the alias does not exist, so this file still
 * builds on a board without LEDs -- which is why the .port check below is not
 * paranoia.
 */
#if IS_ENABLED(CONFIG_CBPM_DONGLE_STATUS_LED)
static const struct gpio_dt_spec status_led =
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
#endif

static void status_led_init(void)
{
#if IS_ENABLED(CONFIG_CBPM_DONGLE_STATUS_LED)
	if (status_led.port == NULL || !gpio_is_ready_dt(&status_led)) {
		LOG_WRN("no status LED on this board");
		return;
	}
	(void)gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
#endif
}

static void status_led_toggle(void)
{
#if IS_ENABLED(CONFIG_CBPM_DONGLE_STATUS_LED)
	if (status_led.port != NULL) {
		(void)gpio_pin_toggle_dt(&status_led);
	}
#endif
}

int main(void)
{
	int err;

	/* M0: prove the board is alive and say who we are. If you see this
	 * line in the RTT log, the toolchain, the flash and the logging path
	 * are all working, and every later problem is your code. */
	LOG_INF("CBPM dongle v%u.%u starting", DONGLE_FW_MAJOR, DONGLE_FW_MINOR);

	sys_health_init();
	status_led_init();

	err = cdc_link_init();
	if (err != 0) {
		LOG_ERR("CDC link failed (%d) -- the host will see no port", err);
		/* Deliberately NOT fatal. A dongle with no USB is useless, but a
		 * dongle that reboots in a loop is worse: it cannot even tell
		 * you what is wrong over RTT. Carry on and report. */
	}

	err = uart_link_init();
	if (err != 0) {
		LOG_ERR("Roo UART failed (%d)", err);
	}

#ifdef CONFIG_CBPM_DONGLE_BLE
	err = ble_central_init();
	if (err != 0) {
		LOG_ERR("Bluetooth failed (%d)", err);
	}
#endif

	dongle_ctrl_init();
	bridge_init();

	/*
	 * The main thread's remaining job is a heartbeat: blink, and once in a
	 * while log a one-line summary. Cheap, and it answers "is the scheduler
	 * still running?" from across the room.
	 *
	 * TODO(M6): once the watchdog exists, this loop is a reasonable place
	 * to feed a channel of its own -- but think first about whether a
	 * heartbeat that keeps running while the bridge is wedged is a comfort
	 * or a lie.
	 */
	while (true) {
		static uint32_t ticks;
		struct dongle_status st;

		status_led_toggle();
		k_sleep(K_MSEC(500));

		if (++ticks % 20 == 0) { /* every 10 seconds */
			dongle_ctrl_fill_status(&st);
			LOG_INF("mode %u, ble %u, uart %u, to_roo %u B (%u lost), "
				"to_host %u B (%u lost)",
				st.mode, st.ble_state, st.uart_state,
				st.bytes_to_roo, st.drops_to_roo,
				st.bytes_to_host, st.drops_to_host);
		}
	}

	return 0;
}
