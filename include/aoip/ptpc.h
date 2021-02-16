#pragma once

#include <stdbool.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "timer.h"
#include "socket.h"
#include "ptp.h"

#define PTP_PACKET_BUF_SIZE 256

#define PTP_OFFSET_THRESHOLD (20 * NS_USEC_LL)  // 1s / 48kHz = 20.833us
#define PTP_OFFSET_INC       (4 * NS_USEC_LL)  // 4us

typedef enum {
	PTP_MODE_NONE = 0,
	PTP_MODE_MULTICAST,
	// PTP_MODE_UNICAST,
} ptp_mode_t;

typedef struct {
	// Mode needs to set multicast or unicast.
	// Currently ptpc only supports multicast mode.
	const ptp_mode_t ptp_mode;

	// PTP domain (default: 0).
	// Currently ptpc only supports domain:0.
	const uint8_t ptp_domain;
} ptpc_config_t;

typedef struct {
	struct in_addr local_addr;

	struct in_addr mcast_addr;

	struct sockaddr_in server_addr;

	uint64_t ptp_server_id;

	int64_t mean_path_delay;
	int64_t ptp_offset;

	int event_fd;

	int general_fd;

	uint8_t *txbuf;

	uint8_t *rxbuf;
} ptpc_ctx_t;

typedef struct {
	ptp_sync_state_t state;

	uint8_t synced_count;

	uint16_t seqid;

	ns_t now;
	ns_t timeout_timer;
	ns_t sap_timeout_timer;

	ns_t t1;
	ns_t t2;
	ns_t t3;
	ns_t t4;
} ptpc_sync_ctx_t;

static inline void ptp_sync_state_reset(ptpc_sync_ctx_t *sync)
{
	sync->t1 = 0;
	sync->t2 = 0;
	sync->t3 = 0;
	sync->t4 = 0;
	sync->state = S_INIT;
}

static inline int64_t ptp_mean_path_delay(ptpc_sync_ctx_t *sync)
{
	int64_t d1 = (int64_t)sync->t2 - (int64_t)sync->t3;
	int64_t d2 = (int64_t)sync->t4 - (int64_t)sync->t1;
	return (d1 + d2) / 2;
}

static inline int64_t ptp_offset(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	return sync->t2 - sync->t1 - ctx->mean_path_delay;
}

static inline ns_t ptp_time(ns_t ts, int64_t offset)
{
	return (ns_t)(ts - offset);
}


/*
static inline float_t ptp_ratio_server_freq(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	return ((float_t)(sync->t1 - ctx->first_sync_ingress_tstamp) / ((float_t)(sync->t2 - ctx->first_correct_server_tstamp));
}
*/

int ptpc_create_context(ptpc_ctx_t *, const ptpc_config_t *, struct in_addr, uint8_t *txbuf, uint8_t *rxbuf);
void ptpc_context_destroy(ptpc_ctx_t *);
void print_ptp_header(ptp_msg_t *);
void build_ptp_delay_req_msg(ptpc_sync_ctx_t *, ptp_delay_req_t *);

int ptpc_recv_sync_msg(ptpc_ctx_t *, ptpc_sync_ctx_t *);
int ptpc_recv_general_packet(ptpc_ctx_t *, ptpc_sync_ctx_t *);
int ptpc_recv_announce_msg(ptpc_ctx_t *);

