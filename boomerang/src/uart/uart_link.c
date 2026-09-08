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
 * MILESTONE M3. Bridge the wire: bytes from the host go out of this UART, and
 * bytes arriving on it go back to the host.
 *
 * Check:  join pad D6 (TX) to pad D7 (RX) with a jumper wire, then
 *         python tools/dongle_probe.py --port <port> --loopback 65536
 *
 * The loopback is the trick that lets you finish this milestone without a Roo
 * on the desk: whatever you transmit comes straight back into your own
 * receiver, so a byte-exact round trip proves both halves at once.
 *
 * THE ASYNC UART API, IN ONE PARAGRAPH. You hand the driver a buffer and it
 * fills it using DMA, interrupting you only when something notable happens.
 * While it is filling buffer A it asks you for buffer B; when A is full you
 * get the data and B takes over seamlessly. Two buffers alternating means the
 * receiver never stops, which at 1 Mbaud matters: a gap of even one character
 * time loses data, because there is no flow control to pause the far end.
 */

#include "uart/uart_link.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bridge/bridge.h"

LOG_MODULE_REGISTER(uart_link, LOG_LEVEL_INF);

/* uart0 is the Roo link. Its pins and baud rate come from devicetree; see
 * boards/xiao_ble_nrf52840.overlay for which pads they are and why. */
static const struct device *const roo_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

/*
 * Two receive buffers, alternating.
 *
 * Sizing: at 1 Mbaud a byte takes 10 us, so a 512-byte buffer fills in about
 * 5 ms. That is the longest you can be late before the second buffer is also
 * full and bytes start hitting the floor. Bigger buffers buy more slack at the
 * cost of RAM and of latency in the RX_RDY event.
 */
#define UART_RX_CHUNK 512
static uint8_t rx_buf[2][UART_RX_CHUNK];
static uint8_t rx_next; /* which buffer to offer when the driver asks */

/* Completion signal for uart_tx(). See uart_link_write(). */
static K_SEM_DEFINE(tx_done, 0, 1);

static enum dongle_uart_state link_state = DONGLE_UART_DOWN;

static void uart_cb(const struct device *dev, struct uart_event *evt,
		    void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_RX_RDY:
		/*
		 * TODO(M3): bytes have arrived.
		 *
		 *   const uint8_t *p = evt->data.rx.buf + evt->data.rx.offset;
		 *   size_t n = evt->data.rx.len;
		 *   bridge_from_roo(p, n);
		 *
		 * Note what this callback must NOT do: block, log at every
		 * event, or take a mutex a slow thread might be holding. It
		 * runs in interrupt context. bridge_from_roo() is written to be
		 * safe here -- it copies into a ring buffer and returns.
		 */
		break;

	case UART_RX_BUF_REQUEST:
		/*
		 * TODO(M3): the driver wants the NEXT buffer, before the
		 * current one is full. Hand it the other one:
		 *
		 *   uart_rx_buf_rsp(dev, rx_buf[rx_next], UART_RX_CHUNK);
		 *   rx_next ^= 1;
		 *
		 * If you ignore this event, reception stops the moment the
		 * current buffer fills, and you get UART_RX_DISABLED instead.
		 * That failure looks like "the first 512 bytes work and then
		 * nothing", which is a very recognisable symptom once you have
		 * seen it.
		 */
		break;

	case UART_RX_BUF_RELEASED:
		/* The driver has finished with a buffer. Nothing to do here
		 * while the buffers are static, but this is where you would
		 * return one to a pool. */
		break;

	case UART_RX_DISABLED:
		/*
		 * TODO(M3): reception has stopped -- either because you did not
		 * supply a buffer in time, or after an error. Restart it:
		 *
		 *   uart_rx_enable(dev, rx_buf[rx_next], UART_RX_CHUNK, 10000);
		 *   rx_next ^= 1;
		 *
		 * Restarting is right, but silently restarting is not: this is
		 * a data-loss event and M4 asks you to count it.
		 */
		break;

	case UART_RX_STOPPED:
		/* A framing, parity or overrun error. evt->data.rx_stop.reason
		 * carries the detail. Record it; do not log per event, or a
		 * disconnected wire will drown you in messages. */
		link_state = DONGLE_UART_ERRORS;
		break;

	case UART_TX_DONE:
	case UART_TX_ABORTED:
		k_sem_give(&tx_done);
		break;

	default:
		break;
	}
}

int uart_link_init(void)
{
	int err;

	if (!device_is_ready(roo_uart)) {
		LOG_ERR("uart0 not ready");
		return -ENODEV;
	}

	err = uart_callback_set(roo_uart, uart_cb, NULL);
	if (err != 0) {
		/* -ENOTSUP here means the async API is not enabled for this
		 * driver instance: check CONFIG_UART_ASYNC_API in prj.conf. */
		LOG_ERR("uart_callback_set failed (%d)", err);
		return err;
	}

	/*
	 * TODO(M3): start receiving.
	 *
	 *   err = uart_rx_enable(roo_uart, rx_buf[0], UART_RX_CHUNK, 10000);
	 *   rx_next = 1;
	 *
	 * The last argument is an inactivity timeout in MICROSECONDS: if the
	 * line goes quiet for that long, the driver hands you what it has
	 * rather than waiting for a full buffer. 10 000 us = 10 ms is a
	 * reasonable compromise between latency and interrupt load. Set it too
	 * low and you interrupt constantly; too high and a short reply sits in
	 * the DMA buffer for ages, which reads as "the dongle ignored my
	 * command".
	 */
	(void)uart_cb;
	link_state = DONGLE_UART_READY;

	LOG_INF("Roo UART up at %d baud",
		DT_PROP(DT_NODELABEL(uart0), current_speed));
	return 0;
}

int uart_link_write(const uint8_t *data, size_t len, int32_t timeout_ms)
{
	/*
	 * TODO(M3):
	 *
	 *   int err = uart_tx(roo_uart, data, len, SYS_FOREVER_US);
	 *   if (err != 0) {
	 *       return err;                    // -EBUSY = a TX is in flight
	 *   }
	 *   if (k_sem_take(&tx_done, K_MSEC(timeout_ms)) != 0) {
	 *       uart_tx_abort(roo_uart);
	 *       return -ETIMEDOUT;
	 *   }
	 *   return (int)len;
	 *
	 * Waiting for the completion semaphore is not optional. uart_tx()
	 * returns as soon as the DMA is armed, and the driver reads from YOUR
	 * buffer while the transfer runs. Return early and the caller is free
	 * to overwrite bytes that have not gone out yet, which produces
	 * corruption that moves around when you add a log line -- the worst
	 * kind of bug to chase.
	 */
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	ARG_UNUSED(timeout_ms);
	return -ENOSYS;
}

enum dongle_uart_state uart_link_state(void)
{
	return link_state;
}
