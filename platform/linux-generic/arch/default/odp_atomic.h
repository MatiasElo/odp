/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 ARM Limited
 */

#ifndef ODP_DEFAULT_ATOMIC_H_
#define ODP_DEFAULT_ATOMIC_H_

#include <odp_types_internal.h>

#ifdef __SIZEOF_INT128__

static inline _odp_u128_t lockfree_load_u128(_odp_u128_t *atomic)
{
	return __atomic_load_n(atomic, __ATOMIC_RELAXED);
}

static inline int lockfree_cas_acq_rel_u128(_odp_u128_t *atomic,
					    _odp_u128_t old_val,
					    _odp_u128_t new_val)
{
	return __atomic_compare_exchange_n(atomic, &old_val, new_val,
					   0 /* strong */,
					   __ATOMIC_ACQ_REL,
					   __ATOMIC_RELAXED);
}

static inline int lockfree_check_u128(void)
{
	return __atomic_is_lock_free(16, NULL);
}

#endif

#endif
