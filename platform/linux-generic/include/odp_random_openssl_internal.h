/* Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_RANDOM_OPENSSL_INTERNAL_H_
#define ODP_RANDOM_OPENSSL_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/random.h>

#include <odp/autoheader_internal.h>
#include <odp_init_internal.h>

#include <openssl/rand.h>

#include <stdint.h>

static inline odp_random_kind_t _odp_random_max_kind(void)
{
	return ODP_RANDOM_CRYPTO;
}

static inline int32_t _odp_random_openssl_data(uint8_t *buf, uint32_t len)
{
	int rc;

	rc = RAND_bytes(buf, len);
	return (1 == rc) ? (int)len /*success*/: -1 /*failure*/;
}

static inline int32_t _odp_random_basic_data(uint8_t *buf, uint32_t len)
{
	return _odp_random_openssl_data(buf, len);
}

static inline int32_t _odp_random_crypto_data(uint8_t *buf, uint32_t len)
{
	return _odp_random_openssl_data(buf, len);
}

static inline int32_t _odp_random_true_data(uint8_t *buf ODP_UNUSED,
					    uint32_t len ODP_UNUSED)
{
	return -1;
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
#endif /* ODP_RANDOM_OPENSSL_INTERNAL_H_ */
