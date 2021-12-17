/* Copyright (c) 2021-2022, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_DEFAULT_RANDOM_H_
#define ODP_DEFAULT_RANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/random.h>

#include <odp_random_std_internal.h>

#include <stdint.h>

static inline odp_random_kind_t _odp_random_max_kind(void)
{
	return _odp_random_std_max_kind();
}

static inline int32_t _odp_random_true_data(uint8_t *buf, uint32_t len)
{
	return _odp_random_std_true_data(buf, len);
}

static inline int32_t _odp_random_crypto_data(uint8_t *buf, uint32_t len)
{
	return _odp_random_std_crypto_data(buf, len);
}

static inline int32_t _odp_random_basic_data(uint8_t *buf, uint32_t len)
{
	return _odp_random_std_basic_data(buf, len);
}

static int _odp_random_init_local_int(void)
{
	return 0;
}

static int _odp_random_term_local_int(void)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
