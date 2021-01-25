#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>
#include <malloc.h>

#include <aoip/rtp.h>
#include <aoip/timer.h>

volatile sig_atomic_t caught_signal;

static struct in_addr local_addr = { .s_addr = 0x6801000a }; // 10.0.1.104

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

	if ((ctx.txbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}
	if ((ctx.rxbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}

	if (rtp_create_context(&ctx, &rtp_config, local_addr) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		return 1;
	}

	rtp_loop(&ctx);

	free(ctx.rxbuf);
	free(ctx.txbuf);
	rtp_context_destroy(&ctx);

	return 0;
}
