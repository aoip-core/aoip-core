#pragma once

#include <stdbool.h>
#include <netinet/in.h>

#include "timer.h"
#include "ptp.h"
#include "ptpc_socket.h"

typedef enum {
	PTP_MODE_NONE = 0,
	PTP_MODE_MULTICAST,
	// PTP_MODE_UNICAST,
} ptp_mode_t;

typedef struct {
	// Mode needs to set multicast or unicast.
	// Currently ptpc only supports multicast mode.
	const ptp_mode_t ptp_mode;

	// Network interface for multicast groups.
	// this parameter is used in imr_interface.
	const char local_addr[MAX_IPV4_ASCII_SIZE+1];

	// PTP domain (default: 0).
	// Currently ptpc only supports domain:0.
	const uint8_t ptp_domain;

} ptpc_config_t;

struct serverinfo {
	struct sockaddr_in addr;
	socklen_t socklen;
};

typedef struct {
	struct in_addr local_addr;

	struct in_addr mcast_addr;

	struct serverinfo serverinfo;

	uint64_t ptp_server_id;

	int event_fd;

	int general_fd;
} ptpc_ctx_t;

typedef struct {
	ptp_sync_state_t state;

	uint16_t seqid;

	ns_t t1;
	ns_t t2;
	ns_t t3;
	ns_t t4;
} ptpc_sync_ctx_t;

int ptpc_create_context(ptpc_ctx_t *, const ptpc_config_t *);
void ptpc_context_destroy(ptpc_ctx_t *);
void print_ptp_header(ptp_msg_t *);
void build_delay_req_msg(ptpc_sync_ctx_t *, ptp_delay_req_t *);
