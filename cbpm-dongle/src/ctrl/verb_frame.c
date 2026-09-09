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
 * MILESTONE M2. This file is the first real thing you implement, and it is
 * deliberately the one with no hardware in it: you can run the tests on your
 * laptop in under a second.
 *
 *     west twister -T tests/verb_frame --platform unit_testing --inline-logs
 *
 * Watch it fail first. Then make it pass. Then read the test file and see
 * whether you can think of a case it does not cover -- there are at least two.
 */

#include "ctrl/verb_frame.h"
#include "verb_frame.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Little-endian helpers.
 *
 * Why not just cast the buffer to a uint16_t pointer? Two reasons, and both
 * bite in real life:
 *
 *   1. Alignment. A uint16_t read from an odd address is undefined behaviour
 *      in C, and on some architectures it faults. Your frame's length field
 *      sits at offset 4 today, but it only takes one protocol revision to
 *      land it on an odd offset.
 *   2. Endianness. It would work on this chip and break the day the code is
 *      reused on a big-endian one. Byte-at-a-time is explicit about what the
 *      wire holds, and the compiler optimises it back into a single load on
 *      platforms where that is legal anyway. You lose nothing.
 */
static inline uint16_t get_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)(v >> 8);
}

/*
function verb_crc16 completes a CRC-16 CCIT-False check on the incoming data
*/
uint16_t verb_crc16(const uint8_t *data, size_t len)
{
	/*
	 * TODO(M2): implement CRC-16/CCITT-FALSE.
	 *
	 * The algorithm, in full, because looking it up gets you four
	 * incompatible answers:
	 *
	 *   crc = 0xFFFF
	 *   for each byte b:
	 *       crc ^= (b << 8)                 // XOR the byte into the TOP
	 *       repeat 8 times:
	 *           if crc & 0x8000:            // test the top bit
	 *               crc = (crc << 1) ^ 0x1021
	 *           else:
	 *               crc = crc << 1
	 *           crc &= 0xFFFF               // keep it 16 bits
	 *   return crc
	 *
	 * Note there is no final XOR and no bit reflection. The variant that
	 * reflects the input and seeds with 0x0000 is also called "CRC16-CCITT"
	 * by plenty of libraries, and it produces completely different numbers.
	 * That ambiguity is why the test carries explicit vectors.
	 *
	 * Keep `crc` in a uint16_t and the masking takes care of itself.
	 */

	uint16_t crc = 0xFFFF;

	for (int j=0; j<len; j++) {
		uint16_t b = data[j];
        crc ^= (b << 8);
		for (int i=0; i<8; i++) {
			if (crc & 0x8000) {
				crc = (crc << 1)^0x1021;
			} else {
				crc = crc << 1;
			}
			crc &= 0xFFFF;
		}
    }

	return crc;
}

/*
verb_parse_result checks the head of the buffer to ensure frames are received 
correctly and in full
*/
enum verb_parse_result verb_frame_parse(const uint8_t *buf, size_t len,
					struct verb_frame *out, size_t *consumed)
{	
	uint16_t arg_len = get_le16(buf + 4);
	uint16_t frame_len = VERB_HEADER_LEN + arg_len + VERB_CRC_LEN;
	
	if (len<1) {
		// length of 0, nothing received
		consumed = (size_t)0;
		return VERB_PARSE_INCOMPLETE;
	} else if (buf[0] != VERB_SYNC) {
		// incorrect first frame
		consumed = (size_t)1;
		return VERB_PARSE_BAD_SYNC;
	} else if (len < VERB_HEADER_LEN) {
		// incomplete header
		consumed = (size_t)0;
		return VERB_PARSE_INCOMPLETE;
	} else if (arg_len > VERB_MAX_ARGS) {
		// length too long, input must be invalid
		consumed = (size_t)1;
		return VERB_PARSE_TOO_LONG;
	} else if (len < frame_len) {
		consumed = (size_t)0;
		return VERB_HEADER_LEN;
	} else if ((verb_crc16(buf, frame_len-VERB_CRC_LEN))!=get_le16(buf + frame_len - VERB_CRC_LEN)) {
		consumed = (size_t)frame_len;
		return VERB_PARSE_BAD_CRC;
	} else {
		return VERB_PARSE_OK;
	}

