/* Copyright (c) 2014-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	 BSD-3-Clause
 */

#include <malloc.h>
#include <odp_api.h>
#include <odp/helper/odph_api.h>
#include <CUnit/Basic.h>
#include <odp_cunit_common.h>
#include <unistd.h>

#define VERBOSE			0
#define MAX_ITERATIONS		1000

#define ADD_SUB_CNT		5

#define CNT			50000
#define U32_INIT_VAL		(1UL << 28)
#define U64_INIT_VAL		(1ULL << 33)
#define U32_MAGIC		0xa23f65b2
#define U64_MAGIC		0xf2e1c5430cb6a52e

#define GLOBAL_SHM_NAME		"GlobalLockTest"

#define UNUSED			__attribute__((__unused__))

typedef __volatile uint32_t volatile_u32_t;
typedef __volatile uint64_t volatile_u64_t;

typedef struct {
	odp_atomic_u128_t a128u;
	odp_atomic_u64_t a64u;
	odp_atomic_u64_t a64u_min;
	odp_atomic_u64_t a64u_max;
	odp_atomic_u64_t a64u_xchg;
	odp_atomic_u32_t a32u;
	odp_atomic_u32_t a32u_min;
	odp_atomic_u32_t a32u_max;
	odp_atomic_u32_t a32u_xchg;

	uint32_t g_num_threads;
	uint32_t g_iterations;
	uint32_t g_verbose;

	odp_barrier_t global_barrier;
} global_shared_mem_t;

/* Per-thread memory */
typedef struct {
	global_shared_mem_t *global_mem;

	int thread_id;
	int thread_core;

	volatile_u64_t delay_counter;
} per_thread_mem_t;

static odp_shm_t global_shm;
static global_shared_mem_t *global_mem;

/* Initialise per-thread memory */
static per_thread_mem_t *thread_init(void)
{
	global_shared_mem_t *global_mem;
	per_thread_mem_t *per_thread_mem;
	odp_shm_t global_shm;
	uint32_t per_thread_mem_len;

	per_thread_mem_len = sizeof(per_thread_mem_t);
	per_thread_mem = malloc(per_thread_mem_len);
	memset(per_thread_mem, 0, per_thread_mem_len);

	per_thread_mem->delay_counter = 1;

	per_thread_mem->thread_id = odp_thread_id();
	per_thread_mem->thread_core = odp_cpu_id();

	global_shm = odp_shm_lookup(GLOBAL_SHM_NAME);
	global_mem = odp_shm_addr(global_shm);
	CU_ASSERT_PTR_NOT_NULL(global_mem);

	per_thread_mem->global_mem = global_mem;

	return per_thread_mem;
}

static void thread_finalize(per_thread_mem_t *per_thread_mem)
{
	free(per_thread_mem);
}

static void test_atomic_inc_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_inc_u32(&global_mem->a32u);
}

static void test_atomic_inc_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_inc_u64(&global_mem->a64u);
}

static void test_atomic_dec_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_dec_u32(&global_mem->a32u);
}

static void test_atomic_dec_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_dec_u64(&global_mem->a64u);
}

static void test_atomic_fetch_inc_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_inc_u32(&global_mem->a32u);
}

static void test_atomic_fetch_inc_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_inc_u64(&global_mem->a64u);
}

static void test_atomic_fetch_dec_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_dec_u32(&global_mem->a32u);
}

static void test_atomic_fetch_dec_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_dec_u64(&global_mem->a64u);
}

static void test_atomic_add_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_add_u32(&global_mem->a32u, ADD_SUB_CNT);
}

static void test_atomic_add_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_add_u64(&global_mem->a64u, ADD_SUB_CNT);
}

static void test_atomic_sub_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_sub_u32(&global_mem->a32u, ADD_SUB_CNT);
}

static void test_atomic_sub_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_sub_u64(&global_mem->a64u, ADD_SUB_CNT);
}

static void test_atomic_fetch_add_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_add_u32(&global_mem->a32u, ADD_SUB_CNT);
}

static void test_atomic_fetch_add_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_add_u64(&global_mem->a64u, ADD_SUB_CNT);
}

