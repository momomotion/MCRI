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
 * MILESTONE M5. The radio.
 *
 * Build with the radio on:
 *     west build -b xiao_ble/nrf52840 -d build-dongle -- -DCONFIG_CBPM_DONGLE_BLE=y
 * or uncomment the line in prj.conf once you are working on this for good.
 *
 * Check:  python tools/dongle_probe.py --port <port> --status
 *         with a Roo powered nearby, ble_state should reach CONNECTED (4) and
 *         the RSSI should be a plausible negative number. With no Roo present
 *         it should sit at SCANNING (2) and never lie about it.
 *
 * Leave this milestone until the wire works. Debugging a radio and a bridge at
 * the same time is two unknowns; debugging a radio when the bridge is already
 * proven is one.
 */

#include "ble/ble_central.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bridge/bridge.h"

LOG_MODULE_REGISTER(ble_central, LOG_LEVEL_INF);

/*
 * The GATT contract, copied from docs/contract.md. These UUIDs are shared with
 * Roo's firmware and with the host GUI; all three must agree exactly.
 *
 * BT_UUID_128_ENCODE takes the five groups of a printed UUID and lays them out
 * in the byte order Bluetooth uses (little-endian, so the printed form appears
 * reversed on the wire). Using the macro rather than a hand-written byte array
 * is how you avoid the classic reversed-UUID afternoon.
 */
#define ROO_UUID_SERVICE_VAL                                                   \
	BT_UUID_128_ENCODE(0xf1a7c0de, 0x0001, 0x4b2e, 0x8a10, 0x5cb9d0c0ffee)
#define ROO_UUID_TX_VAL                                                        \
	BT_UUID_128_ENCODE(0xf1a7c0de, 0x0002, 0x4b2e, 0x8a10, 0x5cb9d0c0ffee)
#define ROO_UUID_RX_VAL                                                        \
	BT_UUID_128_ENCODE(0xf1a7c0de, 0x0003, 0x4b2e, 0x8a10, 0x5cb9d0c0ffee)

static const struct bt_uuid_128 roo_uuid_service =
	BT_UUID_INIT_128(ROO_UUID_SERVICE_VAL);
static const struct bt_uuid_128 roo_uuid_tx = BT_UUID_INIT_128(ROO_UUID_TX_VAL);
static const struct bt_uuid_128 roo_uuid_rx = BT_UUID_INIT_128(ROO_UUID_RX_VAL);

/** The local name Roo advertises. Discovery is by name, not by address, so any
 *  Roo on the bench will do. On a multi-Roo bench day that is a hazard rather
 *  than a convenience -- see the open question in docs/contract.md. */
#define ROO_ADV_NAME "CBPM-Roo"

static struct bt_conn *roo_conn;
static enum dongle_ble_state state = DONGLE_BLE_IDLE;
static int8_t last_rssi;
static uint16_t rx_value_handle; /* where to write verbs, found by discovery */

/* Subscription bookkeeping. The stack keeps a pointer to this structure for
 * the life of the subscription, so it must be static -- a stack local here is
 * a use-after-return waiting to happen. */
static struct bt_gatt_subscribe_params sub_params;

