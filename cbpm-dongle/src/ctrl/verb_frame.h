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
 * @file verb_frame.h
 * @brief The verb framing that host commands travel in. Pure logic, no I/O.
 *
 * A "verb" is one command from the laptop: ping, start a session, set a
 * config, ask the dongle for its status. The framing below is not ours to
 * invent -- it is fixed by the CBPM host contract and the reference
 * implementation is scripts/gui/verbs.py in the firmware repo. Your job is to
 * mirror it exactly.
 *
 * WIRE LAYOUT (all multi-byte fields little-endian):
 *
 *      offset  size  field
 *      0       1     sync = 0xC0
 *      1       1     verb id
 *      2       2     request id (the host's; echo it back unchanged)
 *      4       2     argument length, N
 *      6       N     arguments
 *      6+N     2     CRC16-CCITT over bytes 0 .. 6+N-1 inclusive
 *
 * The CRC is CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF,
 * no reflection of input or output, no final XOR. Getting any one of those
 * four wrong gives you a checksum that looks plausible and never matches, so
 * the unit test in tests/verb_frame checks your implementation against known
 * vectors before you ever put it on a wire.
 *
 * WHY THIS FILE HAS NO ZEPHYR INCLUDES. Everything here is arithmetic over a
 * byte buffer: no threads, no drivers, no devicetree. That makes it testable
 * on your laptop in a fraction of a second (see tests/verb_frame), which is
 * worth more than it sounds -- protocol bugs are miserable to chase on
 * hardware and trivial to chase in a unit test. Keep it that way: if you find
 * yourself wanting to log or read a device here, the code belongs in
 * dongle_ctrl.c instead.
 */

#ifndef CBPM_DONGLE_CTRL_VERB_FRAME_H_
#define CBPM_DONGLE_CTRL_VERB_FRAME_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** First byte of every verb frame. */
#define VERB_SYNC 0xC0u

/** Bytes before the arguments: sync, verb, request id, argument length. */
#define VERB_HEADER_LEN 6u

/** Bytes after the arguments. */
#define VERB_CRC_LEN 2u

/**
 * Largest argument block the protocol defines today: the 192-byte PPG slot
 * table (VERB_SET_PPG_SLOT_CONFIG). Rounded up, and enforced, so that a
 * corrupt length field cannot make us try to buffer 65 535 bytes on a device
 * with 256 KiB of RAM. Refusing absurd input is not paranoia, it is the
 * cheapest defence there is.
 */
#define VERB_MAX_ARGS 256u

/** Largest complete frame. Size your buffers with this, not with a guess. */
#define VERB_MAX_FRAME (VERB_HEADER_LEN + VERB_MAX_ARGS + VERB_CRC_LEN)

/*
 * Verb ids the DONGLE answers itself, and never forwards to Roo.
 *
 * The band 0xD0..0xDF is reserved for the dongle. It sits well clear of the
 * Roo-bound verbs, which today run from 0x01 (ping) to 0x0D (set chip tuning),
 * so there is no risk of the dongle swallowing a command meant for the far
 * end. This allocation is marked PROPOSED in the contract: it is a sensible
 * choice, not yet a ratified one.
 */
#define DONGLE_VERB_BAND_FIRST 0xD0u
#define DONGLE_VERB_BAND_LAST  0xDFu
#define DONGLE_VERB_GET_STATUS 0xD0u
#define DONGLE_VERB_SET_MODE   0xD1u

/**
 * Debug only, and only in a learning build: stall a bridge thread on purpose
 * so the watchdog has something to catch. M6 implements it, and a real
 * shipping dongle would compile it out. A deliberate way to break your own
 * device is the only honest way to prove a recovery mechanism works -- a
 * watchdog you have never seen fire is a watchdog you have not tested.
 */
#define DONGLE_VERB_DEBUG_STALL 0xDFu

/** @brief True if @p verb belongs to the dongle rather than to Roo. */
static inline bool verb_is_dongle_local(uint8_t verb)
{
	return verb >= DONGLE_VERB_BAND_FIRST && verb <= DONGLE_VERB_BAND_LAST;
}

/** @brief One parsed frame. @p args points INTO the caller's buffer -- it is
 *         not a copy, and it stops being valid the moment that buffer is
 *         reused. Copy anything you need to keep.
 */
struct verb_frame {
	uint8_t verb;
	uint16_t request_id;
	uint16_t arg_len;
	const uint8_t *args;
};

/**
 * @brief What verb_frame_parse() found at the head of the buffer.
 *
 * An enum rather than an errno, because the caller must react differently to
 * each case and a test should be able to assert exactly which one happened.
 */
enum verb_parse_result {
	/** A complete, CRC-valid frame. @p out and @p consumed are set. */
	VERB_PARSE_OK = 0,
	/** A valid start, but the whole frame has not arrived yet. Wait for
	 *  more bytes and call again with the longer buffer. NOT an error:
	 *  this is the normal case on a byte stream. */
	VERB_PARSE_INCOMPLETE,
	/** The first byte is not the sync byte. @p consumed is set to 1 so the
	 *  caller can skip it and re-try -- that is how you re-synchronise
	 *  after noise or a truncated frame. */
	VERB_PARSE_BAD_SYNC,
	/** Structurally complete, checksum wrong. @p consumed is set to the
	 *  frame length so the caller can step over the whole bad frame. */
	VERB_PARSE_BAD_CRC,
	/** The length field exceeds VERB_MAX_ARGS. Treated like bad sync:
	 *  skip one byte and re-synchronise, because a length that large means
	 *  we were not looking at a real header. */
	VERB_PARSE_TOO_LONG,
};

/**
 * @brief CRC-16/CCITT-FALSE over @p len bytes at @p data.
 *
 * @note Zephyr ships crc16_itu_t() in <zephyr/sys/crc.h>, which computes the
 *       same thing when seeded with 0xFFFF. Write your own first anyway: this
 *       is eight lines of code and understanding it once means never again
 *       wondering why two "CRC16"s disagree. Swap to the library version
 *       afterwards if you like, and watch the test still pass.
 */
uint16_t verb_crc16(const uint8_t *data, size_t len);

/**
 * @brief Try to parse one frame from the head of @p buf.
 *
 * @param buf       Bytes received so far.
 * @param len       How many bytes are valid at @p buf.
 * @param out       Filled in on VERB_PARSE_OK. Untouched otherwise.
 * @param consumed  Bytes the caller should discard from the head of @p buf.
 *                  Set on OK, BAD_SYNC, BAD_CRC and TOO_LONG. Set to 0 on
 *                  INCOMPLETE (discard nothing, wait for more).
 *
 * @return one of @ref verb_parse_result.
 */
enum verb_parse_result verb_frame_parse(const uint8_t *buf, size_t len,
					struct verb_frame *out, size_t *consumed);

/**
 * @brief Build a complete frame into @p out.
 *
 * Used for the dongle's own replies. @p args may be NULL when @p arg_len is 0.
 *
 * @return the number of bytes written, or 0 if @p out_cap is too small or the
 *         arguments are too long. Zero is a failure that is impossible to
 *         ignore by accident, which is why it is not a negative errno here.
 */
size_t verb_frame_encode(uint8_t verb, uint16_t request_id, const uint8_t *args,
			 uint16_t arg_len, uint8_t *out, size_t out_cap);

#endif /* CBPM_DONGLE_CTRL_VERB_FRAME_H_ */