static void test_atomic_fetch_sub_32(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_sub_u32(&global_mem->a32u, ADD_SUB_CNT);
}

static void test_atomic_fetch_sub_64(void)
{
	int i;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++)
		odp_atomic_fetch_sub_u64(&global_mem->a64u, ADD_SUB_CNT);
}

static void test_atomic_min_32(void)
{
	int i;
	uint32_t tmp;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_fetch_dec_u32(&global_mem->a32u);
		odp_atomic_min_u32(&global_mem->a32u_min, tmp);
	}
}

static void test_atomic_min_64(void)
{
	int i;
	uint64_t tmp;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_fetch_dec_u64(&global_mem->a64u);
		odp_atomic_min_u64(&global_mem->a64u_min, tmp);
	}
}

static void test_atomic_max_32(void)
{
	int i;
	uint32_t tmp;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_fetch_inc_u32(&global_mem->a32u);
		odp_atomic_max_u32(&global_mem->a32u_max, tmp);
	}
}

static void test_atomic_max_64(void)
{
	int i;
	uint64_t tmp;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_fetch_inc_u64(&global_mem->a64u);
		odp_atomic_max_u64(&global_mem->a64u_max, tmp);
	}
}

static void test_atomic_cas_inc_32(void)
{
	int i;
	uint32_t old;
	odp_atomic_u32_t *a32u = &global_mem->a32u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u32(a32u);

		while (odp_atomic_cas_u32(a32u, &old, old + 1) == 0)
			;
	}
}

static void test_atomic_cas_dec_32(void)
{
	int i;
	uint32_t old;
	odp_atomic_u32_t *a32u = &global_mem->a32u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u32(a32u);

		while (odp_atomic_cas_u32(a32u, &old, old - 1) == 0)
			;
	}
}

static void test_atomic_cas_inc_64(void)
{
	int i;
	uint64_t old;
	odp_atomic_u64_t *a64u = &global_mem->a64u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u64(a64u);

		while (odp_atomic_cas_u64(a64u, &old, old + 1) == 0)
			;
	}
}

static void test_atomic_cas_dec_64(void)
{
	int i;
	uint64_t old;
	odp_atomic_u64_t *a64u = &global_mem->a64u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u64(a64u);

		while (odp_atomic_cas_u64(a64u, &old, old - 1) == 0)
			;
	}
}

static void test_atomic_xchg_32(void)
{
	uint32_t old, new;
	int i;
	odp_atomic_u32_t *a32u = &global_mem->a32u;
	odp_atomic_u32_t *a32u_xchg = &global_mem->a32u_xchg;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		new = odp_atomic_fetch_inc_u32(a32u);
		old = odp_atomic_xchg_u32(a32u_xchg, new);

		if (old & 0x1)
			odp_atomic_xchg_u32(a32u_xchg, 0);
		else
			odp_atomic_xchg_u32(a32u_xchg, 1);
	}

	odp_atomic_sub_u32(a32u, CNT);
	odp_atomic_xchg_u32(a32u_xchg, U32_MAGIC);
}

static void test_atomic_xchg_64(void)
{
	uint64_t old, new;
	int i;
	odp_atomic_u64_t *a64u = &global_mem->a64u;
	odp_atomic_u64_t *a64u_xchg = &global_mem->a64u_xchg;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		new = odp_atomic_fetch_inc_u64(a64u);
		old = odp_atomic_xchg_u64(a64u_xchg, new);

		if (old & 0x1)
			odp_atomic_xchg_u64(a64u_xchg, 0);
		else
			odp_atomic_xchg_u64(a64u_xchg, 1);
	}

	odp_atomic_sub_u64(a64u, CNT);
	odp_atomic_xchg_u64(a64u_xchg, U64_MAGIC);
}

