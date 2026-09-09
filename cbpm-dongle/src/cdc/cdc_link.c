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
 * MILESTONE M1. Make the board appear as a serial port on the laptop, and echo
 * everything it sends.
 *
 * Check:  ./scripts/check.sh   (row M1)
 *         python tools/dongle_probe.py --port <your port> --echo
 *
 * The shape of this file is the standard Zephyr CDC-ACM pattern, and it is
 * worth understanding rather than copying, because every interrupt-driven UART
 * in Zephyr looks like this:
 *
 *   - A ring buffer in each direction, because an interrupt handler must never
 *     block and must never do slow work.
 *   - An ISR callback that does nothing but move bytes between the hardware
 *     FIFO and those ring buffers.
 *   - Threads elsewhere that fill the outbound buffer and drain the inbound
 *     one at their own pace.
 *
 * The ISR is the fast, dumb half; the threads are the slow, clever half. Keep
 * that boundary and concurrency bugs mostly stop happening.
 */

#include "cdc/cdc_link.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(cdc_link, LOG_LEVEL_INF);

/*
 * The CDC-ACM port comes from devicetree, not from a string name. The board's
 * devicetree declares a node labelled cdc_acm_uart0 (see the comments in
 * boards/xiao_ble_nrf52840.overlay), and DEVICE_DT_GET turns that label into a
 * device pointer AT COMPILE TIME. If the node does not exist the build fails
 * with a clear message, which is exactly what you want -- far better than a
 * NULL pointer at runtime on a bench.
 */
static const struct device *const cdc_dev =
	DEVICE_DT_GET(DT_NODELABEL(board_cdc_acm_uart));
	// note: on Momo's machine the correct label is 'board_cdc_acm_uart', other versions may use label 'cdc_acm_uart0'

/* Ring buffers. RING_BUF_DECLARE allocates the storage for you. */
RING_BUF_DECLARE(cdc_tx_rb, CONFIG_CBPM_DONGLE_RING_SIZE);
RING_BUF_DECLARE(cdc_rx_rb, CONFIG_CBPM_DONGLE_RING_SIZE);

/*
 * Signalled by the ISR when bytes land, waited on by cdc_link_read(). A
 * semaphore rather than a busy-poll: the reading thread sleeps until there is
 * work, which is both faster and lower power than asking repeatedly.
 *
 * Initial count 0, maximum 1 -- it is a "something happened" flag, not a
 * counter of how many bytes arrived.
 */
static K_SEM_DEFINE(cdc_rx_sem, 0, 1);

static void cdc_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	/*
	 * TODO(M1): the interrupt handler.
	 *
	 *   while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
	 *
	 *       if (uart_irq_rx_ready(dev)) {
	 *           uint8_t buf[64];
	 *           int n = uart_fifo_read(dev, buf, sizeof(buf));
	 *           if (n > 0) {
	 *               // Put what fits. What does not fit is LOST -- and in
	 *               // M4 you will count it rather than ignore it.
	 *               ring_buf_put(&cdc_rx_rb, buf, n);
	 *               k_sem_give(&cdc_rx_sem);
	 *           }
	 *       }
	 *
	 *       if (uart_irq_tx_ready(dev)) {
	 *           uint8_t *data;
	 *           uint32_t claimed = ring_buf_get_claim(&cdc_tx_rb, &data, 64);
	 *           if (claimed == 0) {
	 *               // Nothing left to send. Disable the TX interrupt or it
	 *               // fires forever and the device spends its whole life
	 *               // in this handler. This is THE classic mistake.
	 *               uart_irq_tx_disable(dev);
	 *           } else {
	 *               int sent = uart_fifo_fill(dev, data, claimed);
	 *               ring_buf_get_finish(&cdc_tx_rb, MAX(sent, 0));
	 *           }
	 *       }
	 *   }
	 *
	 * uart_irq_update() must be called before the _ready() queries: it
	 * latches the current interrupt state so the answers are consistent.
	 *
	 * ring_buf_get_claim() hands you a pointer straight into the ring so
	 * you can pass it to the driver without an intermediate copy; the
	 * matching get_finish() tells the ring how much you actually used.
	 */
	ARG_UNUSED(dev);
}

int cdc_link_init(void)
{
	int err;

	if (!device_is_ready(cdc_dev)) {
		LOG_ERR("CDC-ACM device not ready");
		return -ENODEV;
	}

	/*
	 * usb_enable() starts the device stack and begins enumeration. We call
	 * it here rather than letting Zephyr do it before main() (see
	 * CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT in prj.conf) so that the order
	 * of events is visible in code you can read.
	 */
	err = usb_enable(NULL);
	if (err != 0 && err != -EALREADY) {
		LOG_ERR("usb_enable failed (%d)", err);
		return err;
	}

	/*
	 * TODO(M1): finish the bring-up.
	 *
	 *   uart_irq_callback_set(cdc_dev, cdc_isr);
	 *   uart_irq_rx_enable(cdc_dev);
	 *
	 * Do NOT enable the TX interrupt here. It gets enabled by
	 * cdc_link_write() when there is something to send, and the ISR
	 * disables it again when the buffer empties. An always-on TX interrupt
	 * with an empty buffer is an interrupt storm.
	 */
	(void)cdc_isr;

	LOG_INF("CDC-ACM up (buffers %u bytes each way)",
		CONFIG_CBPM_DONGLE_RING_SIZE);
	return 0;
}

size_t cdc_link_write(const uint8_t *data, size_t len)
{
	/*
	 * TODO(M1):
	 *
	 *   uint32_t put = ring_buf_put(&cdc_tx_rb, data, len);
	 *   if (put > 0) {
	 *       uart_irq_tx_enable(cdc_dev);   // wake the ISR up
	 *   }
	 *   return put;
	 *
	 * Returning `put` rather than asserting on a short write is the whole
	 * point: a full buffer is a normal condition on a link whose far end
	 * has stopped reading, and the caller is the one who knows whether to
	 * drop, retry or count it.
	 */
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	return 0;
}

size_t cdc_link_read(uint8_t *buf, size_t cap, k_timeout_t timeout)
{
	/*
	 * TODO(M1):
	 *
	 *   uint32_t n = ring_buf_get(&cdc_rx_rb, buf, cap);
	 *   if (n > 0) {
	 *       return n;                       // fast path, no waiting
	 *   }
	 *   if (k_sem_take(&cdc_rx_sem, timeout) != 0) {
	 *       return 0;                       // nothing arrived in time
	 *   }
	 *   return ring_buf_get(&cdc_rx_rb, buf, cap);
	 *
	 * Read first, wait second. If you wait first you will sooner or later
	 * sleep on a semaphore whose give() happened a microsecond before you
	 * looked, with data sitting in the buffer the whole time.
	 */
	ARG_UNUSED(buf);
	ARG_UNUSED(cap);
	ARG_UNUSED(timeout);
	return 0;
}

bool cdc_link_host_present(void)
{
	uint32_t dtr = 0;

	/*
	 * DTR ("data terminal ready") is asserted by the host when a program
	 * opens the port. Zephyr surfaces it through the line-control API.
	 * Returns a negative errno if the driver does not support the query,
	 * in which case assume a host is there rather than refusing to work.
	 */
	if (uart_line_ctrl_get(cdc_dev, UART_LINE_CTRL_DTR, &dtr) != 0) {
		return true;
	}
	return dtr != 0;
}
