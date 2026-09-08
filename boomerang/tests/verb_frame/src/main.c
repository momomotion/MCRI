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
 * Unit tests for the verb framing (milestone M2).
 *
 * Run them:
 *     west twister -T tests/verb_frame --platform unit_testing --inline-logs
 *
 * They will fail until you implement src/ctrl/verb_frame.c. That is the point:
 * read the failures, they describe the specification more precisely than any
 * prose can.
 *
 * The vectors below were generated from the reference host implementation
 * (scripts/gui/verbs.py in the firmware repo), so agreeing with them means
 * agreeing with the laptop. That is a stronger statement than "my encoder and
 * my decoder agree with each other", which is the trap of testing a codec only
 * by round trip.
 */

#include <zephyr/ztest.h>

#include <string.h>

#include "ctrl/verb_frame.h"

ZTEST_SUITE(verb_frame, NULL, NULL, NULL, NULL, NULL);

/* ------------------------------------------------------------------------- */
/* CRC                                                                       */
/* ------------------------------------------------------------------------- */

ZTEST(verb_frame, test_crc_standard_check_value)
{
	/*
	 * Every CRC variant has a published "check value": the CRC of the nine
	 * ASCII characters "123456789". For CRC-16/CCITT-FALSE it is 0x29B1.
	 * If this passes, your polynomial, seed, bit order and final XOR are
	 * all correct, and no other test here can fail for a CRC reason.
	 */
	zassert_equal(verb_crc16((const uint8_t *)"123456789", 9), 0x29B1,
		      "CRC-16/CCITT-FALSE check value must be 0x29B1");
}

ZTEST(verb_frame, test_crc_empty_input)
{
	/* The CRC of nothing is the seed, untouched. A surprising number of
	 * implementations get this wrong by special-casing the empty case. */
	zassert_equal(verb_crc16((const uint8_t *)"", 0), 0xFFFF, NULL);
}

/* ------------------------------------------------------------------------- */
/* Encoding                                                                  */
/* ------------------------------------------------------------------------- */

ZTEST(verb_frame, test_encode_matches_host_vector)
{
	/* DONGLE_GET_STATUS, request id 1, no arguments. */
	static const uint8_t expect[] = {
		0xC0, 0xD0, 0x01, 0x00, 0x00, 0x00, 0x76, 0xF1
	};
	uint8_t out[VERB_MAX_FRAME];
	size_t n = verb_frame_encode(DONGLE_VERB_GET_STATUS, 1, NULL, 0, out,
				     sizeof(out));

	zassert_equal(n, sizeof(expect), "expected %u bytes, got %u",
		      (unsigned)sizeof(expect), (unsigned)n);
	zassert_mem_equal(out, expect, sizeof(expect), "frame bytes differ");
}

ZTEST(verb_frame, test_encode_with_arguments)
{
	/* DONGLE_SET_MODE, request id 0x1234, one argument byte 0x01. Note the
	 * little-endian request id on the wire: 0x34 then 0x12. */
	static const uint8_t expect[] = {
		0xC0, 0xD1, 0x34, 0x12, 0x01, 0x00, 0x01, 0xF9, 0x32
	};
	static const uint8_t args[] = { 0x01 };
	uint8_t out[VERB_MAX_FRAME];
	size_t n = verb_frame_encode(DONGLE_VERB_SET_MODE, 0x1234, args,
				     sizeof(args), out, sizeof(out));

	zassert_equal(n, sizeof(expect), NULL);
	zassert_mem_equal(out, expect, sizeof(expect), NULL);
}

ZTEST(verb_frame, test_encode_refuses_small_buffer)
{
	uint8_t out[4];

	zassert_equal(verb_frame_encode(DONGLE_VERB_GET_STATUS, 1, NULL, 0, out,
					sizeof(out)),
		      0, "must refuse rather than overrun the buffer");
}

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

ZTEST(verb_frame, test_parse_good_frame)
{
	/* A Roo-bound ping with eight argument bytes. */
	static const uint8_t frame[] = {
		0xC0, 0x01, 0xEF, 0xBE, 0x08, 0x00,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x05, 0xD3
	};
	struct verb_frame vf;
	size_t consumed = 0;

	zassert_equal(verb_frame_parse(frame, sizeof(frame), &vf, &consumed),
		      VERB_PARSE_OK, NULL);
	zassert_equal(consumed, sizeof(frame), "must consume the whole frame");
	zassert_equal(vf.verb, 0x01, NULL);
	zassert_equal(vf.request_id, 0xBEEF, NULL);
	zassert_equal(vf.arg_len, 8, NULL);
	zassert_equal(vf.args[7], 0x07, "args must point at the payload");
}

