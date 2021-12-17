/* Copyright (c) 2020-2022, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/api/random.h>

#include <odp/autoheader_internal.h>
#include <odp_debug_internal.h>
#include <odp_init_internal.h>
#include <odp_random_std_internal.h>

#include <stdint.h>

#if _ODP_OPENSSL_RAND
#include <odp_random_openssl_internal.h>
#else
#include <odp_random.h>
#endif

odp_random_kind_t odp_random_max_kind(void)
{
	return _odp_random_max_kind();
}

int32_t odp_random_data(uint8_t *buf, uint32_t len, odp_random_kind_t kind)
{
	switch (kind) {
	case ODP_RANDOM_BASIC:
		return _odp_random_basic_data(buf, len);
	case ODP_RANDOM_CRYPTO:
		return _odp_random_crypto_data(buf, len);
	case ODP_RANDOM_TRUE:
		return _odp_random_true_data(buf, len);
	}

	return -1;
}

int32_t odp_random_test_data(uint8_t *buf, uint32_t len, uint64_t *seed)
{
	/* All rand implementations use std implementation for test data */
	return _odp_random_std_test_data(buf, len, seed);
}

int _odp_random_init_local(void)
{
	if (_odp_random_std_init_local()) {
		ODP_ERR("Std rand init failed\n");
		return -1;
	}

	return _odp_random_init_local_int();
}

int _odp_random_term_local(void)
{
	if (_odp_random_std_term_local()) {
		ODP_ERR("Std rand term failed\n");
		return -1;
	}

	return _odp_random_term_local_int();
}