static void test_atomic_non_relaxed_32(void)
{
	int i;
	uint32_t tmp;
	odp_atomic_u32_t *a32u = &global_mem->a32u;
	odp_atomic_u32_t *a32u_min = &global_mem->a32u_min;
	odp_atomic_u32_t *a32u_max = &global_mem->a32u_max;
	odp_atomic_u32_t *a32u_xchg = &global_mem->a32u_xchg;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_load_acq_u32(a32u);
		odp_atomic_store_rel_u32(a32u, tmp);

		tmp = odp_atomic_load_acq_u32(a32u_max);
		odp_atomic_add_rel_u32(a32u_max, 1);

		tmp = odp_atomic_load_acq_u32(a32u_min);
		odp_atomic_sub_rel_u32(a32u_min, 1);

		tmp = odp_atomic_load_u32(a32u_xchg);
		while (odp_atomic_cas_acq_u32(a32u_xchg, &tmp, tmp + 1) == 0)
			;

		tmp = odp_atomic_load_u32(a32u_xchg);
		while (odp_atomic_cas_rel_u32(a32u_xchg, &tmp, tmp + 1) == 0)
			;

		tmp = odp_atomic_load_u32(a32u_xchg);
		/* finally set value for validation */
		while (odp_atomic_cas_acq_rel_u32(a32u_xchg, &tmp, U32_MAGIC)
		       == 0)
			;
	}
}

static void test_atomic_non_relaxed_64(void)
{
	int i;
	uint64_t tmp;
	odp_atomic_u64_t *a64u = &global_mem->a64u;
	odp_atomic_u64_t *a64u_min = &global_mem->a64u_min;
	odp_atomic_u64_t *a64u_max = &global_mem->a64u_max;
	odp_atomic_u64_t *a64u_xchg = &global_mem->a64u_xchg;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		tmp = odp_atomic_load_acq_u64(a64u);
		odp_atomic_store_rel_u64(a64u, tmp);

		tmp = odp_atomic_load_acq_u64(a64u_max);
		odp_atomic_add_rel_u64(a64u_max, 1);

		tmp = odp_atomic_load_acq_u64(a64u_min);
		odp_atomic_sub_rel_u64(a64u_min, 1);

		tmp = odp_atomic_load_u64(a64u_xchg);
		while (odp_atomic_cas_acq_u64(a64u_xchg, &tmp, tmp + 1) == 0)
			;

		tmp = odp_atomic_load_u64(a64u_xchg);
		while (odp_atomic_cas_rel_u64(a64u_xchg, &tmp, tmp + 1) == 0)
			;

		tmp = odp_atomic_load_u64(a64u_xchg);
		/* finally set value for validation */
		while (odp_atomic_cas_acq_rel_u64(a64u_xchg, &tmp, U64_MAGIC)
		       == 0)
			;
	}
}

static void test_atomic_relaxed_128(void)
{
	int i, ret;
	odp_u128_t old, new;
	odp_atomic_u128_t *a128u = &global_mem->a128u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u128(a128u);

		do {
			new.u64[0] = old.u64[0] + 2;
			new.u64[1] = old.u64[1] + 1;

			ret = odp_atomic_cas_u128(a128u, &old, new);

		} while (ret == 0);
	}
}

static void test_atomic_non_relaxed_128_acq(void)
{
	int i, ret;
	odp_u128_t old, new;
	odp_atomic_u128_t *a128u = &global_mem->a128u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u128(a128u);

		do {
			new.u64[0] = old.u64[0] + 2;
			new.u64[1] = old.u64[1] + 1;

			ret = odp_atomic_cas_acq_u128(a128u, &old, new);

		} while (ret == 0);
	}
}

static void test_atomic_non_relaxed_128_rel(void)
{
	int i, ret;
	odp_u128_t old, new;
	odp_atomic_u128_t *a128u = &global_mem->a128u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u128(a128u);

		do {
			new.u64[0] = old.u64[0] + 2;
			new.u64[1] = old.u64[1] + 1;

			ret = odp_atomic_cas_rel_u128(a128u, &old, new);

		} while (ret == 0);
	}
}

static void test_atomic_non_relaxed_128_acq_rel(void)
{
	int i, ret;
	odp_u128_t old, new;
	odp_atomic_u128_t *a128u = &global_mem->a128u;

	odp_barrier_wait(&global_mem->global_barrier);

	for (i = 0; i < CNT; i++) {
		old = odp_atomic_load_u128(a128u);

		do {
			new.u64[0] = old.u64[0] + 2;
			new.u64[1] = old.u64[1] + 1;

			ret = odp_atomic_cas_acq_rel_u128(a128u, &old, new);

		} while (ret == 0);
	}
}

