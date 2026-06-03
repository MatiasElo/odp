/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Nokia
 */

/**
 * @example odp_bench_stash.c
 *
 * Microbenchmark application for stash API functions
 *
 * @cond _ODP_HIDE_FROM_DOXYGEN_
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include <bench_common.h>
#include <export_results.h>

#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/** Number of object handles processed per test case
 *
 *  Each test round moves this many object handles in total. Non-batch test
 *  cases perform this many single object calls, batch test cases perform
 *  'TEST_REPEAT_COUNT / burst_size' calls of 'burst_size' objects each. This
 *  keeps the maximum number of object handles stored in a stash bounded so
 *  that put-only test cases do not overflow the stash.
 */
#define TEST_REPEAT_COUNT 1000

/** Default number of rounds per test case */
#define TEST_ROUNDS 100u

/** Maximum burst size for *_batch operations */
#define TEST_MAX_BURST 64

/** Default burst size for *_batch operations */
#define TEST_DEF_BURST 8

/** Maximum number of results to be held */
#define TEST_MAX_BENCH 40

/** Stash size (number of object handles)
 *
 *  Large enough to hold all object handles stored during a single test round.
 */
#define TEST_STASH_SIZE TEST_REPEAT_COUNT

/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ? \
			    strrchr((file_name), '/') + 1 : (file_name))

