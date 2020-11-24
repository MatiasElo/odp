/* Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_api.h>
#include <odp_cunit_common.h>
#include <test_packet_parser.h>

#include <odp/helper/odph_api.h>

#include "lso.h"

#define MAX_NUM_IFACES  2
#define PKT_POOL_NUM    256
#define PKT_POOL_LEN    (2 * 1024)

/* Maximum number of segments test is prepared to receive per outgoing packet */
#define MAX_NUM_SEG     256

/* Payload bytes per LSO segment */
#define PAYLOAD_PER_SEG 288

/**
 * local container for pktio attributes
 */
typedef struct {
	const char *name;
	odp_pktio_t hdl;
	odp_pktout_queue_t pktout;
	odp_pktin_queue_t pktin;
} pktio_info_t;

/** Interface names used for testing */
static const char *iface_name[MAX_NUM_IFACES];

/** Test interfaces */
static pktio_info_t pktios[MAX_NUM_IFACES];
static pktio_info_t *pktio_a;
static pktio_info_t *pktio_b;
static odp_pktio_capability_t pktio_capa;

/** Number of interfaces being used (1=loopback, 2=pair) */
static int num_ifaces;

/** While testing real-world interfaces additional time may be needed for
 *  external network to enable link to pktio interface that just become up.
 */
static bool wait_for_network;

/** LSO test packet pool */
odp_pool_t lso_pool = ODP_POOL_INVALID;

static inline void wait_linkup(odp_pktio_t pktio)
{
	/* wait 1 second for link up */
	uint64_t wait_ns = (10 * ODP_TIME_MSEC_IN_NS);
	int wait_num = 100;
	int i;
	int ret = -1;

	for (i = 0; i < wait_num; i++) {
		ret = odp_pktio_link_status(pktio);
		if (ret == ODP_PKTIO_LINK_STATUS_UNKNOWN || ret == ODP_PKTIO_LINK_STATUS_UP)
			break;
		/* link is down, call status again after delay */
		odp_time_wait_ns(wait_ns);
	}
}

static int pkt_pool_create(void)
{
	odp_pool_capability_t capa;
	odp_pool_param_t params;

	if (odp_pool_capability(&capa) != 0) {
		ODPH_ERR("Pool capability failed\n");
		return -1;
	}

	if (capa.pkt.max_num && capa.pkt.max_num < PKT_POOL_NUM) {
		ODPH_ERR("Packet pool size not supported. Max %" PRIu32 "\n", capa.pkt.max_num);
		return -1;
	} else if (capa.pkt.max_len && capa.pkt.max_len < PKT_POOL_LEN) {
		ODPH_ERR("Packet length not supported.\n");
		return -1;
	} else if (capa.pkt.max_seg_len &&
		   capa.pkt.max_seg_len < PKT_POOL_LEN) {
		ODPH_ERR("Segment length not supported.\n");
		return -1;
	}

	odp_pool_param_init(&params);
	params.pkt.seg_len = PKT_POOL_LEN;
	params.pkt.len     = PKT_POOL_LEN;
	params.pkt.num     = PKT_POOL_NUM;
	params.type        = ODP_POOL_PACKET;

	lso_pool = odp_pool_create("lso_pool", &params);
	if (lso_pool == ODP_POOL_INVALID) {
		ODPH_ERR("Packet pool create failed.\n");
		return -1;
	}

	return 0;
}

static odp_pktio_t create_pktio(int iface_idx, odp_pool_t pool)
{
	odp_pktio_t pktio;
	odp_pktio_config_t config;
	odp_pktio_param_t pktio_param;
	const char *iface = iface_name[iface_idx];

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode = ODP_PKTIN_MODE_DIRECT;
	pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;

	pktio = odp_pktio_open(iface, pool, &pktio_param);
	if (pktio == ODP_PKTIO_INVALID) {
		ODPH_ERR("Failed to open %s\n", iface);
		return ODP_PKTIO_INVALID;
	}

	odp_pktio_config_init(&config);
	config.parser.layer = ODP_PROTO_LAYER_ALL;
	config.enable_lso   = 1;
	if (odp_pktio_config(pktio, &config)) {
		ODPH_ERR("Failed to configure %s\n", iface);
		return ODP_PKTIO_INVALID;
	}

	/* By default, single input and output queue is used */
	if (odp_pktin_queue_config(pktio, NULL)) {
		ODPH_ERR("Failed to config input queue for %s\n", iface);
		return ODP_PKTIO_INVALID;
	}
	if (odp_pktout_queue_config(pktio, NULL)) {
		ODPH_ERR("Failed to config output queue for %s\n", iface);
		return ODP_PKTIO_INVALID;
	}

	if (wait_for_network)
		odp_time_wait_ns(ODP_TIME_SEC_IN_NS / 4);

	return pktio;
}