static void test_atomic_inc_dec_32(void)
{
	test_atomic_inc_32();
	test_atomic_dec_32();
}

static void test_atomic_inc_dec_64(void)
{
	test_atomic_inc_64();
	test_atomic_dec_64();
}

static void test_atomic_fetch_inc_dec_32(void)
{
	test_atomic_fetch_inc_32();
	test_atomic_fetch_dec_32();
}

static void test_atomic_fetch_inc_dec_64(void)
{
	test_atomic_fetch_inc_64();
	test_atomic_fetch_dec_64();
}

static void test_atomic_add_sub_32(void)
{
	test_atomic_add_32();
	test_atomic_sub_32();
}

static void test_atomic_add_sub_64(void)
{
	test_atomic_add_64();
	test_atomic_sub_64();
}

static void test_atomic_fetch_add_sub_32(void)
{
	test_atomic_fetch_add_32();
	test_atomic_fetch_sub_32();
}

static void test_atomic_fetch_add_sub_64(void)
{
	test_atomic_fetch_add_64();
	test_atomic_fetch_sub_64();
}

static void test_atomic_max_min_32(void)
{
	test_atomic_max_32();
	test_atomic_min_32();
}

static void test_atomic_max_min_64(void)
{
	test_atomic_max_64();
	test_atomic_min_64();
}

static void test_atomic_cas_inc_dec_32(void)
{
	test_atomic_cas_inc_32();
	test_atomic_cas_dec_32();
}

static void test_atomic_cas_inc_dec_64(void)
{
	test_atomic_cas_inc_64();
	test_atomic_cas_dec_64();
}

static void test_atomic_cas_inc_128(void)
{
	test_atomic_relaxed_128();
	test_atomic_non_relaxed_128_acq();
	test_atomic_non_relaxed_128_rel();
	test_atomic_non_relaxed_128_acq_rel();
}

static void test_atomic_init(void)
{
	odp_atomic_init_u32(&global_mem->a32u, 0);
	odp_atomic_init_u64(&global_mem->a64u, 0);
	odp_atomic_init_u32(&global_mem->a32u_min, 0);
	odp_atomic_init_u32(&global_mem->a32u_max, 0);
	odp_atomic_init_u64(&global_mem->a64u_min, 0);
	odp_atomic_init_u64(&global_mem->a64u_max, 0);
	odp_atomic_init_u32(&global_mem->a32u_xchg, 0);
	odp_atomic_init_u64(&global_mem->a64u_xchg, 0);

	odp_u128_t a128u_tmp;

	a128u_tmp.u64[0] = 0;
	a128u_tmp.u64[1] = 0;
	odp_atomic_init_u128(&global_mem->a128u, a128u_tmp);
}

static void test_atomic_store(void)
{
	odp_atomic_store_u32(&global_mem->a32u, U32_INIT_VAL);
	odp_atomic_store_u64(&global_mem->a64u, U64_INIT_VAL);
	odp_atomic_store_u32(&global_mem->a32u_min, U32_INIT_VAL);
	odp_atomic_store_u32(&global_mem->a32u_max, U32_INIT_VAL);
	odp_atomic_store_u64(&global_mem->a64u_min, U64_INIT_VAL);
	odp_atomic_store_u64(&global_mem->a64u_max, U64_INIT_VAL);
	odp_atomic_store_u32(&global_mem->a32u_xchg, U32_INIT_VAL);
	odp_atomic_store_u64(&global_mem->a64u_xchg, U64_INIT_VAL);

	odp_u128_t a128u_tmp;

	a128u_tmp.u64[0] = U64_INIT_VAL;
	a128u_tmp.u64[1] = U64_INIT_VAL;
	odp_atomic_store_u128(&global_mem->a128u, a128u_tmp);
}