#define BENCH_INFO(run_fn, init_fn, term_fn, alt_name) \
	{.name = #run_fn, .run = run_fn, .init = init_fn, .term = term_fn, .desc = alt_name}

#define BENCH_INFO_COND(run_fn, init_fn, term_fn, alt_name, cond_fn) \
	{.name = #run_fn, .run = run_fn, .init = init_fn, .term = term_fn, .desc = alt_name, \
	 .cond = cond_fn}

#define BENCH_INFO_TM(run_fn, init_fn, term_fn, cond_fn) \
	{.name = #run_fn, .run = run_fn, .init = init_fn, .term = term_fn, .cond = cond_fn,\
	 .max_rounds = 0}

typedef enum {
	M_TPUT,
	M_LATENCY
} meas_mode_t;

/**
 * Parsed command line arguments
 */
typedef struct {
	int bench_idx;    /** Benchmark index to run indefinitely */
	int burst_size;   /** Burst size for *_batch operations */
	int cache_size;   /** Stash cache size */
	int time;         /** Measure time vs. CPU cycles */
	meas_mode_t mode; /** Measurement mode */
	uint32_t rounds;  /** Rounds per test case */
	odp_stash_op_mode_t put_mode; /** Stash put operation mode */
	odp_stash_op_mode_t get_mode; /** Stash get operation mode */
	odp_bool_t strict_size;       /** Strict size stash */
} appl_args_t;

/**
 * Grouping of all global data
 */
typedef struct {
	/** Application (parsed) arguments */
	appl_args_t appl;

	/** Common benchmark suite data */
	struct {
		union {
			/** Basic suite for avg */
			bench_suite_t b;
			/** TM suite for min/max/avg */
			bench_tm_suite_t t;
		};

		/** Loop breaker */
		odp_atomic_u32_t *exit;
		/** Pointer to suite return value */
		int *retval;
		/** Suite args */
		void *args;
		/** Suite runner */
		int (*suite_fn)(void *args);
		/** Data exporter */
		int (*export_fn)(void *data);
	} suite;

	/** Stash for 32-bit object handles (also used for generic put/get) */
	odp_stash_t stash_u32;
	/** Stash for 64-bit object handles */
	odp_stash_t stash_u64;
	/** Stash for pointer object handles */
	odp_stash_t stash_ptr;

	/** 32-bit object handles are supported */
	int have_u32;
	/** 64-bit object handles are supported */
	int have_u64;
	/** Pointer object handles are supported */
	int have_ptr;
	/** Stash cache is enabled */
	int have_cache;
	/** Stash count statistics counter is enabled */
	int have_stats;

	/** Array for storing test 32-bit object handles */
	uint32_t u32_tbl[TEST_STASH_SIZE];
	/** Array for storing test 64-bit object handles */
	uint64_t u64_tbl[TEST_STASH_SIZE];
	/** Array for storing test pointer object handles */
	uintptr_t ptr_tbl[TEST_STASH_SIZE];

	/** CPU mask as string */
	char cpumask_str[ODP_CPUMASK_STR_SIZE];
	/** Array for storing results */
	union {
		/** Basic suite results */
		double b[TEST_MAX_BENCH];
		/** TM suite results */
		bench_tm_result_t t[TEST_MAX_BENCH];
	} result;
} args_t;

/** Global pointer to args */
static args_t *gbl_args;

static void sig_handler(int signo ODP_UNUSED)
{
	if (gbl_args == NULL)
		return;
	odp_atomic_store_u32(gbl_args->suite.exit, 1);
}

/*
 * State helpers
 *
 * The benchmark framework calls a test case 'init' function before and a 'term'
 * function after every test round. Put benchmarks require an empty stash before
 * the round, get benchmarks a filled one. All test rounds end with an empty
 * stash, which is also a precondition for destroying the stash.
 */

static void drain_u32(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t val[TEST_MAX_BURST];
	int32_t num;

	do {
		num = odp_stash_get_u32(stash, val, TEST_MAX_BURST);
		if (num < 0)
			ODPH_ABORT("Draining stash failed\n");
	} while (num);
}

static void fill_u32(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int32_t n = 0;

	while (n < TEST_REPEAT_COUNT) {
		int32_t ret = odp_stash_put_u32(stash, &tbl[n], TEST_REPEAT_COUNT - n);

		if (ret < 0)
			ODPH_ABORT("Filling stash failed\n");

		n += ret;
	}
}

static void drain_u64(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t val[TEST_MAX_BURST];
	int32_t num;

	do {
		num = odp_stash_get_u64(stash, val, TEST_MAX_BURST);
		if (num < 0)
			ODPH_ABORT("Draining stash failed\n");
	} while (num);
}

static void fill_u64(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int32_t n = 0;

	while (n < TEST_REPEAT_COUNT) {
		int32_t ret = odp_stash_put_u64(stash, &tbl[n], TEST_REPEAT_COUNT - n);

		if (ret < 0)
			ODPH_ABORT("Filling stash failed\n");

		n += ret;
	}
}

static void drain_ptr(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t val[TEST_MAX_BURST];
	int32_t num;

	do {
		num = odp_stash_get_ptr(stash, val, TEST_MAX_BURST);
		if (num < 0)
			ODPH_ABORT("Draining stash failed\n");
	} while (num);
}

static void fill_ptr(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int32_t n = 0;

	while (n < TEST_REPEAT_COUNT) {
		int32_t ret = odp_stash_put_ptr(stash, &tbl[n], TEST_REPEAT_COUNT - n);

		if (ret < 0)
			ODPH_ABORT("Filling stash failed\n");

		n += ret;
	}
}

/*
 * Preconditions
 */

static int check_u32(void)
{
	return gbl_args->have_u32;
}

static int check_u64(void)
{
	return gbl_args->have_u64;
}

static int check_ptr(void)
{
	return gbl_args->have_ptr;
}

static int check_cache(void)
{
	return gbl_args->have_cache;
}

static int check_stats(void)
{
	return gbl_args->have_stats;
}

/*
 * Generic object handle put/get (operating on the 32-bit stash)
 */

static int stash_put(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_put(stash, &tbl[i], 1);

	return ret;
}

static int stash_put_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_put_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_put_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_put_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_get(stash, &tbl[i], 1);

	return ret;
}

static int stash_get_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_get_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_get_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

/*
 * 32-bit object handle put/get
 */

static int stash_put_u32(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_put_u32(stash, &tbl[i], 1);

	return ret;
}