static odp_packet_t create_packet(const uint8_t *data, uint32_t len)
{
	odp_packet_t pkt;

	pkt = odp_packet_alloc(lso_pool, len);
	if (pkt == ODP_PACKET_INVALID)
		return ODP_PACKET_INVALID;

	if (odp_packet_copy_from_mem(pkt, 0, len, data)) {
		ODPH_ERR("Failed to copy test packet data\n");
		odp_packet_free(pkt);
		return ODP_PACKET_INVALID;
	}

	odp_packet_l2_offset_set(pkt, 0);

	return pkt;
}

static void pktio_pkt_set_macs(odp_packet_t pkt, odp_pktio_t src, odp_pktio_t dst)
{
	uint32_t len;
	odph_ethhdr_t *eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, &len);
	int ret;

	ret = odp_pktio_mac_addr(src, &eth->src, ODP_PKTIO_MACADDR_MAXSIZE);
	CU_ASSERT(ret == ODPH_ETHADDR_LEN);
	CU_ASSERT(ret <= ODP_PKTIO_MACADDR_MAXSIZE);

	ret = odp_pktio_mac_addr(dst, &eth->dst, ODP_PKTIO_MACADDR_MAXSIZE);
	CU_ASSERT(ret == ODPH_ETHADDR_LEN);
	CU_ASSERT(ret <= ODP_PKTIO_MACADDR_MAXSIZE);
}

static int send_packets(odp_lso_profile_t lso_profile, pktio_info_t *pktio_a, pktio_info_t *pktio_b,
			const uint8_t *data, uint32_t len, uint32_t hdr_len, int use_opt)
{
	odp_packet_t pkt;
	int ret;
	odp_packet_lso_opt_t lso_opt;
	odp_packet_lso_opt_t *opt_ptr = NULL;
	int retries = 10;

	pkt = create_packet(data, len);
	if (pkt == ODP_PACKET_INVALID) {
		CU_FAIL("failed to generate test packet");
		return -1;
	}

	pktio_pkt_set_macs(pkt, pktio_a->hdl, pktio_b->hdl);
	CU_ASSERT(odp_packet_has_lso_request(pkt) == 0);

	memset(&lso_opt, 0, sizeof(odp_packet_lso_opt_t));
	lso_opt.lso_profile     = lso_profile;
	lso_opt.payload_offset  = hdr_len;
	lso_opt.max_payload_len = PAYLOAD_PER_SEG;

	if (use_opt) {
		opt_ptr = &lso_opt;
	} else {
		if (odp_packet_lso_request(pkt, &lso_opt)) {
			CU_FAIL("LSO request failed");
			return -1;
		}
	}

	CU_ASSERT(odp_packet_has_lso_request(pkt));
	CU_ASSERT(odp_packet_payload_offset(pkt) == hdr_len);

	while (retries) {
		ret = odp_pktout_send_lso(pktio_a->pktout, &pkt, 1, opt_ptr);

		CU_ASSERT_FATAL(ret < 2);

		if (ret < 0) {
			CU_FAIL("LSO send failed\n");
			odp_packet_free(pkt);
			return -1;
		}
		if (ret == 1)
			break;

		odp_time_wait_ns(10 * ODP_TIME_MSEC_IN_NS);
		retries--;
	}

	if (ret < 1) {
		CU_FAIL("LSO send timeout\n");
		odp_packet_free(pkt);
		return -1;
	}

	return 0;
}