static void test_atomic_validate_init_val_32_64(void)
{
	CU_ASSERT(U32_INIT_VAL == odp_atomic_load_u32(&global_mem->a32u));
	CU_ASSERT(U64_INIT_VAL == odp_atomic_load_u64(&global_mem->a64u));
}

static void test_atomic_validate_init_val_128(void)
{
	odp_u128_t a128u = odp_atomic_load_u128(&global_mem->a128u);

	CU_ASSERT(U64_INIT_VAL == a128u.u64[0]);
	CU_ASSERT(U64_INIT_VAL == a128u.u64[1]);
}

static void test_atomic_validate_init_val(void)
{
	test_atomic_validate_init_val_32_64();
	test_atomic_validate_init_val_128();
}

static void test_atomic_validate_cas(void)
{
	test_atomic_validate_init_val_32_64();

	odp_u128_t a128u = odp_atomic_load_u128(&global_mem->a128u);
	const uint64_t iterations = a128u.u64[0] - a128u.u64[1];

	CU_ASSERT(iterations == 4 * CNT * global_mem->g_num_threads);
}

static void test_atomic_validate_max_min(void)
{
	test_atomic_validate_init_val();

	const uint64_t total_count = CNT * global_mem->g_num_threads;

	/* Max is the result of fetch_inc, so the final max value is total_count - 1. */
	CU_ASSERT(odp_atomic_load_u32(&global_mem->a32u_max) == U32_INIT_VAL + total_count - 1);
	CU_ASSERT(odp_atomic_load_u32(&global_mem->a32u_min) == U32_INIT_VAL);
	CU_ASSERT(odp_atomic_load_u64(&global_mem->a64u_max) == U64_INIT_VAL + total_count - 1);
	CU_ASSERT(odp_atomic_load_u64(&global_mem->a64u_min) == U64_INIT_VAL);
}

static void test_atomic_validate_xchg(void)
{
	test_atomic_validate_init_val();

	CU_ASSERT(odp_atomic_load_u32(&global_mem->a32u_xchg) ==
			U32_MAGIC);
	CU_ASSERT(odp_atomic_load_u64(&global_mem->a64u_xchg) ==
			U64_MAGIC);
}

static void test_atomic_validate_non_relaxed(void)
{
	test_atomic_validate_xchg();

	const uint64_t total_count = CNT * global_mem->g_num_threads;

	CU_ASSERT(odp_atomic_load_u32(&global_mem->a32u_max) == U32_INIT_VAL + total_count);
	CU_ASSERT(odp_atomic_load_u32(&global_mem->a32u_min) == U32_INIT_VAL - total_count);
	CU_ASSERT(odp_atomic_load_u64(&global_mem->a64u_max) == U64_INIT_VAL + total_count);
	CU_ASSERT(odp_atomic_load_u64(&global_mem->a64u_min) == U64_INIT_VAL - total_count);
}

static int atomic_init(odp_instance_t *inst)
{
	uint32_t workers_count, max_threads;
	int ret = 0;
	odp_cpumask_t mask;
	odp_init_t init_param;
	odph_helper_options_t helper_options;

	if (odph_options(&helper_options)) {
		fprintf(stderr, "error: odph_options() failed.\n");
		return -1;
	}

	odp_init_param_init(&init_param);
	init_param.mem_model = helper_options.mem_model;

	if (0 != odp_init_global(inst, &init_param, NULL)) {
		fprintf(stderr, "error: odp_init_global() failed.\n");
		return -1;
	}
	if (0 != odp_init_local(*inst, ODP_THREAD_CONTROL)) {
		fprintf(stderr, "error: odp_init_local() failed.\n");
		return -1;
	}

	global_shm = odp_shm_reserve(GLOBAL_SHM_NAME,
				     sizeof(global_shared_mem_t), 64, 0);
	if (ODP_SHM_INVALID == global_shm) {
		fprintf(stderr, "Unable reserve memory for global_shm\n");
		return -1;
	}

	global_mem = odp_shm_addr(global_shm);
	memset(global_mem, 0, sizeof(global_shared_mem_t));

	global_mem->g_num_threads = MAX_WORKERS;
	global_mem->g_iterations = MAX_ITERATIONS;
	global_mem->g_verbose = VERBOSE;

	workers_count = odp_cpumask_default_worker(&mask, 0);

	max_threads = (workers_count >= MAX_WORKERS) ?
			MAX_WORKERS : workers_count;

	if (max_threads < global_mem->g_num_threads) {
		printf("Requested num of threads is too large\n");
		printf("reducing from %" PRIu32 " to %" PRIu32 "\n",
		       global_mem->g_num_threads,
		       max_threads);
		global_mem->g_num_threads = max_threads;
	}

	printf("Num of threads used = %" PRIu32 "\n",
	       global_mem->g_num_threads);

	odp_barrier_init(&global_mem->global_barrier, global_mem->g_num_threads);

	return ret;
}

