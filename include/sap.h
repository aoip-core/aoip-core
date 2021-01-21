#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#define SAP_MULTICAST_GROUP    "239.255.255.255"
#define SAP_PORT               9875

#define SAP_HDR_SIZE        24

#define MAX_SDP_DESC_SIZE   64
#define MAX_SDP_MSG_SIZE    512
struct sap_payload {
	// sap
	uint8_t flags;
	uint8_t authlen;
	uint16_t msg_id_hash;
	uint32_t origin_source;
	char payload_type[16];  // TODO

	// sdp
	char sdp[MAX_SDP_MSG_SIZE];
} __attribute__((packed));

struct sap_msg {
	uint16_t len;
	struct sap_payload payload;
};

typedef struct {
	struct in_addr local_addr;

	struct in_addr mcast_addr;

	int sap_fd;

	struct sap_msg sap_msg;
} sap_ctx_t;

int search_rtp_addr_from_sap_msg(struct in_addr *,
		const struct sap_msg *);
uint16_t build_sap_payload(struct sap_payload *,
		struct in_addr, const char *,
		struct in_addr, uint16_t);
int sap_create_context(sap_ctx_t *, uint8_t *);
void sap_context_destroy(sap_ctx_t *);
