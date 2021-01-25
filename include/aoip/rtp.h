#pragma once

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <aoip/socket.h>

#define RTP_MULTICAST_GROUP    "239.69.179.201"
#define RTP_PORT               5004
#define RTP_HDR_SIZE    12

#define PACKET_BUF_SIZE 512

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

typedef enum {
	RTP_MODE_NONE = 0,
	RTP_MODE_RECV,
	RTP_MODE_SEND,
} rtp_mode_t;

typedef struct {
	rtp_mode_t rtp_mode;
} rtp_config_t;

typedef struct {
	struct in_addr local_addr;

	struct in_addr mcast_addr;

	uint16_t rtp_packet_length;

	int rtp_fd;

	uint8_t *txbuf;

	uint8_t *rxbuf;
} rtp_ctx_t;

int rtp_create_context(rtp_ctx_t *, const rtp_config_t *, struct in_addr);
void rtp_context_destroy(rtp_ctx_t *);
int recv_rtp_packet(rtp_ctx_t *);
void build_rtp_packet(rtp_ctx_t *, struct rtp_hdr *);