	/*
	 * TODO(M2): parse one frame from the head of the buffer.
	 *
	 * The order of the checks matters, and this order is chosen so that a
	 * malicious or corrupt length field can never make you read past the
	 * end of the buffer:
	 *
	 *   1. len < 1                  -> INCOMPLETE, consumed = 0
	 *   2. buf[0] != VERB_SYNC      -> BAD_SYNC, consumed = 1
	 *   3. len < VERB_HEADER_LEN    -> INCOMPLETE, consumed = 0
	 *      (you cannot trust the length field until you have all of it)
	 *   4. arg_len = get_le16(buf + 4)
	 *      arg_len > VERB_MAX_ARGS  -> TOO_LONG, consumed = 1
	 *   5. frame_len = VERB_HEADER_LEN + arg_len + VERB_CRC_LEN
	 *      len < frame_len          -> INCOMPLETE, consumed = 0
	 *   6. CRC over the first (frame_len - VERB_CRC_LEN) bytes, compared
	 *      against get_le16(buf + frame_len - VERB_CRC_LEN)
	 *      mismatch                 -> BAD_CRC, consumed = frame_len
	 *   7. otherwise fill *out and  -> OK, consumed = frame_len
	 *
	 * Step 4's "consumed = 1" rather than "skip the whole frame" is
	 * deliberate: an implausible length means the 0xC0 you found was
	 * probably not a header at all, just a data byte that happened to have
	 * that value. Skipping one byte lets you find the real header behind
	 * it. Skipping a bogus frame_len would throw good data away.
	 *
	 * Set every field of *out on success, including args (which points at
	 * buf + VERB_HEADER_LEN, or may be left pointing there even when
	 * arg_len is 0 -- the test does not care, but be consistent).
	 */
	
}

size_t verb_frame_encode(uint8_t verb, uint16_t request_id, const uint8_t *args,
			 uint16_t arg_len, uint8_t *out, size_t out_cap)
{
	size_t total_len = VERB_HEADER_LEN + arg_len + VERB_CRC_LEN;

	if (arg_len > VERB_MAX_ARGS || out_cap < total_len) {
		return 0;
	}

	out[0] = VERB_SYNC;
	out[1] = verb;

	put_le16(out + 2, request_id);
	put_le16(out + 4, arg_len);

	if (arg_len > 0) {
		memcpy(out + VERB_HEADER_LEN, args, arg_len);
	}

	uint16_t crc = verb_crc16(out, VERB_HEADER_LEN + arg_len);
	put_le16(out + VERB_HEADER_LEN + arg_len, crc);

	return total_len;

	/*
	 * TODO(M2): build a frame.
	 *
	 *   1. Refuse if arg_len > VERB_MAX_ARGS, or if out_cap is smaller than
	 *      VERB_HEADER_LEN + arg_len + VERB_CRC_LEN. Return 0.
	 *   2. out[0] = VERB_SYNC; out[1] = verb;
	 *      put_le16(out + 2, request_id); put_le16(out + 4, arg_len);
	 *   3. memcpy the arguments in, if any. Guard the NULL case: memcpy
	 *      with a NULL source is undefined behaviour even when the length
	 *      is zero, which is a genuinely surprising piece of C.
	 *   4. CRC over the header and arguments, then put_le16 it at the end.
	 *   5. Return the total length.
	 *
	 * This function and verb_frame_parse() are inverses. The test proves
	 * that with a round trip, which is the cheapest kind of protocol test
	 * to write and the one most likely to catch a silly mistake.
	 */
}