static int atomic_term(odp_instance_t inst)
{
	odp_shm_t shm;

	shm = odp_shm_lookup(GLOBAL_SHM_NAME);
	if (0 != odp_shm_free(shm)) {
		fprintf(stderr, "error: odp_shm_free() failed.\n");
		return -1;
	}

	if (0 != odp_term_local()) {
		fprintf(stderr, "error: odp_term_local() failed.\n");
		return -1;
	}

	if (0 != odp_term_global(inst)) {
		fprintf(stderr, "error: odp_term_global() failed.\n");
		return -1;
	}

	return 0;
}

/* Atomic tests */
static int test_atomic_inc_dec_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_inc_dec_32();
	test_atomic_inc_dec_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_add_sub_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_add_sub_32();
	test_atomic_add_sub_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_fetch_inc_dec_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_fetch_inc_dec_32();
	test_atomic_fetch_inc_dec_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_fetch_add_sub_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_fetch_add_sub_32();
	test_atomic_fetch_add_sub_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_max_min_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_max_min_32();
	test_atomic_max_min_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_cas_inc_dec_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_cas_inc_dec_32();
	test_atomic_cas_inc_dec_64();
	test_atomic_cas_inc_128();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_xchg_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_xchg_32();
	test_atomic_xchg_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static int test_atomic_non_relaxed_thread(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;

	per_thread_mem = thread_init();
	test_atomic_non_relaxed_32();
	test_atomic_non_relaxed_64();

	thread_finalize(per_thread_mem);

	return CU_get_number_of_failures();
}

static void test_atomic_functional(int test_fn(void *), void validate_fn(void))
{
	pthrd_arg arg;

	arg.numthrds = global_mem->g_num_threads;
	test_atomic_init();
	test_atomic_store();
	odp_cunit_thread_create(test_fn, &arg);
	odp_cunit_thread_exit(&arg);
	validate_fn();
}

static void test_atomic_op_lock_free_set(void)
{
	odp_atomic_op_t atomic_op;

	memset(&atomic_op, 0xff, sizeof(odp_atomic_op_t));
	atomic_op.all_bits = 0;

	CU_ASSERT(atomic_op.all_bits     == 0);
	CU_ASSERT(atomic_op.op.init      == 0);
	CU_ASSERT(atomic_op.op.load      == 0);
	CU_ASSERT(atomic_op.op.store     == 0);
	CU_ASSERT(atomic_op.op.fetch_add == 0);
	CU_ASSERT(atomic_op.op.add       == 0);
	CU_ASSERT(atomic_op.op.fetch_sub == 0);
	CU_ASSERT(atomic_op.op.sub       == 0);
	CU_ASSERT(atomic_op.op.fetch_inc == 0);
	CU_ASSERT(atomic_op.op.inc       == 0);
	CU_ASSERT(atomic_op.op.fetch_dec == 0);
	CU_ASSERT(atomic_op.op.dec       == 0);
	CU_ASSERT(atomic_op.op.min       == 0);
	CU_ASSERT(atomic_op.op.max       == 0);
	CU_ASSERT(atomic_op.op.cas       == 0);
	CU_ASSERT(atomic_op.op.xchg      == 0);

	/* Test setting first, last and couple of other bits */
	atomic_op.op.init = 1;
	CU_ASSERT(atomic_op.op.init      == 1);
	CU_ASSERT(atomic_op.all_bits     != 0);
	atomic_op.op.init = 0;
	CU_ASSERT(atomic_op.all_bits     == 0);

	atomic_op.op.xchg = 1;
	CU_ASSERT(atomic_op.op.xchg      == 1);
	CU_ASSERT(atomic_op.all_bits     != 0);
	atomic_op.op.xchg = 0;
	CU_ASSERT(atomic_op.all_bits     == 0);

	atomic_op.op.add = 1;
	CU_ASSERT(atomic_op.op.add       == 1);
	CU_ASSERT(atomic_op.all_bits     != 0);
	atomic_op.op.add = 0;
	CU_ASSERT(atomic_op.all_bits     == 0);

	atomic_op.op.dec = 1;
	CU_ASSERT(atomic_op.op.dec       == 1);
	CU_ASSERT(atomic_op.all_bits     != 0);
	atomic_op.op.dec = 0;
	CU_ASSERT(atomic_op.all_bits     == 0);
}