static int recv_packets(pktio_info_t *pktio_info, uint64_t timeout_ns,
			odp_packet_t *pkt_out, int max_num)
{
	odp_packet_t pkt;
	odp_time_t wait_time, end;
	int ret;
	odp_pktin_queue_t pktin = pktio_info->pktin;
	int num = 0;

	wait_time = odp_time_local_from_ns(timeout_ns);
	end = odp_time_sum(odp_time_local(), wait_time);

	do {
		pkt = ODP_PACKET_INVALID;
		ret = odp_pktin_recv(pktin, &pkt, 1);

		CU_ASSERT_FATAL(ret < 2);
		if (ret < 0) {
			CU_FAIL("Packet receive failed\n");
			if (num)
				odp_packet_free_multi(pkt_out, num);
			return -1;
		}

		if (ret == 1) {
			CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
			pkt_out[num] = pkt;
			num++;
			if (num == max_num) {
				CU_FAIL("Too many packets received\n");
				return num;
			}
		}
	} while (odp_time_cmp(end, odp_time_local()) > 0);

	return num;
}

static int compare_data(odp_packet_t pkt, uint32_t offset, const uint8_t *data, uint32_t len)
{
	uint32_t i;
	uint8_t *u8;

	for (i = 0; i < len; i++) {
		u8 = odp_packet_offset(pkt, offset + i, NULL, NULL);
		if (*u8 != data[i])
			return i;
	}

	return -1;
}

int lso_suite_init(void)
{
	int i;

	if (getenv("ODP_WAIT_FOR_NETWORK"))
		wait_for_network = true;

	iface_name[0] = getenv("ODP_PKTIO_IF0");
	iface_name[1] = getenv("ODP_PKTIO_IF1");
	num_ifaces = 1;

	if (!iface_name[0]) {
		printf("No interfaces specified, using default \"loop\".\n");
		iface_name[0] = "loop";
	} else if (!iface_name[1]) {
		printf("Using loopback interface: %s\n", iface_name[0]);
	} else {
		num_ifaces = 2;
		printf("Using paired interfaces: %s %s\n",
		       iface_name[0], iface_name[1]);
	}

	if (pkt_pool_create() != 0) {
		ODPH_ERR("Failed to create pool\n");
		return -1;
	}

	/* Create pktios and associate input/output queues */
	for (i = 0; i < num_ifaces; ++i) {
		pktio_info_t *io;

		io = &pktios[i];
		io->name = iface_name[i];
		io->hdl  = create_pktio(i, lso_pool);
		if (io->hdl == ODP_PKTIO_INVALID) {
			ODPH_ERR("Failed to open iface: %s\n", io->name);
			return -1;
		}

		if (odp_pktout_queue(io->hdl, &io->pktout, 1) != 1) {
			ODPH_ERR("Failed to get pktout queue: %s\n", io->name);
			return -1;
		}

		if (odp_pktin_queue(io->hdl, &io->pktin, 1) != 1) {
			ODPH_ERR("Failed to get pktin queue: %s\n", io->name);
			return -1;
		}

		if (odp_pktio_start(io->hdl)) {
			ODPH_ERR("Failed to start iface: %s\n", io->name);
			return -1;
		}

		wait_linkup(io->hdl);
	}

	pktio_a = &pktios[0];
	pktio_b = &pktios[1];
	if (num_ifaces == 1)
		pktio_b = pktio_a;

	memset(&pktio_capa, 0, sizeof(odp_pktio_capability_t));
	if (odp_pktio_capability(pktio_a->hdl, &pktio_capa)) {
		ODPH_ERR("Pktio capa failed: %s\n", pktio_a->name);
		return -1;
	}

	return 0;
}

int lso_suite_term(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < num_ifaces; ++i) {
		if (odp_pktio_stop(pktios[i].hdl)) {
			ODPH_ERR("Failed to stop pktio: %s\n", pktios[i].name);
			ret = -1;
		}
		if (odp_pktio_close(pktios[i].hdl)) {
			ODPH_ERR("Failed to close pktio: %s\n", pktios[i].name);
			ret = -1;
		}
	}

	if (odp_pool_destroy(lso_pool) != 0) {
		ODPH_ERR("Failed to destroy pool\n");
		ret = -1;
	}

	if (odp_cunit_print_inactive())
		ret = -1;

	return ret;
}

