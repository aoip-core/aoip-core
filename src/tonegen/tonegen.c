#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <endian.h>
#include <malloc.h>

#include <aoip.h>

volatile sig_atomic_t caught_signal;

static aoip_config_t tonegen_config = {
		.aoip_mode = AOIP_MODE_PLAYBACK,

		.audio_format = AUDIO_FORMAT_L24,
		.audio_sampling_rate = 48,
		.audio_channels = AUDIO_CHANNEL_STEREO,
		.audio_packet_time = 1,

		.session_name = "aoip-core v0.0.0",

		.local_addr = "10.0.1.104",

		.ptpc.ptp_mode = PTP_MODE_MULTICAST,
		.ptpc.ptp_domain = 0,

		.rtp.rtp_mode = RTP_MODE_SEND,
};

static struct aoip_operations myapp_ops = {
		.ao_init = myapp_ao_init,
		.ao_release = myapp_ao_release,
		.ao_open = myapp_ao_open,
		.ao_close = myapp_ao_close,
		.ao_read = myapp_ao_read,
		.ao_write = myapp_ao_write,
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
	ptpc_sync_ctx_t sync = {0};

	int ret = 0;

	ns_gettime(&sync.now);
	sync.timeout_timer = sync.now;

	while(!caught_signal) {
		ns_gettime(&sync.now);

		if (recv_ptp_announce_msg(ctx, &sync)) {
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
		if (recv_ptp_sync_msg(ctx, &sync) < 0) {
			fprintf(stderr, "recv_ptp_sync_msg: failed\n");
			ret = -1;
			break;
		}

		// receive PTP general packets
		if (recv_ptp_general_packet(ctx, &sync) < 0) {
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

	aoip_ctx_t ctx = {0};

	if ((ctx.txbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}
	if ((ctx.rxbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}

	if (aoip_create_context(&ctx, &tonegen_config) < 0) {
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
	aoip_context_destroy(&ctx);

	return 0;
}
