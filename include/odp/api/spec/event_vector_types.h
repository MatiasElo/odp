/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Nokia
 */

/**
 * @file
 *
 * ODP event vector API type definitions
 */

#ifndef ODP_API_SPEC_EVENT_VECTOR_TYPES_H_
#define ODP_API_SPEC_EVENT_VECTOR_TYPES_H_
#include <odp/visibility_begin.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/event_types.h>
#include <odp/api/std_types.h>

/** @defgroup odp_event_vector ODP EVENT VECTOR
 *  @{
 */

/**
 * @typedef odp_event_vector_t
 * ODP event vector
 */

/**
 * @def ODP_EVENT_VECTOR_INVALID
 * Invalid event vector
 */

/**
 * Event vector enqueue flags
 */
typedef enum {
	/** Begin new vector
	 *
	 * Forward possible previous events waiting for vector formation
	 * immediately and start a new vector for enqueued events. */
	ODP_EVENT_VECTOR_FLAG_FIRST = 0x1,

	/** Last event of vector
	 *
	 * Forward possible previous events waiting for vector formation and the
	 * enqueued events immediately regardless of vector 'max_tmo_ns' or
	 * 'max_size' parameters. Depending on 'max_size' value, multiple event
	 * vectors may be forwarded. */
	ODP_EVENT_VECTOR_FLAG_LAST = 0x2

} odp_event_vector_flag_t;

/**
 * Event vector configuration
 */
typedef struct odp_event_vector_config_t {
	/** Vector pool
	 *
	 * Vector pool to allocate the vectors to hold events. The pool must
	 * have been created with the ODP_POOL_VECTOR type.
	 */
	odp_pool_t pool;

	/** Maximum time to wait for events
	 *
	 * Maximum time in nanoseconds for the producer to form the vector of
	 * events. This value should be in the range of
	 * odp_event_vector_capability_t::min_tmo_ns to
	 * odp_event_vector_capability_t::max_tmo_ns.
	 */
	uint64_t max_tmo_ns;

	/** Maximum number of events in vector
	 *
	 * The producer forms event vector events when either it reaches
	 * odp_event_vector_config_t::max_tmo_ns or max_size events. This value
	 * should be in the range of
	 * odp_event_vector_capability_t::min_size to
	 * odp_event_vector_capability_t::max_size.
	 *
	 * The maximum number of events a vector can hold is defined by
	 * odp_pool_param_t::vector::max_size of odp_event_vector_config_t::pool.
	 * The max_size must not be greater than odp_pool_param_t::vector::max_size.
	 */
	uint32_t max_size;

	/** ToDo: Event type
	 *
	 * Event type of vector context. If 'event_type' is ODP_EVENT_VECTOR,
	 * user is allowed to enqueue any event types to the vector context.
	 * Otherwise, only events of the 'event_type' are allowed. The default
	 * value is ODP_EVENT_VECTOR.
	 */
	odp_event_type_t event_type;

} odp_event_vector_config_t;

/**
 * Event vector capabilities
 */
typedef struct odp_event_vector_capability_t {
	/** Event vector availability */
	odp_support_t supported;

	/** ToDo: Plain queue support
	 *
	 * This defines whether plain queues support event vectorization.
	 */
	odp_bool_t queue_type_plain;

	/** Maximum number of events that can be accumulated into an event vector
	 *
	 * odp_event_vector_config_t::max_size should not be greater than this
	 * value. */
	uint32_t max_size;

	/** Minimum value allowed to be configured to
	 *  odp_event_vector_config_t::max_size */
	uint32_t min_size;

	/** Maximum time in nanoseconds for a producer to form an event vector
	 *
	 * odp_event_vector_config_t::max_tmo_ns should not be greater than this
	 * value. */
	uint64_t max_tmo_ns;

	/** Minimum value allowed to be configured to
	 *  odp_event_vector_config_t::max_tmo_ns */
	uint64_t min_tmo_ns;

} odp_event_vector_capability_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <odp/visibility_end.h>
#endif
