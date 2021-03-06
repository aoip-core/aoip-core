#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>
#include <malloc.h>

#include <aoip/rtp.h>
#include <aoip/timer.h>

volatile sig_atomic_t caught_signal;

static struct in_addr local_addr = { .s_addr = 0x6801000a }; // 10.0.1.104
static uint8_t audio_format = 24; // L24
static uint32_t audio_sampling_rate = 48000;
static uint8_t audio_channels = 2;
static uint16_t audio_packet_time = 1000;  // 1ms

static const rtp_config_t rtp_config = {
		.rtp_mode = RTP_MODE_RECV,
};

void sig_handler(int sig) {
	caught_signal = sig;
}

int set_signal(struct sigaction *sa, int sig) {
	int ret = 0;

	sa->sa_handler = sig_handler;
	if (sigaction(sig, sa, NULL) < 0) {
		ret = -1;
	}

	return ret;
}

void rtp_loop(rtp_ctx_t *ctx)
{
	const struct rtp_hdr *rtp_hdr = (struct rtp_hdr *)&ctx->rxbuf;

	while(!caught_signal) {
		if (recv_rtp_packet(ctx)) {
			printf("SSRC=%04X, Time=%u, Seq=%u\n",
					htonl(rtp_hdr->ssrc), htonl(rtp_hdr->timestamp), htons(rtp_hdr->sequence));
		}

		sched_yield();
	}
}

int
main(void)
{
	// signal
	struct sigaction sa = {0};
	caught_signal = 0;
	if (set_signal(&sa, SIGINT) < 0) {
		fprintf(stderr, "set_signal: failed\n");
		return 1;
	}

	rtp_ctx_t ctx = {0};
	uint8_t txbuf[RTP_PACKET_BUF_SIZE] = {0};
	uint8_t rxbuf[RTP_PACKET_BUF_SIZE] = {0};

	uint16_t rtp_packet_length = RTP_HDR_SIZE +
					   ((audio_format >> 3) * audio_channels * (audio_sampling_rate / audio_packet_time));
	if (rtp_create_context(&ctx, &rtp_config, local_addr, rtp_packet_length, audio_packet_time, txbuf, rxbuf) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		return 1;
	}

	rtp_loop(&ctx);

	rtp_context_destroy(&ctx);

	return 0;
}
