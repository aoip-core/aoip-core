#pragma once

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>

#include <aoip/socket.h>
#include <aoip/timer.h>

#define RTP_MULTICAST_GROUP    "239.69.179.222"
#define RTP_PORT               5004
#define RTP_HDR_SIZE    12

#define RTP_PACKET_BUF_SIZE 512

#define RTP_MCAST_TTL 15

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

//#if __BYTE_ORDER == __LITTLE_ENDIAN
//    uint8_t payload_type :7;
//    uint8_t marker :1;
//#elif __BYTE_ORDER == __BIG_ENDIAN
//    uint8_t marker :1;
//    uint8_t payload_type :7;
//#else
//# error "Please fix <bits/endian.h>"
//#endif

    uint8_t payload_type;

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
	rtp_mode_t rtp_mode;

	struct in_addr local_addr;

	struct sockaddr_in mcast_addr;

	uint16_t rtp_packet_length;

	uint16_t sequence;

	int rtp_fd;

	ns_t rtp_send_interval;

	uint8_t *txbuf;
	uint8_t *rxbuf;
} rtp_ctx_t;

static inline uint32_t ns_to_rtp_tstamp(ns_t ts, uint32_t rate)
{
	return (uint32_t)((uint64_t)floor((ts / 1000000000.0) * rate) & 0xffffffff);
}

int rtp_create_context(rtp_ctx_t *, const rtp_config_t *, struct in_addr, uint16_t, uint16_t, uint8_t *, uint8_t *);
void rtp_context_destroy(rtp_ctx_t *);
int recv_rtp_packet(rtp_ctx_t *);
void build_rtp_hdr(uint8_t *, uint8_t, uint32_t);