static void test_atomic_op_lock_free_64(void)
{
	odp_atomic_op_t atomic_op;
	int ret_null, ret;

	memset(&atomic_op, 0xff, sizeof(odp_atomic_op_t));
	ret      = odp_atomic_lock_free_u64(&atomic_op);
	ret_null = odp_atomic_lock_free_u64(NULL);

	CU_ASSERT(ret == ret_null);

	/* Init operation is not atomic by the spec. Call to
	 * odp_atomic_lock_free_u64() zeros it but never sets it. */

	if (ret == 0) {
		/* none are lock free */
		CU_ASSERT(atomic_op.all_bits     == 0);
		CU_ASSERT(atomic_op.op.init      == 0);
		CU_ASSERT(atomic_op.op.load      == 0);
		CU_ASSERT(atomic_op.op.store     == 0);
		CU_ASSERT(atomic_op.op.fetch_add == 0);
		CU_ASSERT(atomic_op.op.add       == 0);
		CU_ASSERT(atomic_op.op.fetch_sub == 0);
		CU_ASSERT(atomic_op.op.sub       == 0);
		CU_ASSERT(atomic_op.op.fetch_inc == 0);
		CU_ASSERT(atomic_op.op.inc       == 0);
		CU_ASSERT(atomic_op.op.fetch_dec == 0);
		CU_ASSERT(atomic_op.op.dec       == 0);
		CU_ASSERT(atomic_op.op.min       == 0);
		CU_ASSERT(atomic_op.op.max       == 0);
		CU_ASSERT(atomic_op.op.cas       == 0);
		CU_ASSERT(atomic_op.op.xchg      == 0);
	}

	if (ret == 1) {
		/* some are lock free */
		CU_ASSERT(atomic_op.all_bits     != 0);
		CU_ASSERT(atomic_op.op.init      == 0);
	}

	if (ret == 2) {
		/* all are lock free */
		CU_ASSERT(atomic_op.all_bits     != 0);
		CU_ASSERT(atomic_op.op.init      == 0);
		CU_ASSERT(atomic_op.op.load      == 1);
		CU_ASSERT(atomic_op.op.store     == 1);
		CU_ASSERT(atomic_op.op.fetch_add == 1);
		CU_ASSERT(atomic_op.op.add       == 1);
		CU_ASSERT(atomic_op.op.fetch_sub == 1);
		CU_ASSERT(atomic_op.op.sub       == 1);
		CU_ASSERT(atomic_op.op.fetch_inc == 1);
		CU_ASSERT(atomic_op.op.inc       == 1);
		CU_ASSERT(atomic_op.op.fetch_dec == 1);
		CU_ASSERT(atomic_op.op.dec       == 1);
		CU_ASSERT(atomic_op.op.min       == 1);
		CU_ASSERT(atomic_op.op.max       == 1);
		CU_ASSERT(atomic_op.op.cas       == 1);
		CU_ASSERT(atomic_op.op.xchg      == 1);
	}
}

