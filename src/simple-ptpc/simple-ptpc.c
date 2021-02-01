#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <endian.h>
#include <malloc.h>

#include <aoip/ptpc.h>

volatile sig_atomic_t caught_signal;

struct in_addr local_addr = { .s_addr = 0x6801000a }; //10.0.1.104

static const ptpc_config_t ptp_config = {
		.ptp_mode = PTP_MODE_MULTICAST,
		.ptp_domain = 0,
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

int ptp_sync_loop(ptpc_ctx_t *ctx)
{
	ptpc_sync_ctx_t sync = {0};
	sync.state = S_INIT;

	int ret = 0;

	// get current time
	ns_gettime(&sync.now);
	// set the initial timeout timer
	sync.timeout_timer = sync.now;

	while(!caught_signal) {
		ns_gettime(&sync.now);

		// receive PTP event packets
		if (ptpc_recv_sync_msg(ctx, &sync) < 0) {
			fprintf(stderr, "recv_ptp_sync_msg: failed\n");
			ret = -1;
			break;
		}

		// receive PTP general packets
		if (ptpc_recv_general_packet(ctx, &sync) < 0) {
			fprintf(stderr, "recv_ptp_general_packet: failed\n");
			ret = -1;
			break;
		}

		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_PTP_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

int ptpc_announce_msg_loop(ptpc_ctx_t *ctx)
{
	ptpc_sync_ctx_t sync = {0};

	int ret = 0;

	ns_gettime(&sync.now);
	sync.timeout_timer = sync.now;

	while(!caught_signal) {
		ns_gettime(&sync.now);

		if (ptpc_recv_announce_msg(ctx, &sync)) {
			printf("Detected a PTPv2 Announce message. ptp_server_id=%"PRIx64"\n",
				   htobe64(ctx->ptp_server_id));
			break;
		}

		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_PTP_ANNOUNCE_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
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

	ptpc_ctx_t ctx = {0};
	uint8_t txbuf[PACKET_BUF_SIZE] = {0};
	ctx.txbuf = txbuf;
	uint8_t rxbuf[PACKET_BUF_SIZE] = {0};
	ctx.rxbuf = rxbuf;

	if (ptpc_create_context(&ctx, &ptp_config, local_addr) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return 1;
	}

	if (ptpc_announce_msg_loop(&ctx) < 0) {
		fprintf(stderr, "ptp_wait_announce_message: failed\n");
		return 1;
	}

	if (ptp_sync_loop(&ctx) < 0) {
		fprintf(stderr, "ptp_sync_loop: failed\n");
		return 1;
	}

	ptpc_context_destroy(&ctx);

	return 0;
}
