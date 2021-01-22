#include <aoip/sap.h>

/* sample sap
 * v=0
 * o=- 1311738121 1311738121 IN IP4 192.168.1.1
 * s=Stage left I/O
 * c=IN IP4 239.0.0.1/32
 * t=0 0
 * m=audio 5004 RTP/AVP 96
 * i=Channels 1-8
 * a=rtpmap:96 L24/48000/8
 * a=recvonly
 * a=ptime:1
 * a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:0
 * a=mediaclk:direct=963214424
 */

#define MAX_IPV4_ASCII_SIZE    15
#define MIN_SAP_C_MSG_SIZE    27

int
search_rtp_addr_from_sap_msg(struct in_addr *addr, const struct sap_msg *msg)
{
	char ascii_addr[MAX_IPV4_ASCII_SIZE] = {};

	char *p = (char *)&msg->payload.sdp[0];
	char *end = (char *)&msg->payload.sdp[msg->len-1];

	// v=0
	if (*p != 'v' || *(p+1) != '=' || *(p+2) != '0') {
		//printf("this is not sdp\n");
		return -1;
	}
	p += 4;

	// search the words of 'c='
	uint8_t detected = 0;
	while (p++ != (end - MIN_SAP_C_MSG_SIZE)) {  // minimum length of 'c=' message
		if (*p == 'c' && *(p + 1) == '=') {
			detected = 1;
			break;
		}
	}
	if (!detected) {
		return -1;
	}
	p += 2;

	if (*(p) != 'I' || *(p+1) != 'N' || // IN
		*(p+3) != 'I' || *(p+4) != 'P' || *(p+5) != '4') {  // IP4
		return -1;
	}
	p += 7;

	for (uint8_t i = 0; *p != '/'; i++, p++) {
		ascii_addr[i] = *p;
		if (i == MAX_IPV4_ASCII_SIZE - 1) {
			return -1;
		}
	}

	return inet_pton(AF_INET, ascii_addr, addr);
}


#define SAP_PAYLOAD_TYPE  "application/sdp"

#define VER_DESC      "v=0\r\n"
#define OWNER_ID      "o=- %d %d IN IP4 %s\r\n"
#define SESSION_NAME  "s=%s\r\n"
#define CONNECT_INFO  "c=IN IP4 %s/32\r\n"
#define TIME_ACTIVE   "t=0 0\r\n"
#define MEDIA_NAME    "m=audio %d RTP/AVP 96\r\n"
//#define MEDIA_TITLE   "i=Channels 1-8\r\n"
#define MEDIA_ATTR0   "a=rtpmap:96 L24/48000/8\r\n"
#define MEDIA_ATTR1   "a=recvonly\r\n"
#define MEDIA_ATTR2   "a=ptime:1\r\n"
#define MEDIA_ATTR3   "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:0\r\n"
#define MEDIA_ATTR4   "a=mediaclk:direct=%d\r\n"

uint16_t
build_sap_payload(struct sap_payload *payload,
		const struct in_addr src_addr, const char *session_name,
		const struct in_addr rtp_addr, const uint16_t rtp_port)
{
	int ret, now;

	/* SAP message */
	payload->flags = 0x20;
	payload->authlen = 0;
	payload->msg_id_hash = 0;
	payload->origin_source = 0x10111213;
	strncpy(payload->payload_type, SAP_PAYLOAD_TYPE, 16);

	/* SDP message */
	const char *st = &payload->sdp[0];
	char *cur = &payload->sdp[0];

	// version description
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, VER_DESC);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// owner ID 
	now = time(NULL);
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, OWNER_ID, now, now,
			inet_ntoa(src_addr));
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// session name
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, SESSION_NAME, session_name);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// connect info
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, CONNECT_INFO, inet_ntoa(rtp_addr));
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// time active
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, TIME_ACTIVE);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// media name
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_NAME, rtp_port);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// media attr0
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR0);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// media attr1
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR1);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// media attr2
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR2);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// media attr3
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR3);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret; 

	// media attr4
	now = time(NULL);
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR4, now);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	return SAP_HDR_SIZE + (cur - st);

err:
	return 0;
}

int
sap_create_context(sap_ctx_t *ctx, uint8_t *local_addr)
{
	int ret = 0;

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	if (local_addr == NULL) {
		ctx->local_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, (const char *)local_addr, &ctx->local_addr);
	}

	// mcast_addr
	inet_pton(AF_INET, SAP_MULTICAST_GROUP, &ctx->mcast_addr);

	// sap_fd
	if ((ctx->sap_fd = create_udp_socket_nonblock()) < 0) {
		fprintf(stderr, "create_udp_socket_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	// sap_msg
	const char *session_name = "AOIP_CORE";
	struct in_addr maddr;
	inet_pton(AF_INET, "239.69.179.201", &maddr);
	const uint16_t mport = 5004;
	uint16_t count = build_sap_payload(&ctx->sap_msg.payload, ctx->local_addr,
							session_name, maddr, mport);
	if (count < 0) {
		fprintf(stderr, "build_sap_message: failed\n");
		ret = -1;
		goto out;
	} else {
		ctx->sap_msg.len = count;
	}

out:
	return ret;
}

void
sap_context_destroy(sap_ctx_t *ctx)
{
	close(ctx->sap_fd);
	ctx->sap_fd = -1;
}
