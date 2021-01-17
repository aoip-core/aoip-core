#include "picotest.h"

#include <aoip.h>

static void
test_build_sap_payload(void)
{
	struct sap_msg msg = {};

	struct in_addr saddr = { .s_addr = 0x11223344 };
	char sess_name[] = "HOGE";
	struct in_addr rtp_addr = { .s_addr = 0xffeeddcc };
	uint16_t rtp_port = 5004;

	msg.len = build_sap_payload(&msg.payload, saddr,
		sess_name, rtp_addr, rtp_port);
	if (msg.len < 30) {  // min len
		fprintf(stderr, "build_sap_msg: failed\n");
	}

	ok(msg.len == 279);
}

static void
test_search_rtp_addr_from_sap_msg(void)
{
	int ret;

	struct sap_msg msg = {
		.payload.sdp =
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
	msg.len = strlen(msg.payload.sdp);
	printf("msg.len=%d\n", msg.len);

	struct in_addr correct_addr;
	ret = inet_aton("239.0.0.1", &correct_addr);

	struct in_addr result_addr = {};
	ret = search_rtp_addr_from_sap_msg(&result_addr, &msg);

	ok(ret != 0);
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

