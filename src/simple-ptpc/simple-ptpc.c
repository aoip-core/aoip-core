#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <endian.h>

#include <aoip/ptpc.h>
#include <malloc.h>

volatile sig_atomic_t caught_signal;

static uint8_t local_addr[] = "10.0.1.104";

static const ptpc_config_t config = {
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

int ptp_wait_announce_message(ptpc_ctx_t *ctx)
{
	struct sockaddr_in sender = {0};
	socklen_t slen = sizeof(sender);
	uint8_t buf[256] = {0};
	ns_t now = 0, timer;
	int ret = -1;

	ns_gettime(&now);
	timer = now;

	while(ns_sub(now, timer) < TIMEOUT_PTP_ANNOUNCE_TIMER) {
		const ptp_msg_t *msg = (ptp_msg_t *)&buf;

		// get current time
		ns_gettime(&now);

		// receive PTP general packets
		if (recvfrom(ctx->general_fd, buf, sizeof(buf), 0, (struct sockaddr *)&sender, &slen) > 0) {
			if (msg->hdr.ver != 0x2 && msg->hdr.msgtype != PTP_MSGID_ANNOUNCE) {
				 continue;
			}

			if (msg->hdr.ndomain == 0 && flag_is_set(msg->hdr.flag, PTP_FLAG_TWO_STEP) &&
				 !flag_is_set(msg->hdr.flag, PTP_FLAG_UNICAST)) {
				 printf("Detected a PTPv2 Announce message. ptp_server_id=%"PRIx64"\n", htobe64(msg->hdr.clockId));

				 // server_addr
				 ctx->server_addr.sin_addr.s_addr = sender.sin_addr.s_addr;

				 // ctx->ptp_server_id
				 ctx->ptp_server_id = msg->hdr.clockId;

				 ret = 1;
				 break;
			}
		}
		sched_yield();
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
		// timeout PTP timer
		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_PTP_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			ret = -1;
			break;
		}

		// receive PTP event packets
		if (recv_ptp_event_packet(ctx, &sync) < 0) {
			fprintf(stderr, "recv_ptp_event_packet: failed\n");
			ret = -1;
			break;
		}

		// receive PTP general packets
		if (recv_ptp_general_packet(ctx, &sync) < 0) {
			fprintf(stderr, "recv_ptp_general_packet: failed\n");
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
	if ((ctx.txbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}
	if ((ctx.rxbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}

	if (ptpc_create_context(&ctx, &config, (uint8_t *)&local_addr) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return 1;
	}

	if (ptp_wait_announce_message(&ctx) < 0) {
		fprintf(stderr, "ptp_wait_announce_message: failed\n");
		return 1;
	}

	if (ptp_sync_loop(&ctx) < 0) {
		fprintf(stderr, "ptp_sync_loop: failed\n");
		return 1;
	}

	free(ctx.rxbuf);
	free(ctx.txbuf);
	ptpc_context_destroy(&ctx);

	return 0;
}
