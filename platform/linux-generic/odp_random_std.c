/* Copyright (c) 2014-2018, Linaro Limited
 * Copyright (c) 2022, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_posix_extensions.h>

#include <odp/api/byteorder.h>
#include <odp/api/cpu.h>
#include <odp/api/debug.h>
#include <odp/api/random.h>

#include <odp_init_internal.h>
#include <odp_random_std_internal.h>
#include <odp_cpu.h>

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/*
 * Xorshift64*, adapted from [1], and modified to return only the high 32 bits.
 *
 * [1] An experimental exploration of Marsaglia's xorshift generators, scrambled
 *     Sebastiano Vigna, July 2016.
 *     http://vigna.di.unimi.it/ftp/papers/xorshift.pdf
 */
static inline uint32_t xorshift64s32(uint64_t *x)
{
	/* The variable x should be initialized to a nonzero seed. [1] */
	if (!*x)
		/*
		 * 2^64 / phi. As far away as possible from any small integer
		 * fractions, which the caller might be likely to use for the
		 * next seed after 0.
		 */
		*x = 11400714819323198485ull;

	*x ^= *x >> 12; /* a */
	*x ^= *x << 25; /* b */
	*x ^= *x >> 27; /* c */
	return (*x * 2685821657736338717ull) >> 32;
}

static int32_t _random_data(uint8_t *buf, uint32_t len, uint64_t *seed)
{
	const uint32_t ret = len;

	if (!_ODP_UNALIGNED && ((uintptr_t)buf & 3) && len) {
		uint32_t r = xorshift64s32(seed);

		if ((uintptr_t)buf & 1) {
			*(uint8_t *)(uintptr_t)buf = r & 0xff;
			r >>= 8;
			buf += 1;
			len -= 1;
		}

		if (((uintptr_t)buf & 2) && len >= 2) {
			*(uint16_t *)(uintptr_t)buf = r & 0xffff;
			buf += 2;
			len -= 2;
		}
	}

	for (uint32_t i = 0; i < len / 4; i++) {
		*(uint32_t *)(uintptr_t)buf = xorshift64s32(seed);
		buf += 4;
	}

	if (len & 3) {
		uint32_t r = xorshift64s32(seed);

		if (len & 2) {
			*(uint16_t *)(uintptr_t)buf = r & 0xffff;
			r >>= 16;
			buf += 2;
		}

		if (len & 1)
			*(uint8_t *)(uintptr_t)buf = r & 0xff;
	}

	return ret;
}

int32_t _odp_random_std_test_data(uint8_t *buf, uint32_t len, uint64_t *seed)
{
	return _random_data(buf, len, seed);
}

static __thread uint64_t this_seed;

int32_t _odp_random_std_basic_data(uint8_t *buf, uint32_t len)
{
	return _random_data(buf, len, &this_seed);
}

int32_t _odp_random_std_crypto_data(uint8_t *buf, uint32_t len)
{
	(void)buf;
	(void)len;

	return -1;
}

int32_t _odp_random_std_true_data(uint8_t *buf, uint32_t len)
{
	(void)buf;
	(void)len;

	return -1;
}

int _odp_random_std_init_local(void)
{
	this_seed = time(NULL);
	this_seed ^= (uint64_t)odp_cpu_id() << 32;

	return 0;
}

int _odp_random_std_term_local(void)
{
	return 0;
}

odp_random_kind_t _odp_random_std_max_kind(void)
{
	return ODP_RANDOM_BASIC;
}
