#pragma once

#include <stdint.h>

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

