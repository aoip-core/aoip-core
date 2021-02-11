#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "utils.h"

#define BIT(x) (1 << (x))
//#define BIT_ULL(x) (1ULL << (x))

#define PTP_MSGID_SYNC                     0x0
#define PTP_MSGID_DELAY_REQ                0x1
#define PTP_MSGID_PDELAY_REQ               0x2
#define PTP_MSGID_PDELAY_RESP              0x3
#define PTP_MSGID_FOLLOW_UP                0x8
#define PTP_MSGID_DELAY_RESP               0x9
#define PTP_MSGID_PDELAY_RESP_FOLLOW_UP    0xa
#define PTP_MSGID_ANNOUNCE                 0xb
#define PTP_MSGID_SIGNALING                0xc
#define PTP_MSGID_MANAGEMENT               0xd

#define PTP_FLAG_ALTER_MASTER      BIT(0)
#define PTP_FLAG_TWO_STEP          BIT(1)
#define PTP_FLAG_UNICAST           BIT(2)
#define PTP_FLAG_LI_61             BIT(8)
#define PTP_FLAG_LI_59             BIT(9)
#define PTP_FLAG_UTC_REASONABLE    BIT(10)
#define PTP_FLAG_TIMESCALE         BIT(11)
#define PTP_FLAG_TIME_TRACEABLE    BIT(12)
#define PTP_FLAG_FREQ_TRACEABLE    BIT(13)

static const char ptp_multicast_addr[][MAX_IPV4_ASCII_SIZE+1] = {
		"224.0.1.129",  // domain 0
		"224.0.1.130",  // domain 1
		"224.0.1.131",  // domain 2
		"224.0.1.132"   // domain 3
};

#define PTP_EVENT_PORT 319
#define PTP_GENERAL_PORT 320

#define TIMEOUT_PTP_ANNOUNCE_TIMER (3 * NS_SEC)
#define TIMEOUT_PTP_TIMER (2 * NS_SEC)

struct ptp_common_hdr {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t msgtype :4;
    uint8_t tsport  :4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t tsport  :4;
    uint8_t msgtype :4;
#else
# error "Please fix <bits/endian.h>"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t ver :4;
	uint8_t reserved1 :4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t reserved1 :4;
	uint8_t ver :4;
#else
# error "Please fix <bits/endian.h>"
#endif

    uint16_t msglen;
    uint8_t ndomain;
    uint8_t reserved2;
    uint16_t flag;
    int64_t correction;
    uint32_t reserved3;
    uint64_t clockId;  //srcportid[10];
    uint16_t portNum;
    uint16_t seqid;
    uint8_t ctrl;
    int8_t logmsgintval;
} __attribute__((packed));
typedef struct ptp_common_hdr ptp_hdr_t;

struct ptp_origin_timestamp {
	uint8_t seconds[6];
	uint32_t nanoseconds;
} __attribute__((packed));

struct ptp_announce_msg {
	int16_t curUtcOffset;
	uint8_t reserved1;
	uint8_t grandmasterPriority1;
	uint32_t grandmasterClockQuality;
	uint8_t grandmasterPriority2;
	uint64_t grandmasterId;
	uint16_t stepsRemoved;
	uint8_t timeSource;
} __attribute__((packed));

struct ptpc_ptp_announce_msg {
	struct ptp_common_hdr hdr;
	struct ptp_origin_timestamp tstamp;
	struct ptp_announce_msg announce;
} __attribute__((packed));
typedef struct ptpc_ptp_announce_msg ptp_announce_t;

struct ptpc_ptp_sync_msg {
	struct ptp_common_hdr hdr;
	struct ptp_origin_timestamp tstamp;
} __attribute__((packed));
typedef struct ptpc_ptp_sync_msg ptp_sync_t;
typedef struct ptpc_ptp_sync_msg ptp_delay_req_t;
typedef struct ptpc_ptp_sync_msg ptp_follow_up_t;

struct ptpc_ptp_delay_resp_msg {
	struct ptp_common_hdr hdr;
	struct ptp_origin_timestamp tstamp;
	uint8_t reqPortId[10];
} __attribute__((packed));
typedef struct ptpc_ptp_delay_resp_msg ptp_delay_resp_t;

union ptpc_ptp_msg {
	ptp_hdr_t hdr;
	ptp_announce_t announce;
	ptp_sync_t sync;
	ptp_delay_req_t delay_req;
	ptp_follow_up_t follow_up;
	ptp_delay_resp_t delay_resp;
} __attribute__((packed));
typedef union ptpc_ptp_msg ptp_msg_t;

typedef enum {
	S_INIT = 0,
	S_SYNC,
	S_FOLLOW_UP,

} ptp_sync_state_t;

static inline bool flag_is_set(uint16_t flg, uint16_t bit)
{
	return flg & bit ? true : false;
}
