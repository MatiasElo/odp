/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017-2021 ARM Limited
 * Copyright (c) 2017-2018 Linaro Limited
 */

#ifndef PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_ATOMIC_H
#define PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_ATOMIC_H

#ifndef PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H
#error This file should not be included directly, please include odp_cpu.h
#endif

#include <odp_types_internal.h>
#include <limits.h>

#define HAS_ACQ(mo) ((mo) != __ATOMIC_RELAXED && (mo) != __ATOMIC_RELEASE)
#define HAS_RLS(mo) ((mo) == __ATOMIC_RELEASE || (mo) == __ATOMIC_ACQ_REL || \
		     (mo) == __ATOMIC_SEQ_CST)

#define LL_MO(mo) (HAS_ACQ((mo)) ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED)
#define SC_MO(mo) (HAS_RLS((mo)) ? __ATOMIC_RELEASE : __ATOMIC_RELAXED)

#ifndef __ARM_FEATURE_QRDMX /* Feature only available in v8.1a and beyond */
static inline bool
__lockfree_compare_exchange_16(register _odp_u128_t *var, _odp_u128_t *exp,
			       register _odp_u128_t neu, bool weak, int mo_success,
			       int mo_failure)
{
	(void)weak; /* Always do strong CAS or we can't perform atomic read */
	/* Ignore memory ordering for failure, memory order for
	 * success must be stronger or equal. */
	(void)mo_failure;
	register _odp_u128_t old;
	register _odp_u128_t expected;
	int ll_mo = LL_MO(mo_success);
	int sc_mo = SC_MO(mo_success);

	expected = *exp;
	__asm__ volatile("" ::: "memory");
	do {
		/* Atomicity of LLD is not guaranteed */
		old = lld(var, ll_mo);
		/* Must write back neu or old to verify atomicity of LLD */
	} while (odp_unlikely(scd(var, old == expected ? neu : old, sc_mo)));
	*exp = old; /* Always update, atomically read value */
	return old == expected;
}

#else

static inline _odp_u128_t cas_u128(_odp_u128_t *ptr, _odp_u128_t old_val,
				   _odp_u128_t new_val, int mo)
{
	/* CASP instructions require that the first register number is paired */
	register uint64_t old0 __asm__ ("x0");
	register uint64_t old1 __asm__ ("x1");
	register uint64_t new0 __asm__ ("x2");
	register uint64_t new1 __asm__ ("x3");

	old0 = (uint64_t)old_val;
	old1 = (uint64_t)(old_val >> 64);
	new0 = (uint64_t)new_val;
	new1 = (uint64_t)(new_val >> 64);

	if (mo == __ATOMIC_RELAXED) {
		__asm__ volatile("casp %[old0], %[old1], %[new0], %[new1], [%[ptr]]"
				 : [old0] "+r" (old0), [old1] "+r" (old1)
				 : [new0] "r"  (new0), [new1] "r"  (new1), [ptr] "r" (ptr)
				 : "memory");
	} else if (mo == __ATOMIC_ACQUIRE) {
		__asm__ volatile("caspa %[old0], %[old1], %[new0], %[new1], [%[ptr]]"
				 : [old0] "+r" (old0), [old1] "+r" (old1)
				 : [new0] "r"  (new0), [new1] "r"  (new1), [ptr] "r" (ptr)
				 : "memory");
	} else if (mo == __ATOMIC_ACQ_REL) {
		__asm__ volatile("caspal %[old0], %[old1], %[new0], %[new1], [%[ptr]]"
				 : [old0] "+r" (old0), [old1] "+r" (old1)
				 : [new0] "r"  (new0), [new1] "r"  (new1), [ptr] "r" (ptr)
				 : "memory");
	} else if (mo == __ATOMIC_RELEASE) {
		__asm__ volatile("caspl %[old0], %[old1], %[new0], %[new1], [%[ptr]]"
				 : [old0] "+r" (old0), [old1] "+r" (old1)
				 : [new0] "r"  (new0), [new1] "r"  (new1), [ptr] "r" (ptr)
				 : "memory");
	} else {
		abort();
	}

	return ((_odp_u128_t)old0) | (((_odp_u128_t)old1) << 64);
}

static inline bool
__lockfree_compare_exchange_16(register _odp_u128_t *var, _odp_u128_t *exp,
			       register _odp_u128_t neu, bool weak, int mo_success,
			       int mo_failure)
{
	(void)weak;
	(void)mo_failure;
	_odp_u128_t old;
	_odp_u128_t expected;

	expected = *exp;
	old = cas_u128(var, expected, neu, mo_success);
	*exp = old; /* Always update, atomically read value */
	return old == expected;
}

#endif  /* __ARM_FEATURE_QRDMX */

static inline _odp_u128_t __lockfree_load_16(_odp_u128_t *var, int mo)
{
	_odp_u128_t old = *var; /* Possibly torn read */

	/* Do CAS to ensure atomicity
	 * Either CAS succeeds (writing back the same value)
	 * Or CAS fails and returns the old value (atomic read)
	 */
	(void)__lockfree_compare_exchange_16(var, &old, old, false, mo, mo);
	return old;
}

static inline _odp_u128_t lockfree_load_u128(_odp_u128_t *atomic)
{
	return __lockfree_load_16((_odp_u128_t *)atomic, __ATOMIC_RELAXED);
}

static inline int lockfree_cas_acq_rel_u128(_odp_u128_t *atomic,
					    _odp_u128_t old_val,
					    _odp_u128_t new_val)
{
	return __lockfree_compare_exchange_16((_odp_u128_t *)atomic,
					      (_odp_u128_t *)&old_val,
					      new_val,
					      0,
					      __ATOMIC_ACQ_REL,
					      __ATOMIC_RELAXED);
}

static inline int lockfree_check_u128(void)
{
	return 1;
}

#endif  /* PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_ATOMIC_H */