ZTEST(verb_frame, test_parse_incomplete_waits)
{
	static const uint8_t partial[] = { 0xC0, 0xD0, 0x01, 0x00 };
	struct verb_frame vf;
	size_t consumed = 12345;

	zassert_equal(verb_frame_parse(partial, sizeof(partial), &vf, &consumed),
		      VERB_PARSE_INCOMPLETE, NULL);
	zassert_equal(consumed, 0,
		      "an incomplete frame must consume nothing, or the bytes "
		      "you need are gone by the time the rest arrives");
}

ZTEST(verb_frame, test_parse_bad_sync_skips_one_byte)
{
	static const uint8_t noise[] = { 0x00, 0xC0, 0xD0, 0x01, 0x00, 0x00,
					 0x00, 0x76, 0xF1 };
	struct verb_frame vf;
	size_t consumed = 0;

	zassert_equal(verb_frame_parse(noise, sizeof(noise), &vf, &consumed),
		      VERB_PARSE_BAD_SYNC, NULL);
	zassert_equal(consumed, 1,
		      "skip exactly one byte so the real frame behind the "
		      "noise is still found");

	/* And now the caller retries past the junk byte and it works. This is
	 * re-synchronisation, and it is why a stream protocol survives a
	 * cable being unplugged mid-frame. */
	zassert_equal(verb_frame_parse(noise + 1, sizeof(noise) - 1, &vf,
				       &consumed),
		      VERB_PARSE_OK, NULL);
}

ZTEST(verb_frame, test_parse_bad_crc_is_rejected)
{
	uint8_t frame[] = {
		0xC0, 0xD0, 0x01, 0x00, 0x00, 0x00, 0x76, 0xF1
	};
	struct verb_frame vf;
	size_t consumed = 0;

	frame[6] ^= 0xFF; /* corrupt the checksum */

	zassert_equal(verb_frame_parse(frame, sizeof(frame), &vf, &consumed),
		      VERB_PARSE_BAD_CRC, NULL);
	zassert_equal(consumed, sizeof(frame),
		      "step over the whole bad frame, not one byte");
}

ZTEST(verb_frame, test_parse_refuses_absurd_length)
{
	/* Length field says 0xFFFF. A real header never says that, so this is
	 * noise that happens to start with 0xC0. Refuse it and re-synchronise
	 * rather than waiting forever for 65 535 bytes that will never come. */
	static const uint8_t bogus[] = { 0xC0, 0x01, 0x00, 0x00, 0xFF, 0xFF };
	struct verb_frame vf;
	size_t consumed = 0;

	zassert_equal(verb_frame_parse(bogus, sizeof(bogus), &vf, &consumed),
		      VERB_PARSE_TOO_LONG, NULL);
	zassert_equal(consumed, 1, NULL);
}

/* ------------------------------------------------------------------------- */
/* Round trip and band                                                       */
/* ------------------------------------------------------------------------- */

ZTEST(verb_frame, test_round_trip)
{
	uint8_t args[64];
	uint8_t buf[VERB_MAX_FRAME];
	struct verb_frame vf;
	size_t consumed = 0;
	size_t n;

	for (size_t i = 0; i < sizeof(args); i++) {
		args[i] = (uint8_t)(i * 7);
	}

	n = verb_frame_encode(0x09, 0xABCD, args, sizeof(args), buf, sizeof(buf));
	zassert_true(n > 0, "encode failed");

	zassert_equal(verb_frame_parse(buf, n, &vf, &consumed), VERB_PARSE_OK,
		      NULL);
	zassert_equal(vf.verb, 0x09, NULL);
	zassert_equal(vf.request_id, 0xABCD, NULL);
	zassert_equal(vf.arg_len, sizeof(args), NULL);
	zassert_mem_equal(vf.args, args, sizeof(args), NULL);
}

ZTEST(verb_frame, test_dongle_band)
{
	/* The band is ours; everything below it belongs to Roo. Getting this
	 * predicate wrong means either swallowing Roo's commands or forwarding
	 * our own, and both fail in ways that look like the far end's fault. */
	zassert_true(verb_is_dongle_local(DONGLE_VERB_GET_STATUS), NULL);
	zassert_true(verb_is_dongle_local(DONGLE_VERB_SET_MODE), NULL);
	zassert_true(verb_is_dongle_local(0xDF), NULL);
	zassert_false(verb_is_dongle_local(0xCF), NULL);
	zassert_false(verb_is_dongle_local(0xE0), NULL);
	zassert_false(verb_is_dongle_local(0x01), "ping belongs to Roo");
	zassert_false(verb_is_dongle_local(0x0D), NULL);
}
