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
 * MILESTONE M2 (second half). Answer DONGLE_GET_STATUS and DONGLE_SET_MODE.
 *
 * Check:  python tools/dongle_probe.py --port <port> --status
 *
 * When this works you can ask the board what it thinks is going on, from the
 * laptop, without a debugger. That is worth more than it sounds: every
 * milestone after this one is easier to debug because of it, which is why it
 * comes second rather than last.
 */

#include "ctrl/dongle_ctrl.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bridge/bridge.h"
#include "cdc/cdc_link.h"
#include "ctrl/verb_frame.h"
#include "sys/sys_health.h"
#include "uart/uart_link.h"

#ifdef CONFIG_CBPM_DONGLE_BLE
#include "ble/ble_central.h"
#endif

LOG_MODULE_REGISTER(dongle_ctrl, LOG_LEVEL_INF);

/*
 * The contract says MODE_BLE at power-up. The milestone ladder builds the wire
 * first, so until M5 this starts in UART mode and the line below is a lie you
 * are expected to fix. It is marked so you cannot forget.
 */
static enum dongle_mode current_mode = DONGLE_MODE_UART; /* TODO(M5): BLE */

void dongle_ctrl_init(void)
{
	LOG_INF("control channel ready, verbs 0x%02X..0x%02X, mode %s",
		DONGLE_VERB_BAND_FIRST, DONGLE_VERB_BAND_LAST,
		current_mode == DONGLE_MODE_UART ? "UART" : "BLE");
}

enum dongle_mode dongle_ctrl_mode(void)
{
	return current_mode;
}

void dongle_ctrl_set_mode(enum dongle_mode mode)
{
	if (mode == current_mode) {
		return;
	}

	/*
	 * TODO(M5): tear the old link down before standing the new one up.
	 *
	 * Leaving BLE: disconnect, stop scanning. Leaving UART: nothing to do,
	 * the peripheral can stay armed.
	 *
	 * The contract's requirement is that a mode change "tears down the
	 * inactive link cleanly and is reflected in the next status reply".
	 * The second half is free if you set current_mode here; the first half
	 * is the part that matters, because a BLE central that stays connected
	 * while the host thinks it is on the wire will keep consuming
	 * notifications nobody reads.
	 */
	LOG_INF("mode %s -> %s", current_mode == DONGLE_MODE_UART ? "UART" : "BLE",
		mode == DONGLE_MODE_UART ? "UART" : "BLE");
	current_mode = mode;
	sys_health_breadcrumb_mode(mode);
}

void dongle_ctrl_fill_status(struct dongle_status *out)
{
	struct bridge_counters c;

	bridge_counters_get(&c);
	memset(out, 0, sizeof(*out));

	out->status_version = DONGLE_STATUS_VERSION;
	out->mode = (uint8_t)current_mode;
	out->uart_state = (uint8_t)uart_link_state();
	out->fw_major = DONGLE_FW_MAJOR;
	out->fw_minor = DONGLE_FW_MINOR;
	out->bytes_to_roo = c.bytes_to_roo;
	out->bytes_to_host = c.bytes_to_host;
	out->drops_to_roo = c.drops_to_roo;
	out->drops_to_host = c.drops_to_host;
	out->reset_cause = sys_health_reset_cause();
	out->breadcrumb_mode = sys_health_prev_mode();
	out->breadcrumb_fault = sys_health_prev_fault();

	if (bridge_lossy()) {
		out->flags |= DONGLE_FLAG_BRIDGE_LOSSY;
	}
	if (sys_health_was_watchdog_reset()) {
		out->flags |= DONGLE_FLAG_WDT_RESET;
	}

#ifdef CONFIG_CBPM_DONGLE_BLE
	out->ble_state = (uint8_t)ble_central_state();
	out->ble_rssi_dbm = ble_central_rssi();
#else
	out->ble_state = (uint8_t)DONGLE_BLE_DISABLED;
#endif
}

static void send_status_reply(uint16_t request_id)
{
	struct dongle_status status;
	uint8_t frame[VERB_HEADER_LEN + sizeof(status) + VERB_CRC_LEN];
	size_t n;

	dongle_ctrl_fill_status(&status);

	n = verb_frame_encode(DONGLE_VERB_GET_STATUS, request_id,
			      (const uint8_t *)&status, sizeof(status), frame,
			      sizeof(frame));
	if (n == 0) {
		LOG_ERR("status frame would not encode");
		return;
	}

	/*
	 * Reply on the same port the request arrived on. If the buffer is full
	 * the reply is lost -- and that is the correct behaviour: a status
	 * request whose answer cannot fit is a host that has stopped reading,
	 * and blocking here would stall the bridge for a diagnostic.
	 */
	(void)cdc_link_write(frame, n);
}

size_t dongle_ctrl_intercept(const uint8_t *chunk, size_t len)
{
	struct verb_frame vf;
	size_t consumed = 0;

	/*
	 * TODO(M2): recognise and handle a dongle-band verb.
	 *
	 *   enum verb_parse_result r = verb_frame_parse(chunk, len, &vf,
	 *                                               &consumed);
	 *   if (r != VERB_PARSE_OK) {
	 *       return 0;                      // not a frame, or not yet whole
	 *   }
	 *   if (!verb_is_dongle_local(vf.verb)) {
	 *       return 0;                      // Roo's. Hands off.
	 *   }
	 *
	 *   switch (vf.verb) {
	 *   case DONGLE_VERB_GET_STATUS:
	 *       send_status_reply(vf.request_id);
	 *       break;
	 *   case DONGLE_VERB_SET_MODE:
	 *       if (vf.arg_len >= 1 && vf.args[0] <= DONGLE_MODE_UART) {
	 *           dongle_ctrl_set_mode((enum dongle_mode)vf.args[0]);
	 *       }
	 *       send_status_reply(vf.request_id);   // ack by reporting the
	 *                                           // new state, so the host
	 *                                           // never has to assume
	 *       break;
	 *   default:
	 *       // A verb in our band that we do not implement. Swallow it
	 *       // rather than forwarding: the band is ours, and passing an
	 *       // unknown 0xDx to Roo would be worse than doing nothing.
	 *       break;
	 *   }
	 *   return consumed;
	 *
	 * Note that BOTH verbs reply with the full status. The contract's rule
	 * is that mode is explicit in every reply and the host never keeps its
	 * own idea of what mode the dongle is in. State that lives in two
	 * places disagrees eventually.
	 */
	ARG_UNUSED(chunk);
	ARG_UNUSED(len);
	ARG_UNUSED(vf);
	ARG_UNUSED(consumed);
	(void)send_status_reply;
	return 0;
}
