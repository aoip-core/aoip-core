#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <aoip/socket.h>

#define SAP_MULTICAST_GROUP    "239.255.255.255"
#define SAP_PORT               9875

#define TIMEOUT_SAP_TIMER (30 * NS_SEC)

#define SAP_HDR_SIZE        24

#define MAX_SDP_DESC_SIZE   64
#define MAX_SDP_MSG_SIZE    512

#define SAP_FLAGS_ANNOUNCE 0x20
#define SAP_FLAGS_DELETION 0x24

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
	struct sap_payload data;
};

typedef struct {
	struct in_addr local_addr;

	struct sockaddr_in mcast_addr;

	int sap_fd;

	struct sap_msg sap_msg;
} sap_ctx_t;

static inline ssize_t sap_send(sap_ctx_t *sap) {
	return sendto(sap->sap_fd, &sap->sap_msg.data, sap->sap_msg.len, 0,
			   (struct sockaddr *)&sap->mcast_addr, sizeof(sap->mcast_addr));
}

int search_rtp_addr_from_sap_msg(struct in_addr *, const struct sap_msg *);
int build_sap_msg(struct sap_msg *, uint8_t, uint8_t *, struct in_addr, struct in_addr,
		uint8_t, uint32_t, uint8_t, uint64_t);
int sap_create_context(sap_ctx_t *, struct in_addr);
void sap_context_destroy(sap_ctx_t *);
