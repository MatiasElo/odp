/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2025 Nokia
 */

/**
 * @example odp_atomic_stress.c
 *
 * Test application that can be used to validate atomic queue atomicity and
 * event ordering.
 *
 * @cond _ODP_HIDE_FROM_DOXYGEN_
 */

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>

#define TEST_EVENTS 1024

typedef struct test_options_t {
	uint32_t burst_size;
	uint32_t num_cpu;
	uint32_t num_rounds;

} test_options_t;

typedef struct thread_arg_t {
	void *global;

} thread_arg_t;

typedef struct event_payload_t {
	uint64_t seqnum;

} event_payload_t;

typedef struct test_global_t {
	test_options_t test_options;
	odp_atomic_u32_t exit_test;
	odp_barrier_t barrier;
	odp_cpumask_t cpumask;
	odp_pool_t pool;
	void *worker_mem;
	odp_queue_t queue;
	odph_thread_t thread_tbl[ODP_THREAD_COUNT_MAX];
	thread_arg_t thread_arg[ODP_THREAD_COUNT_MAX];

	struct {
		odp_buffer_t buf;
		uint64_t seqnum;
		odp_time_t ts;
		int thread;
	} rx_event[TEST_EVENTS];

	odp_atomic_u64_t global_seqnum;
	odp_atomic_u32_t queue_state;

} test_global_t;

test_global_t *test_global;

static void print_usage(void)
{
	printf("\n"
	       "Scheduler test options:\n"
	       "\n"
	       "  -c, --num_cpu          Number of CPUs (worker threads). 0: all available CPUs. Default: 1\n"
	       "  -b, --burst_size       Number of events to enqueue at once. Default: 1\n"
	       "  -r, --num_rounds       Number of test rounds. Default: 1\n"
	       "  -h, --help             This help\n"
	       "\n");
}

