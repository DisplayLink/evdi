// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <kunit/test.h>
#include <drm/drm_fixed.h>
#include <drm/drm_mode.h>

#include "evdi_color.h"

static void test_identity_is_noop(struct kunit *test)
{
	struct evdi_color_data data = { 0 };
	u8 row[8] = { 10, 20, 30, 40, 200, 100, 50, 255 };
	u8 expected[8];

	memcpy(expected, row, sizeof(row));

	evdi_color_transform_apply_row(&data, row, 2, false);

	KUNIT_EXPECT_MEMEQ(test, row, expected, sizeof(row));
}

static void test_gamma_lut_applied_per_channel(struct kunit *test)
{
	struct evdi_color_data data = { 0 };
	u8 row[4] = { 10, 20, 30, 0xAA }; /* B, G, R, X (XRGB8888 byte order) */
	int i;

	data.active = true;
	data.has_gamma = true;
	for (i = 0; i < EVDI_GAMMA_LUT_SIZE; ++i) {
		data.gamma[0][i] = 255 - i; /* invert blue */
		data.gamma[1][i] = i;       /* identity green */
		data.gamma[2][i] = 0;       /* zero red */
	}

	evdi_color_transform_apply_row(&data, row, 1, false);

	KUNIT_EXPECT_EQ(test, row[0], (u8)(255 - 10));
	KUNIT_EXPECT_EQ(test, row[1], (u8)20);
	KUNIT_EXPECT_EQ(test, row[2], (u8)0);
	KUNIT_EXPECT_EQ(test, row[3], (u8)0xAA); /* X/alpha byte untouched */
}

static void test_ctm_swaps_channels(struct kunit *test)
{
	struct evdi_color_data data = { 0 };
	/* out = [ [0 0 1] [0 1 0] [1 0 0] ] x in : swaps R and B, keeps G */
	u8 row[4] = { 10, 20, 30, 0 }; /* B=10 G=20 R=30 (XRGB8888) */

	data.active = true;
	data.has_ctm = true;
	data.ctm[0][2] = drm_int2fixp(1); /* out_r = in_b */
	data.ctm[1][1] = drm_int2fixp(1); /* out_g = in_g */
	data.ctm[2][0] = drm_int2fixp(1); /* out_b = in_r */

	evdi_color_transform_apply_row(&data, row, 1, false);

	/* row[0]=B should become old R (30), row[2]=R should become old B (10) */
	KUNIT_EXPECT_EQ(test, row[0], (u8)30);
	KUNIT_EXPECT_EQ(test, row[1], (u8)20);
	KUNIT_EXPECT_EQ(test, row[2], (u8)10);
}

static void test_swap_rb_targets_correct_bytes(struct kunit *test)
{
	struct evdi_color_data data = { 0 };
	u8 row_xrgb[4] = { 1, 2, 3, 9 };  /* B, G, R, X */
	u8 row_xbgr[4] = { 3, 2, 1, 9 };  /* R, G, B, X - same logical pixel */
	int i;

	data.active = true;
	data.has_gamma = true;
	for (i = 0; i < EVDI_GAMMA_LUT_SIZE; ++i) {
		data.gamma[0][i] = i * 2 > 255 ? 255 : i * 2; /* boost red */
		data.gamma[1][i] = i;
		data.gamma[2][i] = i;
	}

	evdi_color_transform_apply_row(&data, row_xrgb, 1, false);
	evdi_color_transform_apply_row(&data, row_xbgr, 1, true);

	/* Both encode the same logical (R,G,B); result must match once
	 * accounting for the swapped byte layout.
	 */
	KUNIT_EXPECT_EQ(test, row_xrgb[2], row_xbgr[0]); /* R byte */
	KUNIT_EXPECT_EQ(test, row_xrgb[1], row_xbgr[1]); /* G byte */
	KUNIT_EXPECT_EQ(test, row_xrgb[0], row_xbgr[2]); /* B byte */
	KUNIT_EXPECT_EQ(test, row_xrgb[3], (u8)9);
	KUNIT_EXPECT_EQ(test, row_xbgr[3], (u8)9);
}

static struct kunit_case evdi_color_test_cases[] = {
	KUNIT_CASE(test_identity_is_noop),
	KUNIT_CASE(test_gamma_lut_applied_per_channel),
	KUNIT_CASE(test_ctm_swaps_channels),
	KUNIT_CASE(test_swap_rb_targets_correct_bytes),
	{}
};

static struct kunit_suite evdi_color_test_suite = {
	.name = "drm_evdi_color_tests",
	.test_cases = evdi_color_test_cases,
};

kunit_test_suite(evdi_color_test_suite);

MODULE_LICENSE("GPL");
