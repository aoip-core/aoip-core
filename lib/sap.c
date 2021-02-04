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

	char *p = (char *)&msg->data.sdp[0];
	char *end = (char *)&msg->data.sdp[msg->len-1];

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
#define MEDIA_NAME    "m=audio 5004 RTP/AVP 96\r\n"
//#define MEDIA_TITLE   "i=2 channels: Left, Right\r\n"
//#define MEDIA_TITLE   "i=Channels 1-8\r\n"
#define MEDIA_ATTR0   "a=rtpmap:97 L%d/%d/%d\r\n"
#define MEDIA_ATTR1   "a=recvonly\r\n"
#define MEDIA_ATTR2   "a=ptime:1\r\n"
#define MEDIA_ATTR3   "a=ts-refclk:ptp=IEEE1588-2008:%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X:0\r\n"
#define MEDIA_ATTR4   "a=mediaclk:direct=0\r\n"

int
build_sap_msg(struct sap_msg *msg, uint8_t *stream_name, struct in_addr local_addr,
		struct in_addr rtp_mcast_addr, uint8_t audio_format, uint32_t audio_sampling_rate,
				uint8_t audio_channels, uint64_t ptp_server_id)
{
	struct sap_payload *data = &msg->data;
	int ret, now;

	memset(msg, 0, sizeof(*msg));

	/* SAP message */
	data->flags = 0x20;
	data->authlen = 0;
	data->msg_id_hash = 0x3776;
	data->origin_source = local_addr.s_addr;
	strncpy(data->payload_type, SAP_PAYLOAD_TYPE, 16);

	/* SDP message */
	const char *st = &data->sdp[0];
	char *cur = &data->sdp[0];

	// version description
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, VER_DESC);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// owner ID 
	now = time(NULL);
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, OWNER_ID, now, now, inet_ntoa(local_addr));
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// session name
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, SESSION_NAME, stream_name);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// connect info
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, CONNECT_INFO, inet_ntoa(rtp_mcast_addr));
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
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_NAME);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

/*
	// media title
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_TITLE);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;
 */

	// media attr0
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR0, audio_format,
				audio_sampling_rate, audio_channels);
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
	const uint8_t *id = (uint8_t *)&ptp_server_id;
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR3,
				id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret; 

	// media attr4
	ret = snprintf(cur, MAX_SDP_DESC_SIZE, MEDIA_ATTR4);
	if (ret < 0) {
		perror("snprintf");
		goto err;
	}
	cur += ret;

	// sap_msg.len
	msg->len = SAP_HDR_SIZE + (cur - st);

	return 0;

err:
	return -1;
}

int
sap_create_context(sap_ctx_t *ctx, struct in_addr local_addr)
{
	int ret = 0;

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	ctx->local_addr = local_addr;

	// mcast_addr
	ctx->mcast_addr.sin_family = AF_INET;
	ctx->mcast_addr.sin_port = htons(SAP_PORT);
	inet_pton(AF_INET, SAP_MULTICAST_GROUP, &ctx->mcast_addr.sin_addr);

	// sap_fd
	if ((ctx->sap_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_interface(ctx->sap_fd, ctx->local_addr) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		ret = -1;
		goto out;
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
