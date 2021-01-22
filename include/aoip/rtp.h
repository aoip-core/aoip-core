#pragma once

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <aoip/aoip_socket.h>

#define RTP_MULTICAST_GROUP    "239.69.179.201"
#define RTP_PORT               5004
#define RTP_HDR_SIZE    12

struct rtp_hdr {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t csrc_count :4;
    uint8_t extension :1;
    uint8_t padding :1;
    uint8_t version :2;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t version :2;
    uint8_t padding :1;
    uint8_t extension :1;
    uint8_t csrc_count :4;
#else
# error "Please fix <bits/endian.h>"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t payload_type :7;
    uint8_t marker :1;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t marker :1;
    uint8_t payload_type :7;
#else
# error "Please fix <bits/endian.h>"
#endif

    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} __attribute__((packed));

typedef struct {
	struct in_addr local_addr;

	struct in_addr mcast_addr;

	int rtp_fd;
} rtp_ctx_t;

int rtp_create_context(rtp_ctx_t *, uint8_t *);
void rtp_context_destroy(rtp_ctx_t *);
