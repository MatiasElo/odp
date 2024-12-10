/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 ARM Limited
 * Copyright (c) 2017-2018 Linaro Limited
 */

#ifndef PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H
#define PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H

#if !defined(__aarch64__)
#error Use this file only when compiling for ARMv8 architecture
#endif

#include <odp_debug_internal.h>
#include <odp_types_internal.h>

union i128 {
	_odp_u128_t i128;
	int64_t  i64[2];
};

static inline _odp_u128_t lld(_odp_u128_t *var, int mm)
{
	union i128 old;

	_ODP_ASSERT(mm == __ATOMIC_ACQUIRE || mm == __ATOMIC_RELAXED);

	if (mm == __ATOMIC_ACQUIRE)
		__asm__ volatile("ldaxp %0, %1, [%2]"
				 : "=&r" (old.i64[0]), "=&r" (old.i64[1])
				 : "r" (var)
				 : "memory");
	else
		__asm__ volatile("ldxp %0, %1, [%2]"
				 : "=&r" (old.i64[0]), "=&r" (old.i64[1])
				 : "r" (var)
				 : );
	return old.i128;
}

/* Return 0 on success, 1 on failure */
static inline uint32_t scd(_odp_u128_t *var, _odp_u128_t neu, int mm)
{
	uint32_t ret;

	_ODP_ASSERT(mm == __ATOMIC_RELEASE || mm == __ATOMIC_RELAXED);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	if (mm == __ATOMIC_RELEASE)
		__asm__ volatile("stlxp %w0, %1, %2, [%3]"
				 : "=&r" (ret)
				 : "r" (((*(union i128 *)&neu)).i64[0]),
				   "r" (((*(union i128 *)&neu)).i64[1]),
				   "r" (var)
				 : "memory");
	else
		__asm__ volatile("stxp %w0, %1, %2, [%3]"
				 : "=&r" (ret)
				 : "r" (((*(union i128 *)&neu)).i64[0]),
				   "r" (((*(union i128 *)&neu)).i64[1]),
				   "r" (var)
				 : );
#pragma GCC diagnostic pop
	return ret;
}

#include "odp_atomic.h"

#ifdef __ARM_FEATURE_UNALIGNED
#define _ODP_UNALIGNED 1
#else
#define _ODP_UNALIGNED 0
#endif

#endif  /* PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H */
