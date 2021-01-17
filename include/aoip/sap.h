#ifndef _sap_h_
#define _sap_h_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>

#include <arpa/inet.h>
#include <netinet/in.h>

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


int search_rtp_addr_from_sap_msg(struct in_addr *,
		const struct sap_msg *);
uint16_t build_sap_payload(struct sap_payload *,
		const struct in_addr, const char *,
		const struct in_addr, const uint16_t);

#endif  /* _sap_h_ */

