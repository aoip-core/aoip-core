#include "picotest.h"

#include <aoip.h>

static void
test_build_sap_payload(void)
{
	struct sap_msg msg = {0};

	uint8_t stream_name[] = "AOIP_CORE v0.0.0";
	struct in_addr local_addr = { .s_addr = 0x6801000a }; // 10.0.1.104
	struct in_addr rtp_mcast_addr = { .s_addr = 0xc9b345ef }; // 239.69.179.201
	uint8_t audio_format = 24; // L24
	uint32_t audio_sampling_rate = 48000;
	uint8_t audio_channels = 2;
	uint64_t ptp_server_id = 0x782351feffc11d;

	if (build_sap_msg(&msg, SAP_FLAGS_ANNOUNCE, (uint8_t *)&stream_name, local_addr, rtp_mcast_addr,
					  audio_format, audio_sampling_rate, audio_channels, ptp_server_id) < 0) {
		fprintf(stderr, "build_sap_msg: failed\n");
	}

	if (msg.len < 30) {  // min len
		fprintf(stderr, "build_sap_msg: failed\n");
	}

	ok(msg.len == 280);
}

static void
test_search_rtp_addr_from_sap_msg(void)
{
	struct sap_msg msg = {
		.data.sdp =
			 "v=0\r\n"
			 "o=- 1311738121 1311738121 IN IP4 192.168.1.1\r\n"
			 "s=Stage left I/O\r\n"
			 "c=IN IP4 239.0.0.1/32\r\n"
			 "t=0 0\r\n"
			 "m=audio 5004 RTP/AVP 96\r\n"
			 "i=Channels 1-8\r\n"
			 "a=rtpmap:96 L24/48000/8\r\n"
			 "a=recvonly\r\n"
			 "a=ptime:1\r\n"
			 "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:0\r\n"
			 "a=mediaclk:direct=963214424\r\n",
		.len = 0
	};
	msg.len = strlen(msg.data.sdp);
	printf("msg.len=%d\n", msg.len);

	struct in_addr correct_addr;
	inet_aton("239.0.0.1", &correct_addr);

	struct in_addr result_addr = {0};
	int ret = search_rtp_addr_from_sap_msg(&result_addr, &msg);

	ok(ret == 1);
	ok(msg.len == 274);
	ok(result_addr.s_addr == correct_addr.s_addr);
}

int
main(int argc, char **argv)
{
	subtest("test_build_sap_payload", test_build_sap_payload);
	subtest("test_search_rtp_addr_from_sap_msg",
			test_search_rtp_addr_from_sap_msg);

	return 0;
}