static int stash_put_u32_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_u32()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_u32(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_put_u32_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_put_u32_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_put_u32_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_u32_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_u32_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_u32(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_get_u32(stash, &tbl[i], 1);

	return ret;
}

static int stash_get_u32_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_u32()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_u32(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_u32_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_get_u32_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_get_u32_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	uint32_t *tbl = gbl_args->u32_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_u32_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_u32_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

/*
 * 64-bit object handle put/get
 */

static int stash_put_u64(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_put_u64(stash, &tbl[i], 1);

	return ret;
}

static int stash_put_u64_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_u64()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_u64(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_put_u64_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_put_u64_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_put_u64_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_u64_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_u64_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_u64(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_get_u64(stash, &tbl[i], 1);

	return ret;
}

static int stash_get_u64_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_u64()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_u64(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_u64_batch(void)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_get_u64_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_get_u64_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u64;
	uint64_t *tbl = gbl_args->u64_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_u64_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_u64_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

/*
 * Pointer object handle put/get
 */

static int stash_put_ptr(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_put_ptr(stash, &tbl[i], 1);

	return ret;
}

static int stash_put_ptr_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_ptr()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_ptr(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_put_ptr_batch(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_put_ptr_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_put_ptr_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_put_ptr_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_put_ptr_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_ptr(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_get_ptr(stash, &tbl[i], 1);

	return ret;
}

static int stash_get_ptr_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_ptr()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_ptr(stash, &tbl[i], 1);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

static int stash_get_ptr_batch(void)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = TEST_REPEAT_COUNT / burst_size;
	int ret = 0;

	for (int i = 0; i < iters; i++)
		ret += odp_stash_get_ptr_batch(stash, &tbl[i * burst_size], burst_size);

	return ret;
}

static int stash_get_ptr_batch_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_ptr;
	uintptr_t *tbl = gbl_args->ptr_tbl;
	int burst_size = gbl_args->appl.burst_size;
	int iters = repeat_count / burst_size;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_get_ptr_batch()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < iters; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_get_ptr_batch(stash, &tbl[i * burst_size], burst_size);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return ret;
}

/*
 * Other stash functions
 */

static int stash_flush_cache(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_flush_cache(stash);

	return !ret;
}

static int stash_flush_cache_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_flush_cache()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_flush_cache(stash);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return !ret;
}

static int stash_stats(void)
{
	odp_stash_t stash = gbl_args->stash_u32;
	odp_stash_stats_t stats;
	int ret = 0;

	for (int i = 0; i < TEST_REPEAT_COUNT; i++)
		ret += odp_stash_stats(stash, &stats);

	return !ret;
}

static int stash_stats_tm(bench_tm_result_t *res, int repeat_count)
{
	odp_stash_t stash = gbl_args->stash_u32;
	odp_stash_stats_t stats;
	int ret = 0;
	const uint8_t id1 = bench_tm_func_register(res, "odp_stash_stats()");
	bench_tm_stamp_t s1, s2;

	for (int i = 0; i < repeat_count; i++) {
		bench_tm_now(res, &s1);
		ret += odp_stash_stats(stash, &stats);
		bench_tm_now(res, &s2);
		bench_tm_func_record(&s2, &s1, res, id1);
	}

	return !ret;
}

/**
 * Print usage information
 */
static void usage(char *progname)
{
	printf("\n"
	       "OpenDataPlane Stash API microbenchmarks.\n"
	       "\n"
	       "Usage: %s OPTIONS\n"
	       "  E.g. %s\n"
	       "\n"
	       "Optional OPTIONS:\n"
	       "  -b, --burst <num>       Test burst size.\n"
	       "  -c, --cache_size <num>  Stash cache size.\n"
	       "  -p, --put_mode <mode>   Stash put operation mode. 0: MT safe (default), 1: single thread\n"
	       "                          safe, 2: thread local.\n"
	       "  -g, --get_mode <mode>   Stash get operation mode. 0: MT safe (default), 1: single thread\n"
	       "                          safe, 2: thread local.\n"
	       "  -s, --strict_size       Create strict size stashes.\n"
	       "  -i, --index <idx>       Benchmark index to run indefinitely.\n"
	       "  -r, --rounds <num>      Run each test case 'num' times (default %u).\n"
	       "  -t, --time <opt>        Time measurement. 0: measure CPU cycles (default), 1: measure time\n"
	       "  -m, --mode <mode>       Measurement mode. 0: measure throughput, track average execution\n"
	       "                          time (default), 1: measure latency, track function minimum and\n"
	       "                          maximum in addition to average execution time.\n"
	       "  -h, --help              Display help and exit.\n\n"
	       "\n", NO_PATH(progname), NO_PATH(progname), TEST_ROUNDS);
}

/**
 * Parse and store the command line arguments
 *
 * @param argc       argument count
 * @param argv[]     argument vector
 * @param appl_args  Store application arguments here
 */
static void parse_args(int argc, char *argv[], appl_args_t *appl_args)
{
	int opt;
	static const struct option longopts[] = {
		{"burst", required_argument, NULL, 'b'},
		{"cache_size", required_argument, NULL, 'c'},
		{"put_mode", required_argument, NULL, 'p'},
		{"get_mode", required_argument, NULL, 'g'},
		{"strict_size", no_argument, NULL, 's'},
		{"index", required_argument, NULL, 'i'},
		{"rounds", required_argument, NULL, 'r'},
		{"time", required_argument, NULL, 't'},
		{"mode", required_argument, NULL, 'm'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	static const char *shortopts =  "c:b:p:g:si:r:t:m:h";

	appl_args->bench_idx = 0; /* Run all benchmarks */
	appl_args->burst_size = TEST_DEF_BURST;
	appl_args->cache_size = -1;
	appl_args->rounds = TEST_ROUNDS;
	appl_args->time = 0;
	appl_args->mode = M_TPUT;
	appl_args->put_mode = ODP_STASH_OP_MT;
	appl_args->get_mode = ODP_STASH_OP_MT;
	appl_args->strict_size = false;

	while (1) {
		opt = getopt_long(argc, argv, shortopts, longopts, NULL);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 'c':
			appl_args->cache_size = atoi(optarg);
			break;
		case 'b':
			appl_args->burst_size = atoi(optarg);
			break;
		case 'p':
			appl_args->put_mode = atoi(optarg);
			break;
		case 'g':
			appl_args->get_mode = atoi(optarg);
			break;
		case 's':
			appl_args->strict_size = true;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'i':
			appl_args->bench_idx = atoi(optarg);
			break;
		case 'r':
			appl_args->rounds = atoi(optarg);
			break;
		case 't':
			appl_args->time = atoi(optarg);
			break;
		case 'm':
			appl_args->mode = atoi(optarg);
			break;
		default:
			break;
		}
	}

	if (appl_args->burst_size < 1 ||
	    appl_args->burst_size > TEST_MAX_BURST) {
		printf("Invalid burst size (max %d)\n", TEST_MAX_BURST);
		exit(EXIT_FAILURE);
	}

	if (appl_args->rounds < 1) {
		printf("Invalid number test rounds: %d\n", appl_args->rounds);
		exit(EXIT_FAILURE);
	}

	if (appl_args->mode != M_TPUT && appl_args->mode != M_LATENCY) {
		printf("Invalid measurement mode: %d\n", appl_args->mode);
		exit(EXIT_FAILURE);
	}

	if (appl_args->put_mode != ODP_STASH_OP_MT &&
	    appl_args->put_mode != ODP_STASH_OP_ST &&
	    appl_args->put_mode != ODP_STASH_OP_LOCAL) {
		printf("Invalid put operation mode: %d\n", appl_args->put_mode);
		exit(EXIT_FAILURE);
	}

	if (appl_args->get_mode != ODP_STASH_OP_MT &&
	    appl_args->get_mode != ODP_STASH_OP_ST &&
	    appl_args->get_mode != ODP_STASH_OP_LOCAL) {
		printf("Invalid get operation mode: %d\n", appl_args->get_mode);
		exit(EXIT_FAILURE);
	}

	/* ODP_STASH_OP_LOCAL must be set to both put and get modes */
	if ((appl_args->put_mode == ODP_STASH_OP_LOCAL ||
	     appl_args->get_mode == ODP_STASH_OP_LOCAL) &&
	    appl_args->put_mode != appl_args->get_mode) {
		printf("Local operation mode must be set to both put and get modes\n");
		exit(EXIT_FAILURE);
	}

	optind = 1; /* Reset 'extern optind' from the getopt lib */
}

/**
 * Print system and application info
 */
static void print_info(void)
{
	odp_sys_info_print();

	printf("\n"
	       "odp_bench_stash options\n"
	       "-----------------------\n");

	static const char *op_mode_str[] = {"MT safe", "single thread safe", "thread local"};

	printf("Burst size:       %d\n", gbl_args->appl.burst_size);
	printf("CPU mask:         %s\n", gbl_args->cpumask_str);
	printf("Put mode:         %s\n", op_mode_str[gbl_args->appl.put_mode]);
	printf("Get mode:         %s\n", op_mode_str[gbl_args->appl.get_mode]);
	printf("Strict size:      %s\n", gbl_args->appl.strict_size ? "yes" : "no");
	if (gbl_args->appl.cache_size < 0)
		printf("Stash cache size: default\n");
	else
		printf("Stash cache size: %d\n", gbl_args->appl.cache_size);
	printf("Measurement unit: %s\n", gbl_args->appl.time ? "nsec" : "CPU cycles");
	printf("Test rounds:      %u\n", gbl_args->appl.rounds);
	printf("Measurement mode: %s\n", gbl_args->appl.mode == M_TPUT ? "throughput" : "latency");
	printf("\n");
}

static int bench_stash_tm_export(void *data)
{
	args_t *gbl_args = data;
	bench_tm_result_t *res;
	uint64_t num;
	int ret = 0;
	const char *unit = gbl_args->appl.time ? "nsec" : "cpu cycles";

	if (test_common_write("function name,min %s per function call,"
			      "average %s per function call,"
			      "max %s per function call\n", unit, unit, unit)) {
		ret = -1;
		goto exit;
	}

	for (uint32_t i = 0; i < gbl_args->suite.t.num_bench; i++) {
		res = &gbl_args->suite.t.result[i];

		for (int j = 0; j < res->num; j++) {
			num = res->func[j].num ? res->func[j].num : 1;
			if (test_common_write("%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
					      res->func[j].name,
					      bench_tm_to_u64(res, &res->func[j].min),
					      bench_tm_to_u64(res, &res->func[j].tot) / num,
					      bench_tm_to_u64(res, &res->func[j].max))) {
				ret = -1;
				goto exit;
			}
		}
	}

exit:
	test_common_write_term();

	return ret;
}

static int bench_stash_export(void *data)
{
	args_t *gbl_args = data;
	int ret = 0;

	if (test_common_write("%s", gbl_args->appl.time ?
			      "function name,average nsec per function call\n" :
			      "function name,average cpu cycles per function call\n")) {
		ret = -1;
		goto exit;
	}

	for (int i = 0; i < gbl_args->suite.b.num_bench; i++) {
		if (test_common_write("odp_%s,%f\n", gbl_args->suite.b.bench[i].desc != NULL ?
					gbl_args->suite.b.bench[i].desc :
					gbl_args->suite.b.bench[i].name,
				      gbl_args->suite.b.result[i])) {
			ret = -1;
			goto exit;
		}
	}

exit:
	test_common_write_term();

	return ret;
}

/**
 * Test functions
 */
bench_info_t test_suite[] = {
	BENCH_INFO_COND(stash_put, drain_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_put_batch, drain_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_get, fill_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_get_batch, fill_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_put_u32, drain_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_put_u32_batch, drain_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_get_u32, fill_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_get_u32_batch, fill_u32, drain_u32, NULL, check_u32),
	BENCH_INFO_COND(stash_put_u64, drain_u64, drain_u64, NULL, check_u64),
	BENCH_INFO_COND(stash_put_u64_batch, drain_u64, drain_u64, NULL, check_u64),
	BENCH_INFO_COND(stash_get_u64, fill_u64, drain_u64, NULL, check_u64),
	BENCH_INFO_COND(stash_get_u64_batch, fill_u64, drain_u64, NULL, check_u64),
	BENCH_INFO_COND(stash_put_ptr, drain_ptr, drain_ptr, NULL, check_ptr),
	BENCH_INFO_COND(stash_put_ptr_batch, drain_ptr, drain_ptr, NULL, check_ptr),
	BENCH_INFO_COND(stash_get_ptr, fill_ptr, drain_ptr, NULL, check_ptr),
	BENCH_INFO_COND(stash_get_ptr_batch, fill_ptr, drain_ptr, NULL, check_ptr),
	BENCH_INFO_COND(stash_flush_cache, NULL, NULL, NULL, check_cache),
	BENCH_INFO_COND(stash_stats, NULL, NULL, NULL, check_stats),
};

ODP_STATIC_ASSERT(ODPH_ARRAY_SIZE(test_suite) < TEST_MAX_BENCH,
		  "Result array is too small to hold all the results");

/**
 * Test functions
 */
bench_tm_info_t test_suite_tm[] = {
	BENCH_INFO_TM(stash_put_tm, drain_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_put_batch_tm, drain_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_get_tm, fill_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_get_batch_tm, fill_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_put_u32_tm, drain_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_put_u32_batch_tm, drain_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_get_u32_tm, fill_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_get_u32_batch_tm, fill_u32, drain_u32, check_u32),
	BENCH_INFO_TM(stash_put_u64_tm, drain_u64, drain_u64, check_u64),
	BENCH_INFO_TM(stash_put_u64_batch_tm, drain_u64, drain_u64, check_u64),
	BENCH_INFO_TM(stash_get_u64_tm, fill_u64, drain_u64, check_u64),
	BENCH_INFO_TM(stash_get_u64_batch_tm, fill_u64, drain_u64, check_u64),
	BENCH_INFO_TM(stash_put_ptr_tm, drain_ptr, drain_ptr, check_ptr),
	BENCH_INFO_TM(stash_put_ptr_batch_tm, drain_ptr, drain_ptr, check_ptr),
	BENCH_INFO_TM(stash_get_ptr_tm, fill_ptr, drain_ptr, check_ptr),
	BENCH_INFO_TM(stash_get_ptr_batch_tm, fill_ptr, drain_ptr, check_ptr),
	BENCH_INFO_TM(stash_flush_cache_tm, NULL, NULL, check_cache),
	BENCH_INFO_TM(stash_stats_tm, NULL, NULL, check_stats),
};

ODP_STATIC_ASSERT(ODPH_ARRAY_SIZE(test_suite_tm) < TEST_MAX_BENCH,
		  "Result array is too small to hold all the results");

static void init_suite(args_t *gbl_args, odp_bool_t is_export)
{
	if (gbl_args->appl.mode == M_LATENCY) {
		bench_tm_suite_init(&gbl_args->suite.t);
		gbl_args->suite.t.bench = test_suite_tm;
		gbl_args->suite.t.num_bench = ODPH_ARRAY_SIZE(test_suite_tm);
		gbl_args->suite.t.rounds = TEST_REPEAT_COUNT;
		gbl_args->suite.t.bench_idx = gbl_args->appl.bench_idx;
		gbl_args->suite.t.measure_time = !!gbl_args->appl.time;
		gbl_args->suite.exit = &gbl_args->suite.t.exit_worker;
		gbl_args->suite.retval = &gbl_args->suite.t.retval;
		gbl_args->suite.args = &gbl_args->suite.t;
		gbl_args->suite.suite_fn = bench_tm_run;
		gbl_args->suite.export_fn = bench_stash_tm_export;

		if (is_export)
			gbl_args->suite.t.result = gbl_args->result.t;
	} else {
		bench_suite_init(&gbl_args->suite.b);
		gbl_args->suite.b.bench = test_suite;
		gbl_args->suite.b.num_bench = ODPH_ARRAY_SIZE(test_suite);
		gbl_args->suite.b.indef_idx = gbl_args->appl.bench_idx;
		gbl_args->suite.b.rounds = gbl_args->appl.rounds;
		gbl_args->suite.b.repeat_count = TEST_REPEAT_COUNT;
		gbl_args->suite.b.measure_time = !!gbl_args->appl.time;
		gbl_args->suite.exit = &gbl_args->suite.b.exit_worker;
		gbl_args->suite.retval = &gbl_args->suite.b.retval;
		gbl_args->suite.args = &gbl_args->suite.b;
		gbl_args->suite.suite_fn = bench_run;
		gbl_args->suite.export_fn = bench_stash_export;

		if (is_export)
			gbl_args->suite.b.result = gbl_args->result.b;
	}
}

static odp_stash_t create_stash(uint32_t obj_size)
{
	odp_stash_param_t param;

	odp_stash_param_init(&param);
	param.type = ODP_STASH_TYPE_DEFAULT;
	param.put_mode = gbl_args->appl.put_mode;
	param.get_mode = gbl_args->appl.get_mode;
	param.num_obj = TEST_STASH_SIZE;
	param.obj_size = obj_size;
	param.strict_size = gbl_args->appl.strict_size;

	if (gbl_args->have_cache)
		param.cache_size = gbl_args->appl.cache_size;

	if (gbl_args->have_stats)
		param.stats.bit.count = 1;

	return odp_stash_create("microbench", &param);
}

/**
 * ODP stash microbenchmark application
 */
int main(int argc, char *argv[])
{
	odph_helper_options_t helper_options;
	test_common_options_t common_options;
	odph_thread_t worker_thread;
	odph_thread_common_param_t thr_common;
	odph_thread_param_t thr_param;
	int cpu;
	odp_shm_t shm;
	odp_cpumask_t cpumask, default_mask;
	odp_stash_capability_t capa;
	odp_instance_t instance;
	odp_init_t init_param;
	uint8_t ret;

	/* Let helper collect its own arguments (e.g. --odph_proc) */
	argc = odph_parse_options(argc, argv);
	if (odph_options(&helper_options)) {
		ODPH_ERR("Error: reading ODP helper options failed\n");
		exit(EXIT_FAILURE);
	}

	argc = test_common_parse_options(argc, argv);
	if (test_common_options(&common_options)) {
		ODPH_ERR("Error: reading test options failed\n");
		exit(EXIT_FAILURE);
	}

	odp_init_param_init(&init_param);
	init_param.mem_model = helper_options.mem_model;

	/* Init ODP before calling anything else */
	if (odp_init_global(&instance, &init_param, NULL)) {
		ODPH_ERR("Error: ODP global init failed\n");
		exit(EXIT_FAILURE);
	}

	/* Init this thread */
	if (odp_init_local(instance, ODP_THREAD_CONTROL)) {
		ODPH_ERR("Error: ODP local init failed\n");
		exit(EXIT_FAILURE);
	}

	/* Reserve memory for args from shared mem */
	shm = odp_shm_reserve("shm_args", sizeof(args_t), ODP_CACHE_LINE_SIZE, 0);
	if (shm == ODP_SHM_INVALID) {
		ODPH_ERR("Error: shared mem reserve failed\n");
		exit(EXIT_FAILURE);
	}

	gbl_args = odp_shm_addr(shm);
	if (gbl_args == NULL) {
		ODPH_ERR("Error: shared mem alloc failed\n");
		exit(EXIT_FAILURE);
	}

	memset(gbl_args, 0, sizeof(args_t));
	gbl_args->stash_u32 = ODP_STASH_INVALID;
	gbl_args->stash_u64 = ODP_STASH_INVALID;
	gbl_args->stash_ptr = ODP_STASH_INVALID;

	/* Parse and store the application arguments */
	parse_args(argc, argv, &gbl_args->appl);
	init_suite(gbl_args, common_options.is_export);

	/* Get default worker cpumask */
	if (odp_cpumask_default_worker(&default_mask, 1) != 1) {
		ODPH_ERR("Error: unable to allocate worker thread\n");
		exit(EXIT_FAILURE);
	}
	(void)odp_cpumask_to_str(&default_mask, gbl_args->cpumask_str,
				 sizeof(gbl_args->cpumask_str));

	if (odp_stash_capability(&capa, ODP_STASH_TYPE_DEFAULT)) {
		ODPH_ERR("Error: unable to query stash capability\n");
		exit(EXIT_FAILURE);
	}

	if (capa.max_num_obj < TEST_STASH_SIZE) {
		ODPH_ERR("Error: stash size not supported (max %" PRIu64 ")\n", capa.max_num_obj);
		exit(EXIT_FAILURE);
	}

	if (gbl_args->appl.cache_size > (int)capa.max_cache_size) {
		ODPH_ERR("Error: cache size not supported (max %" PRIu32 ")\n",
			 capa.max_cache_size);
		exit(EXIT_FAILURE);
	}

	/* Clamp burst size to supported batch sizes */
	if (gbl_args->appl.burst_size > (int)capa.max_put_batch)
		gbl_args->appl.burst_size = capa.max_put_batch;
	if (gbl_args->appl.burst_size > (int)capa.max_get_batch)
		gbl_args->appl.burst_size = capa.max_get_batch;

	gbl_args->have_cache = gbl_args->appl.cache_size > 0;
	gbl_args->have_stats = !!capa.stats.bit.count;

	gbl_args->have_u32 = capa.max_obj_size >= sizeof(uint32_t);
	gbl_args->have_u64 = capa.max_obj_size >= sizeof(uint64_t);
	gbl_args->have_ptr = capa.max_obj_size >= sizeof(uintptr_t);

	print_info();

	/* The 32-bit stash is always required (also used for generic put/get) */
	gbl_args->stash_u32 = create_stash(sizeof(uint32_t));
	if (gbl_args->stash_u32 == ODP_STASH_INVALID) {
		ODPH_ERR("Error: stash create failed\n");
		exit(EXIT_FAILURE);
	}
	gbl_args->have_u32 = 1;

	if (gbl_args->have_u64) {
		gbl_args->stash_u64 = create_stash(sizeof(uint64_t));
		if (gbl_args->stash_u64 == ODP_STASH_INVALID) {
			ODPH_ERR("Error: stash create failed\n");
			exit(EXIT_FAILURE);
		}
	}

	if (gbl_args->have_ptr) {
		gbl_args->stash_ptr = create_stash(sizeof(uintptr_t));
		if (gbl_args->stash_ptr == ODP_STASH_INVALID) {
			ODPH_ERR("Error: stash create failed\n");
			exit(EXIT_FAILURE);
		}
	}

	memset(&worker_thread, 0, sizeof(odph_thread_t));

	signal(SIGINT, sig_handler);

	/* Create worker thread */
	cpu = odp_cpumask_first(&default_mask);

	odp_cpumask_zero(&cpumask);
	odp_cpumask_set(&cpumask, cpu);

	odph_thread_common_param_init(&thr_common);
	thr_common.instance = instance;
	thr_common.cpumask = &cpumask;
	thr_common.share_param = 1;

	odph_thread_param_init(&thr_param);
	thr_param.start = gbl_args->suite.suite_fn;
	thr_param.arg = gbl_args->suite.args;
	thr_param.thr_type = ODP_THREAD_WORKER;

	odph_thread_create(&worker_thread, &thr_common, &thr_param, 1);

	odph_thread_join(&worker_thread, 1);

	ret = *gbl_args->suite.retval;

	if (ret == 0 && common_options.is_export) {
		if (gbl_args->suite.export_fn(gbl_args)) {
			ODPH_ERR("Error: Export failed\n");
			ret = -1;
		}
	}

	if (gbl_args->stash_ptr != ODP_STASH_INVALID &&
	    odp_stash_destroy(gbl_args->stash_ptr)) {
		ODPH_ERR("Error: stash destroy\n");
		exit(EXIT_FAILURE);
	}

	if (gbl_args->stash_u64 != ODP_STASH_INVALID &&
	    odp_stash_destroy(gbl_args->stash_u64)) {
		ODPH_ERR("Error: stash destroy\n");
		exit(EXIT_FAILURE);
	}

	if (odp_stash_destroy(gbl_args->stash_u32)) {
		ODPH_ERR("Error: stash destroy\n");
		exit(EXIT_FAILURE);
	}

	if (odp_shm_free(shm)) {
		ODPH_ERR("Error: shm free\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_local()) {
		ODPH_ERR("Error: term local\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_global(instance)) {
		ODPH_ERR("Error: term global\n");
		exit(EXIT_FAILURE);
	}

	return ret;
}