static void test_atomic_op_lock_free_128(void)
{
	odp_atomic_op_t atomic_op;
	int ret_null, ret;

	memset(&atomic_op, 0xff, sizeof(odp_atomic_op_t));
	ret      = odp_atomic_lock_free_u128(&atomic_op);
	ret_null = odp_atomic_lock_free_u128(NULL);

	CU_ASSERT(ret == ret_null);

	/* Init operation is not atomic by the spec. Call to
	 * odp_atomic_lock_free_u128() zeros it but never sets it. */

	if (ret == 0) {
		/* none are lock free */
		CU_ASSERT(atomic_op.all_bits     == 0);
		CU_ASSERT(atomic_op.op.init      == 0);
		CU_ASSERT(atomic_op.op.load      == 0);
		CU_ASSERT(atomic_op.op.store     == 0);
		CU_ASSERT(atomic_op.op.cas       == 0);
	}

	if (ret == 1) {
		/* some are lock free */
		CU_ASSERT(atomic_op.all_bits     != 0);
		CU_ASSERT(atomic_op.op.init      == 0);
	}

	if (ret == 2) {
		/* all are lock free */
		CU_ASSERT(atomic_op.all_bits     != 0);
		CU_ASSERT(atomic_op.op.init      == 0);
		CU_ASSERT(atomic_op.op.load      == 1);
		CU_ASSERT(atomic_op.op.store     == 1);
		CU_ASSERT(atomic_op.op.cas       == 1);
	}
}

static void atomic_test_atomic_inc_dec(void)
{
	test_atomic_functional(test_atomic_inc_dec_thread, test_atomic_validate_init_val);
}

static void atomic_test_atomic_add_sub(void)
{
	test_atomic_functional(test_atomic_add_sub_thread, test_atomic_validate_init_val);
}

static void atomic_test_atomic_fetch_inc_dec(void)
{
	test_atomic_functional(test_atomic_fetch_inc_dec_thread, test_atomic_validate_init_val);
}

static void atomic_test_atomic_fetch_add_sub(void)
{
	test_atomic_functional(test_atomic_fetch_add_sub_thread, test_atomic_validate_init_val);
}

static void atomic_test_atomic_max_min(void)
{
	test_atomic_functional(test_atomic_max_min_thread, test_atomic_validate_max_min);
}

static void atomic_test_atomic_cas_inc_dec(void)
{
	test_atomic_functional(test_atomic_cas_inc_dec_thread, test_atomic_validate_cas);
}

static void atomic_test_atomic_xchg(void)
{
	test_atomic_functional(test_atomic_xchg_thread, test_atomic_validate_xchg);
}

static void atomic_test_atomic_non_relaxed(void)
{
	test_atomic_functional(test_atomic_non_relaxed_thread,
			       test_atomic_validate_non_relaxed);
}

static void atomic_test_atomic_op_lock_free(void)
{
	test_atomic_op_lock_free_set();
	test_atomic_op_lock_free_64();
	test_atomic_op_lock_free_128();
}

odp_testinfo_t atomic_suite_atomic[] = {
	ODP_TEST_INFO(atomic_test_atomic_inc_dec),
	ODP_TEST_INFO(atomic_test_atomic_add_sub),
	ODP_TEST_INFO(atomic_test_atomic_fetch_inc_dec),
	ODP_TEST_INFO(atomic_test_atomic_fetch_add_sub),
	ODP_TEST_INFO(atomic_test_atomic_max_min),
	ODP_TEST_INFO(atomic_test_atomic_cas_inc_dec),
	ODP_TEST_INFO(atomic_test_atomic_xchg),
	ODP_TEST_INFO(atomic_test_atomic_non_relaxed),
	ODP_TEST_INFO(atomic_test_atomic_op_lock_free),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t atomic_suites[] = {
	{"atomic", NULL, NULL,
		atomic_suite_atomic},
	ODP_SUITE_INFO_NULL
};

int main(int argc, char *argv[])
{
	int ret;

	/* parse common options: */
	if (odp_cunit_parse_options(argc, argv))
		return -1;

	odp_cunit_register_global_init(atomic_init);
	odp_cunit_register_global_term(atomic_term);

	ret = odp_cunit_register(atomic_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