static uint8_t on_notify(struct bt_conn *conn,
			 struct bt_gatt_subscribe_params *params,
			 const void *data, uint16_t length)
{
	ARG_UNUSED(conn);

	if (data == NULL) {
		/* The peripheral has unsubscribed us, usually because the link
		 * dropped. Returning STOP releases the parameters. */
		LOG_INF("notifications ended");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}

	/*
	 * TODO(M5): forward the notification to the host, verbatim.
	 *
	 *   bridge_from_roo(data, length);
	 *
	 * Verbatim is the whole point. These bytes are COMPACT records that the
	 * host's existing decoder already understands; a record may span
	 * several notifications and a notification may hold several records,
	 * and the decoder copes with both. If the dongle tried to align
	 * anything it would only get in the way.
	 */
	ARG_UNUSED(data);
	ARG_UNUSED(length);
	return BT_GATT_ITER_CONTINUE;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0) {
		LOG_WRN("connect failed (0x%02x)", err);
		state = DONGLE_BLE_SCANNING;
		/* TODO(M5): restart scanning here. */
		return;
	}

	roo_conn = bt_conn_ref(conn);
	state = DONGLE_BLE_CONNECTING; /* not CONNECTED until subscribed */
	LOG_INF("connected, discovering services");

	/*
	 * TODO(M5): discover the service, then subscribe to TX.
	 *
	 * Two routes, both fine:
	 *
	 *   (a) NCS's GATT Discovery Manager, <bluetooth/gatt_dm.h>: call
	 *       bt_gatt_dm_start(conn, &roo_uuid_service.uuid, &dm_cb, NULL)
	 *       and pick the characteristics out of the result in the callback.
	 *       Less code, one more dependency (CONFIG_BT_GATT_DM, already
	 *       selected by CONFIG_CBPM_DONGLE_BLE).
	 *
	 *   (b) Plain Zephyr bt_gatt_discover() with a small state machine:
	 *       discover the primary service, then the characteristic, then the
	 *       CCC descriptor, each step starting the next from its callback.
	 *       This is what the upstream central_hr sample does, and writing it
	 *       once teaches you what the GATT database actually looks like.
	 *
	 * Whichever you choose, the end state is: rx_value_handle holds the
	 * handle of the RX characteristic's value, and sub_params is filled in
	 * (notify = on_notify, value_handle = TX value handle, ccc_handle = the
	 * TX CCC descriptor, value = BT_GATT_CCC_NOTIFY) and passed to
	 * bt_gatt_subscribe(). Only then set state = DONGLE_BLE_CONNECTED.
	 *
	 * Reporting CONNECTED before the subscription exists would be a small
	 * lie with real consequences: the host would wait for a stream that is
	 * never coming and blame the far end.
	 */
	(void)roo_uuid_service;
	(void)roo_uuid_tx;
	(void)sub_params;
	(void)on_notify;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);

	LOG_INF("disconnected (0x%02x)", reason);

	if (roo_conn != NULL) {
		bt_conn_unref(roo_conn);
		roo_conn = NULL;
	}
	rx_value_handle = 0;
	last_rssi = 0;
	state = DONGLE_BLE_SCANNING;

	/*
	 * TODO(M5): restart scanning.
	 *
	 * And surface the event to the host: the contract says connect and
	 * disconnect are "surfaced to the host via the dongle status channel,
	 * never silently". The status reply carries the state, so simply
	 * keeping this variable honest satisfies it -- but only if the host
	 * asks. Consider whether a spontaneous status frame on a state change
	 * would serve the operator better.
	 */
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *ad)
{
	ARG_UNUSED(adv_type);

	/*
	 * TODO(M5): find Roo by name and connect.
	 *
	 * Advertising data is a sequence of {length, type, value} records. Use
	 * bt_data_parse(ad, cb, user_data) with a callback that looks for
	 * BT_DATA_NAME_COMPLETE (and BT_DATA_NAME_SHORTENED) and compares the
	 * value against ROO_ADV_NAME.
	 *
	 * On a match:
	 *   bt_le_scan_stop();
	 *   bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
	 *                     BT_LE_CONN_PARAM_DEFAULT, &roo_conn);
	 *   state = DONGLE_BLE_CONNECTING;
	 *
	 * You MUST stop scanning before creating a connection; the controller
	 * will refuse otherwise, and the -EBUSY that comes back is easy to
	 * misread as a hardware problem.
	 *
	 * Keep the callback short. It runs in the Bluetooth stack's context and
	 * a slow callback shows up as missed advertisements, which looks like a
	 * range problem rather than a software one.
	 */
	ARG_UNUSED(addr);
	ARG_UNUSED(ad);
	last_rssi = rssi;
}

int ble_central_init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

	LOG_INF("Bluetooth up, looking for \"%s\"", ROO_ADV_NAME);

	/*
	 * TODO(M5): start scanning.
	 *
	 *   err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, scan_cb);
	 *   state = DONGLE_BLE_SCANNING;
	 *
	 * ACTIVE scanning sends a scan request to each advertiser, which is how
	 * you get the complete local name when it does not fit in the initial
	 * advertisement. Passive scanning is lower power and would work here
	 * only if Roo puts its full name in the primary packet. Active is the
	 * safe default; power is not a concern on a USB-powered dongle.
	 */
	(void)scan_cb;
	state = DONGLE_BLE_IDLE;
	return 0;
}

int ble_central_write(const uint8_t *data, size_t len)
{
	if (roo_conn == NULL || rx_value_handle == 0) {
		return -ENOTCONN;
	}

	/*
	 * TODO(M5): write to the RX characteristic.
	 *
	 *   int err = bt_gatt_write_without_response(roo_conn, rx_value_handle,
	 *                                            data, len, false);
	 *
	 * The contract asks for write-WITH-response, so the host knows the
	 * write landed. That needs bt_gatt_write() with a parameter struct and
	 * a callback, and only one such write can be outstanding at a time --
	 * which is a real constraint on a bridge, so think about what happens
	 * to the second verb that arrives while the first is in flight.
	 *
	 * A frame longer than the negotiated ATT MTU must be split across
	 * writes, and Roo reassembles. Check bt_gatt_get_mtu(roo_conn) rather
	 * than assuming the 23-byte default: the MTU is negotiated per
	 * connection and is usually much larger.
	 */
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	(void)roo_uuid_rx;
	return -ENOSYS;
}

enum dongle_ble_state ble_central_state(void)
{
	return state;
}

int8_t ble_central_rssi(void)
{
	return state == DONGLE_BLE_CONNECTED ? last_rssi : 0;
}