static int check_lso_custom(void)
{
	if (pktio_capa.lso.max_profiles == 0 || pktio_capa.lso.max_profiles_per_pktio == 0)
		return ODP_TEST_INACTIVE;

	if (pktio_capa.lso.proto.custom == 0 || pktio_capa.lso.mod_op.add_segment_num == 0)
		return ODP_TEST_INACTIVE;

	return ODP_TEST_ACTIVE;
}

static void lso_capability(void)
{
	/* LSO not supported when max_profiles is zero */
	if (pktio_capa.lso.max_profiles == 0 || pktio_capa.lso.max_profiles_per_pktio == 0)
		return;

	CU_ASSERT(pktio_capa.lso.max_profiles >= pktio_capa.lso.max_profiles_per_pktio);
	/* At least 32 bytes of payload */
	CU_ASSERT(pktio_capa.lso.max_payload_len >= 32);
	/* At least two segments */
	CU_ASSERT(pktio_capa.lso.max_segments > 1);
	/* At least Ethernet header */
	CU_ASSERT(pktio_capa.lso.max_payload_offset >= 14);

	if (pktio_capa.lso.proto.custom)
		CU_ASSERT(pktio_capa.lso.max_num_custom > 0);
}

static void lso_create_custom_profile(void)
{
	odp_lso_profile_param_t param_0, param_1;
	odp_lso_profile_t profile_0, profile_1;

	odp_lso_profile_param_init(&param_0);
	param_0.lso_proto = ODP_LSO_PROTO_CUSTOM;
	param_0.custom.num_custom = 1;
	param_0.custom.field[0].mod_op = ODP_LSO_ADD_SEGMENT_NUM;
	param_0.custom.field[0].offset = 16;
	param_0.custom.field[0].size   = 2;

	profile_0 = odp_lso_profile_create(pktio_a->hdl, &param_0);
	CU_ASSERT_FATAL(profile_0 != ODP_LSO_PROFILE_INVALID);

	CU_ASSERT_FATAL(odp_lso_profile_destroy(profile_0) == 0);

	if (pktio_capa.lso.max_profiles < 2 || pktio_capa.lso.max_num_custom < 3)
		return;

	if (pktio_capa.lso.mod_op.add_payload_len == 0 ||
	    pktio_capa.lso.mod_op.add_payload_offset == 0)
		return;

	odp_lso_profile_param_init(&param_1);
	param_1.lso_proto = ODP_LSO_PROTO_CUSTOM;
	param_1.custom.num_custom = 3;
	param_1.custom.field[0].mod_op = ODP_LSO_ADD_PAYLOAD_LEN;
	param_1.custom.field[0].offset = 14;
	param_1.custom.field[0].size   = 2;
	param_1.custom.field[1].mod_op = ODP_LSO_ADD_SEGMENT_NUM;
	param_1.custom.field[1].offset = 16;
	param_1.custom.field[1].size   = 2;
	param_1.custom.field[2].mod_op = ODP_LSO_ADD_PAYLOAD_OFFSET;
	param_1.custom.field[2].offset = 18;
	param_1.custom.field[2].size   = 2;

	profile_0 = odp_lso_profile_create(pktio_a->hdl, &param_0);
	CU_ASSERT_FATAL(profile_0 != ODP_LSO_PROFILE_INVALID);

	profile_1 = odp_lso_profile_create(pktio_a->hdl, &param_1);
	CU_ASSERT_FATAL(profile_1 != ODP_LSO_PROFILE_INVALID);

	CU_ASSERT_FATAL(odp_lso_profile_destroy(profile_1) == 0);
	CU_ASSERT_FATAL(odp_lso_profile_destroy(profile_0) == 0);
}

static void test_lso_request_clear(odp_lso_profile_t lso_profile, const uint8_t *data,
				   uint32_t len, uint32_t hdr_len)
{
	odp_packet_t pkt;
	odp_packet_lso_opt_t lso_opt;

	memset(&lso_opt, 0, sizeof(odp_packet_lso_opt_t));
	lso_opt.lso_profile     = lso_profile;
	lso_opt.payload_offset  = hdr_len;
	lso_opt.max_payload_len = PAYLOAD_PER_SEG;

	pkt = create_packet(data, len);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(odp_packet_has_lso_request(pkt) == 0);
	CU_ASSERT(odp_packet_lso_request(pkt, &lso_opt) == 0);
	CU_ASSERT(odp_packet_has_lso_request(pkt) != 0);
	CU_ASSERT(odp_packet_payload_offset(pkt) == hdr_len);
	odp_packet_lso_request_clr(pkt);
	CU_ASSERT(odp_packet_has_lso_request(pkt) == 0);
	CU_ASSERT(odp_packet_payload_offset(pkt) == hdr_len);
	CU_ASSERT(odp_packet_payload_offset_set(pkt, ODP_PACKET_OFFSET_INVALID) == 0);
	CU_ASSERT(odp_packet_payload_offset(pkt) == ODP_PACKET_OFFSET_INVALID);

	odp_packet_free(pkt);
}

