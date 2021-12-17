/* Copyright (c) 2020-2022, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_RANDOM_STD_INTERNAL_H_
#define ODP_RANDOM_STD_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/random.h>

#include <stdint.h>

int32_t _odp_random_std_test_data(uint8_t *buf, uint32_t len, uint64_t *seed);
int _odp_random_std_init_local(void);
int _odp_random_std_term_local(void);
odp_random_kind_t _odp_random_std_max_kind(void);
int32_t _odp_random_std_true_data(uint8_t *buf, uint32_t len);
int32_t _odp_random_std_crypto_data(uint8_t *buf, uint32_t len);
int32_t _odp_random_std_basic_data(uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif
#endif /* ODP_RANDOM_STD_INTERNAL_H_ */
