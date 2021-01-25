#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>

#include <aoip/rtp.h>
#include <aoip/timer.h>
#include <aoip.h>

volatile sig_atomic_t caught_signal;

static uint8_t stream_name[] = "AOIP_CORE v0.0.0";
static struct in_addr local_addr = { .s_addr = 0x6801000a }; // 10.0.1.104
static struct in_addr rtp_mcast_addr = { .s_addr = 0xc9b345ef }; // 239.69.179.201
static uint8_t audio_format = 24; // L24
static uint32_t audio_sampling_rate = 48000;
static uint8_t audio_channels = 2;
static uint64_t ptp_server_id = 0x11111111ff1111;

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

/*
int rtp_loop(rtp_ctx_t *ctx)
{
	ns_t timeout_timer, now;
	int ret = 0;

	if (build_rtp_msg(&ctx->rtp_msg, (uint8_t *)&stream_name, local_addr, rtp_mcast_addr,
					  audio_format, audio_sampling_rate, audio_channels, ptp_server_id) < 0) {
		fprintf(stderr, "build_rtp_msg: failed\n");
		ret = -1;
		goto err;
	}

	ns_gettime(&now);
	timeout_timer = now;

	printf("%"PRIu64": send rtp_msg\n", now);
	if (sendto(ctx->rtp_fd, &ctx->rtp_msg, sizeof(ctx->rtp_msg), 0,
			   (struct sockaddr *)&ctx->mcast_addr, sizeof(ctx->mcast_addr)) < 0) {
		perror("send");
		ret = -1;
		goto err;
	}

	while(!caught_signal) {
		ns_gettime(&now);

		if (ns_sub(now, timeout_timer) >= TIMEOUT_RTP_TIMER) {
			printf("%"PRIu64": send rtp_msg\n", now);
			if (sendto(ctx->rtp_fd, &ctx->rtp_msg, sizeof(ctx->rtp_msg), 0,
					   (struct sockaddr *)&ctx->mcast_addr, sizeof(ctx->mcast_addr)) < 0) {
				perror("send");
				ret = -1;
				break;
			}
			timeout_timer = now;
		}

		sched_yield();
	}

	err:
	return ret;
}
*/

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

	if (rtp_create_context(&ctx, local_addr) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		return 1;
	}

	/*
	if (rtp_loop(&ctx) < 0) {
		fprintf(stderr, "rtp_sync_loop: failed\n");
		return 1;
	}
	 */

	rtp_context_destroy(&ctx);

	return 0;
}