static void lso_send_custom_eth_1(void)
{
	int i, ret, num;
	odp_lso_profile_param_t param;
	odp_lso_profile_t profile;
	uint32_t offset, len, payload_len, payload_sum;
	uint16_t segnum;
	odp_packet_t pkt_out[MAX_NUM_SEG];
	/* Ethernet 14B + custom headers 8B */
	uint32_t hdr_len = 22;
	/* Offset to "segment number" field */
	uint32_t segnum_offset = 16;
	uint32_t sent_payload = sizeof(test_packet_custom_eth_1) - hdr_len;

	odp_lso_profile_param_init(&param);
	param.lso_proto = ODP_LSO_PROTO_CUSTOM;
	param.custom.num_custom = 1;
	param.custom.field[0].mod_op = ODP_LSO_ADD_SEGMENT_NUM;
	param.custom.field[0].offset = segnum_offset;
	param.custom.field[0].size   = 2;

	profile = odp_lso_profile_create(pktio_a->hdl, &param);
	CU_ASSERT_FATAL(profile != ODP_LSO_PROFILE_INVALID);

	test_lso_request_clear(profile, test_packet_custom_eth_1,
			       sizeof(test_packet_custom_eth_1), hdr_len);

	ret = send_packets(profile, pktio_a, pktio_b, test_packet_custom_eth_1,
			   sizeof(test_packet_custom_eth_1), hdr_len, 0);
	CU_ASSERT_FATAL(ret == 0);

	ODPH_DBG("\n    Sent payload length:     %u bytes\n", sent_payload);

	/* Wait 1 sec to receive all created segments. Timeout and MAX_NUM_SEG values should be
	 * large enough to ensure that we receive all created segments. */
	num = recv_packets(pktio_b, ODP_TIME_SEC_IN_NS, pkt_out, MAX_NUM_SEG);
	CU_ASSERT(num > 0);
	CU_ASSERT(num < MAX_NUM_SEG);

	offset = hdr_len;
	payload_sum = 0;
	segnum = 0xffff;
	for (i = 0; i < num; i++) {
		len = odp_packet_len(pkt_out[i]);
		payload_len = len - hdr_len;

		ret = odp_packet_copy_to_mem(pkt_out[i], segnum_offset, 2, &segnum);

		if (ret == 0) {
			segnum = odp_be_to_cpu_16(segnum);
			CU_ASSERT(segnum == i);
		} else {
			CU_FAIL("Seg num field read failed\n");
		}

		ODPH_DBG("    LSO segment[%u] payload:  %u bytes\n", segnum, payload_len);

		if (compare_data(pkt_out[i], hdr_len,
				 test_packet_custom_eth_1 + offset, payload_len) >= 0) {
			ODPH_ERR("    Payload compare failed at offset %u\n", offset);
			CU_FAIL("Payload compare failed\n");
		}

		offset      += payload_len;
		payload_sum += payload_len;
	}

	ODPH_DBG("    Received payload length: %u bytes\n", payload_sum);

	CU_ASSERT(payload_sum == sent_payload);

	if (num > 0)
		odp_packet_free_multi(pkt_out, num);

	CU_ASSERT_FATAL(odp_lso_profile_destroy(profile) == 0);
}

odp_testinfo_t lso_suite[] = {
	ODP_TEST_INFO(lso_capability),
	ODP_TEST_INFO_CONDITIONAL(lso_create_custom_profile, check_lso_custom),
	ODP_TEST_INFO_CONDITIONAL(lso_send_custom_eth_1, check_lso_custom),
	ODP_TEST_INFO_NULL
};