static int parse_options(int argc, char *argv[], test_options_t *test_options)
{
	int opt;
	int ret = 0;

	static const struct option longopts[] = {
		{"burst_size",   required_argument, NULL, 'b'},
		{"num_cpu",      required_argument, NULL, 'c'},
		{"num_rounds",   required_argument, NULL, 'r'},
		{"help",         no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	static const char *shortopts = "+b:c:r:h";

	test_options->burst_size  = 1;
	test_options->num_cpu     = 1;
	test_options->num_rounds  = 1;

	while (1) {
		opt = getopt_long(argc, argv, shortopts, longopts, NULL);

		if (opt == -1)
			break;

		switch (opt) {
		case 'b':
			test_options->burst_size = atoi(optarg);
			break;
		case 'c':
			test_options->num_cpu = atoi(optarg);
			break;
		case 'r':
			test_options->num_rounds = atoi(optarg);
			break;
		case 'h':
			/* fall through */
		default:
			print_usage();
			ret = -1;
			break;
		}
	}

	return ret;
}

static int set_num_cpu(test_global_t *global)
{
	int ret;
	test_options_t *test_options = &global->test_options;
	int num_cpu = test_options->num_cpu;

	/* One thread used for the main thread */
	if (num_cpu < 0 || num_cpu > ODP_THREAD_COUNT_MAX - 1) {
		ODPH_ERR("Bad number of workers. Maximum is %i.\n", ODP_THREAD_COUNT_MAX - 1);
		return -1;
	}

	ret = odp_cpumask_default_worker(&global->cpumask, num_cpu);

	if (num_cpu && ret != num_cpu) {
		ODPH_ERR("Too many workers. Max supported %i\n.", ret);
		return -1;
	}

	/* Zero: all available workers */
	if (num_cpu == 0) {
		num_cpu = ret;
		test_options->num_cpu = num_cpu;
	}

	odp_barrier_init(&global->barrier, num_cpu + 1);

	return 0;
}

static int worker_thread(void *arg)
{
	const int thr = odp_thread_id();
	odp_event_t ev;
	thread_arg_t *thread_arg = arg;
	test_global_t *global = thread_arg->global;
	uint64_t received = 0, global_seqnum;
	event_payload_t *payload;
	odp_buffer_t buf;
	uint32_t state;
	uint64_t seqnum;
	uint64_t sched_calls = 0;
	odp_time_t ts1, ts2;
	double sched_calls_per_sec;
	int ret = 0;

	/* Start all workers at the same time */
	odp_barrier_wait(&global->barrier);

	ts1 = odp_time_global_strict();
	while (!odp_atomic_load_u32(&global->exit_test)) {
		ev = odp_schedule(NULL, ODP_SCHED_NO_WAIT);

		sched_calls++;

		if (ev == ODP_EVENT_INVALID)
			continue;

		/* Check for atomicity */
		state = odp_atomic_fetch_inc_u32(&global->queue_state);
		if (state != 0) {
			ODPH_ERR("!!! Thread %2i: Error: queue_state %u != 0\n", thr, state);
			ret = -1;
		}

		/* Check event order */
		buf = odp_buffer_from_event(ev);
		payload = odp_buffer_addr(buf);
		seqnum = payload->seqnum;
		global_seqnum = odp_atomic_fetch_inc_u64(&global->global_seqnum);

		global->rx_event[global_seqnum].buf = buf;
		global->rx_event[global_seqnum].seqnum = seqnum;
		global->rx_event[global_seqnum].thread = thr;
		global->rx_event[global_seqnum].ts = odp_time_global();

		if (global_seqnum != seqnum) {
			ODPH_ERR("!!! Thread %2i: Error: seqnum %" PRIu64 " != %" PRIu64 "\n", thr,
				 global_seqnum, seqnum);
			ret = -1;
		}
		received++;

		/* Check for atomicity */
		state = odp_atomic_fetch_dec_u32(&global->queue_state);
		if (state != 1) {
			ODPH_ERR("!!! Thread %2i: Error: queue_state %u != 1, seqnum %" PRIu64 "\n",
				 thr, state, global_seqnum);
			ret = -1;
		}
	}
	ts2 = odp_time_global_strict();

	sched_calls_per_sec = sched_calls / ((double)odp_time_diff_ns(ts2, ts1) /
					     ODP_TIME_SEC_IN_NS);

	printf("  Thread %2i processed %" PRIu64 " events, %.4fM/sec sched calls\n",
	       thr, received, sched_calls_per_sec / 1000000);

	return ret;
}

static int start_workers(test_global_t *global, odp_instance_t instance)
{
	odph_thread_common_param_t thr_common;
	int i, ret;
	test_options_t *test_options = &global->test_options;
	int num_cpu   = test_options->num_cpu;
	odph_thread_param_t thr_param[num_cpu];

	memset(global->thread_tbl, 0, sizeof(global->thread_tbl));
	odph_thread_common_param_init(&thr_common);

	thr_common.instance = instance;
	thr_common.cpumask  = &global->cpumask;

	for (i = 0; i < num_cpu; i++) {
		odph_thread_param_init(&thr_param[i]);
		thr_param[i].start = worker_thread;
		thr_param[i].arg      = &global->thread_arg[i];
		thr_param[i].thr_type = ODP_THREAD_WORKER;
	}

	ret = odph_thread_create(global->thread_tbl, &thr_common, thr_param, num_cpu);

	if (ret != num_cpu) {
		ODPH_ERR("Thread create failed %i\n", ret);
		return -1;
	}

	return 0;
}

static int create_queue(test_global_t *global)
{
	odp_queue_param_t queue_param;

	odp_queue_param_init(&queue_param);
	queue_param.type = ODP_QUEUE_TYPE_SCHED;
	queue_param.size = TEST_EVENTS;
	queue_param.sched.sync = ODP_SCHED_SYNC_ATOMIC;
	queue_param.sched.group = ODP_SCHED_GROUP_ALL;

	global->queue = odp_queue_create(NULL, &queue_param);
	if (global->queue == ODP_QUEUE_INVALID) {
		ODPH_ERR("Queue create failed\n");
		return -1;
	}

	return 0;
}

static void destroy_queue(test_global_t *global)
{
	odp_queue_t queue = global->queue;

	if (odp_queue_destroy(queue))
		ODPH_ERR("Queue destroy failed\n");
}

static int create_pool(test_global_t *global)
{
	odp_pool_param_t pool_param;

	odp_pool_param_init(&pool_param);
	pool_param.type = ODP_POOL_BUFFER;
	pool_param.buf.num = TEST_EVENTS;
	pool_param.buf.size = sizeof(event_payload_t);

	global->pool = odp_pool_create("stress_pool", &pool_param);
	if (global->pool == ODP_POOL_INVALID) {
		ODPH_ERR("Pool create failed\n");
		return -1;
	}

	return 0;
}

static void destroy_pool(test_global_t *global)
{
	if (odp_pool_destroy(global->pool))
		ODPH_ERR("Pool destroy failed\n");
}

static void sig_handler(int signo)
{
	(void)signo;

	if (test_global == NULL)
		return;

	odp_atomic_add_u32(&test_global->exit_test, 1);
}

static int enqueue_events_and_wait(test_global_t *global)
{
	const odp_queue_t queue = global->queue;
	odp_buffer_t buf_tbl[TEST_EVENTS];
	odp_event_t event_tbl[TEST_EVENTS];
	event_payload_t *payload;
	int num_enqueued = 0;

	for (int i = 0; i < TEST_EVENTS; i++) {
		buf_tbl[i] = odp_buffer_alloc(global->pool);
		if (buf_tbl[i] == ODP_BUFFER_INVALID) {
			ODPH_ERR("Buffer alloc failed\n");
			return -1;
		}

		payload = odp_buffer_addr(buf_tbl[i]);
		payload->seqnum = i;
	}

	odp_buffer_to_event_multi(buf_tbl, event_tbl, TEST_EVENTS);

	while (num_enqueued < TEST_EVENTS) {
		uint32_t num_left = TEST_EVENTS - num_enqueued;
		uint32_t burst_size = ODPH_MIN(global->test_options.burst_size, num_left);
		int ret;

		ret = odp_queue_enq_multi(queue, &event_tbl[num_enqueued], burst_size);

		if (ret < 0) {
			ODPH_ERR("Queue enqueue failed\n");
			return -1;
		}
		num_enqueued += ret;
	}

	/* Wait for all events to be processed */
	while (odp_atomic_load_u64(&global->global_seqnum) < TEST_EVENTS)
		;

	odp_atomic_store_u32(&global->exit_test, 1);

	return 0;
}

static void print_debug(test_global_t *global)
{
	for (uint64_t i = 0; i < TEST_EVENTS; i++) {
		odp_bool_t valid = global->rx_event[i].seqnum == i;

		printf(" %s RX event %" PRIu64 ": seqnum %" PRIu64 ", thr %2d, time %" PRIu64 "\n",
		       valid ? "" : "!!!", i, global->rx_event[i].seqnum,
		       global->rx_event[i].thread, odp_time_to_ns(global->rx_event[i].ts));
	}
}

static void init_test_round(test_global_t *global)
{
	odp_atomic_store_u32(&global->exit_test, 0);
	odp_atomic_store_u64(&global->global_seqnum, 0);

	for (uint64_t i = 0; i < TEST_EVENTS; i++) {
		global->rx_event[i].buf = ODP_BUFFER_INVALID;
		global->rx_event[i].seqnum = 0;
		global->rx_event[i].thread = -1;
		global->rx_event[i].ts = ODP_TIME_NULL;
	}
}

static void clean_test_round(test_global_t *global)
{
	for (uint64_t i = 0; i < TEST_EVENTS; i++) {
		if (global->rx_event[i].buf != ODP_BUFFER_INVALID)
			odp_buffer_free(global->rx_event[i].buf);
	}
}

int main(int argc, char **argv)
{
	odph_helper_options_t helper_options;
	odp_instance_t instance;
	odp_init_t init;
	odp_shm_t shm, shm_global;
	test_global_t *global;
	test_options_t *test_options;
	int i;
	uint32_t num_cpu;

	signal(SIGINT, sig_handler);

	/* Let helper collect its own arguments (e.g. --odph_proc) */
	argc = odph_parse_options(argc, argv);
	if (odph_options(&helper_options)) {
		ODPH_ERR("Reading ODP helper options failed.\n");
		exit(EXIT_FAILURE);
	}

	odp_init_param_init(&init);
	init.mem_model = helper_options.mem_model;

	if (odp_init_global(&instance, &init, NULL)) {
		ODPH_ERR("Global init failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_init_local(instance, ODP_THREAD_CONTROL)) {
		ODPH_ERR("Local init failed.\n");
		exit(EXIT_FAILURE);
	}

	shm = odp_shm_reserve("Stress global", sizeof(test_global_t), ODP_CACHE_LINE_SIZE, 0);
	shm_global = shm;
	if (shm == ODP_SHM_INVALID) {
		ODPH_ERR("SHM reserve failed.\n");
		exit(EXIT_FAILURE);
	}

	global = odp_shm_addr(shm);
	if (global == NULL) {
		ODPH_ERR("SHM addr failed\n");
		exit(EXIT_FAILURE);
	}
	test_global = global;

	memset(global, 0, sizeof(test_global_t));
	odp_atomic_init_u32(&global->exit_test, 0);
	odp_atomic_init_u32(&global->queue_state, 0);
	odp_atomic_init_u64(&global->global_seqnum, 0);

	for (i = 0; i < ODP_THREAD_COUNT_MAX; i++)
		global->thread_arg[i].global = global;

	if (parse_options(argc, argv, &global->test_options))
		exit(EXIT_FAILURE);

	test_options = &global->test_options;

	odp_sys_info_print();

	odp_schedule_config(NULL);

	if (set_num_cpu(global))
		exit(EXIT_FAILURE);

	num_cpu = test_options->num_cpu;

	printf("\n");
	printf("Test parameters:\n");
	printf("  burst size          %u\n", test_options->burst_size);
	printf("  num workers         %u\n\n", num_cpu);

	if (create_queue(global))
		exit(EXIT_FAILURE);

	if (create_pool(global))
		exit(EXIT_FAILURE);

	for (uint32_t round = 0; round < test_options->num_rounds; round++) {
		printf("Round %i\n", round + 1);

		init_test_round(global);

		start_workers(global, instance);

		/* Wait until all workers are ready to receive events */
		odp_barrier_wait(&global->barrier);

		if (enqueue_events_and_wait(global)) {
			ODPH_ERR("Enqueue events failed\n");
			exit(EXIT_FAILURE);
		}

		/* Wait workers to exit */
		if (odph_thread_join(global->thread_tbl, num_cpu) != (int)num_cpu) {
			print_debug(global);
			printf("FAIL\n");
			exit(EXIT_FAILURE);
		}

		clean_test_round(global);
	}

	destroy_pool(global);

	destroy_queue(global);

	if (odp_shm_free(shm_global)) {
		ODPH_ERR("SHM free failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_local()) {
		ODPH_ERR("Term local failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_global(instance)) {
		ODPH_ERR("Term global failed.\n");
		exit(EXIT_FAILURE);
	}

	printf("PASS\n");

	return 0;
}
