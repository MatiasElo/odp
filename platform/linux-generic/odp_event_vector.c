/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Nokia
 */

#include <odp/api/align.h>
#include <odp/api/event_vector.h>
#include <odp/api/hints.h>
#include <odp/api/packet.h>
#include <odp/api/pool.h>

#include <odp/api/plat/event_vector_inlines.h>
#include <odp/api/plat/strong_types.h>

#include <odp_debug_internal.h>
#include <odp_event_vector_internal.h>
#include <odp_pool_internal.h>
#include <odp_string_internal.h>

#include <inttypes.h>
#include <stdint.h>

#include <odp/visibility_begin.h>

/* Event vector header field offsets for inline functions */
const _odp_event_vector_inline_offset_t _odp_event_vector_inline ODP_ALIGNED_CACHE = {
	.event      = offsetof(odp_event_vector_hdr_t, event),
	.pool       = offsetof(odp_event_vector_hdr_t, event_hdr.pool),
	.size       = offsetof(odp_event_vector_hdr_t, size),
	.uarea_addr = offsetof(odp_event_vector_hdr_t, uarea_addr),
	.flags      = offsetof(odp_event_vector_hdr_t, flags)
};

#include <odp/visibility_end.h>

static inline odp_event_vector_hdr_t *event_vector_hdr_from_event(odp_event_t event)
{
	return (odp_event_vector_hdr_t *)(uintptr_t)event;
}

uint32_t odp_event_vector_type(odp_event_vector_t evv,
			       odp_event_type_t *event_type)
{
	odp_event_type_t first_type;
	odp_event_vector_hdr_t *evv_hdr = _odp_event_vector_hdr(evv);
	uint32_t vector_size = odp_event_vector_size(evv);

	_ODP_ASSERT(event_type != NULL);

	/* ToDo: optimization when vector context event type is set */

	if (odp_unlikely(vector_size == 0))
		return 0;

	first_type = odp_event_type(evv_hdr->event[0]);

	for (uint32_t i = 1; i < vector_size; i++) {
		if (odp_event_type(evv_hdr->event[i]) != first_type)
			return 0;
	}
	*event_type = first_type;
	return 1;
}

odp_event_vector_t odp_event_vector_alloc(odp_pool_t pool_hdl)
{
	odp_event_t event;
	pool_t *pool;

	_ODP_ASSERT(pool_hdl != ODP_POOL_INVALID);

	pool = _odp_pool_entry(pool_hdl);

	_ODP_ASSERT(pool->type == ODP_POOL_VECTOR);

	event = _odp_event_alloc(pool);
	if (odp_unlikely(event == ODP_EVENT_INVALID))
		return ODP_EVENT_VECTOR_INVALID;

	_ODP_ASSERT(event_vector_hdr_from_event(event)->size == 0);

	return odp_event_vector_from_event(event);
}

void odp_event_vector_free(odp_event_vector_t evv)
{
	odp_event_vector_hdr_t *evv_hdr = _odp_event_vector_hdr(evv);

	evv_hdr->size = 0;
	evv_hdr->flags.all_flags = 0;

	_odp_event_free(odp_event_vector_to_event(evv));
}

int odp_event_vector_valid(odp_event_vector_t evv)
{
	odp_event_vector_hdr_t *evv_hdr;
	odp_event_t ev;
	pool_t *pool;
	uint32_t i;

	if (odp_unlikely(evv == ODP_EVENT_VECTOR_INVALID))
		return 0;

	ev = odp_event_vector_to_event(evv);

	if (_odp_event_is_valid(ev) == 0)
		return 0;

	if (odp_event_type(ev) != ODP_EVENT_VECTOR)
		return 0;

	evv_hdr = _odp_event_vector_hdr(evv);
	pool = _odp_pool_entry(evv_hdr->event_hdr.pool);

	if (odp_unlikely(evv_hdr->size > pool->params.vector.max_size))
		return 0;

	for (i = 0; i < evv_hdr->size; i++) {
		if (evv_hdr->event[i] == ODP_EVENT_INVALID)
			return 0;
	}

	return 1;
}

void odp_event_vector_print(odp_event_vector_t evv)
{
	int max_len = 4096;
	char str[max_len];
	int len = 0;
	int n = max_len - 1;
	uint32_t i;
	odp_event_vector_hdr_t *evv_hdr = _odp_event_vector_hdr(evv);

	len += _odp_snprint(&str[len], n - len, "Event vector info\n");
	len += _odp_snprint(&str[len], n - len, "-----------------\n");
	len += _odp_snprint(&str[len], n - len, "  handle         0x%" PRIx64 "\n",
			    odp_event_vector_to_u64(evv));
	len += _odp_snprint(&str[len], n - len, "  size           %" PRIu32 "\n", evv_hdr->size);
	len += _odp_snprint(&str[len], n - len, "  flags          0x%" PRIx32 "\n",
			    evv_hdr->flags.all_flags);
	len += _odp_snprint(&str[len], n - len, "  user area      %p\n", evv_hdr->uarea_addr);

	for (i = 0; i < evv_hdr->size; i++) {
		odp_event_t ev = evv_hdr->event[i];
		char seg_str[max_len];
		int str_len;

		str_len = _odp_snprint(seg_str, max_len, "    event      %p  type %d\n",
				       (void *)ev, odp_event_type(ev));

		/* Prevent print buffer overflow */
		if (n - len - str_len < 10) {
			len += _odp_snprint(&str[len], n - len, "    ...\n");
			break;
		}
		len += _odp_snprint(&str[len], n - len, "%s", seg_str);
	}

	_ODP_PRINT("%s\n", str);
}

uint64_t odp_event_vector_to_u64(odp_event_vector_t pktv)
{
	return _odp_pri(pktv);
}