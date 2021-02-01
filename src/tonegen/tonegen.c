#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

#include <aoip.h>

int tonegen_ao_init(aoip_ctx_t *);
int tonegen_ao_release(aoip_ctx_t *);
int tonegen_ao_open(aoip_ctx_t *);
int tonegen_ao_close(aoip_ctx_t *);
int tonegen_ao_read(aoip_ctx_t *);
int tonegen_ao_write(aoip_ctx_t *);

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

static struct aoip_operations tonegen_ops = {
		.ao_init = tonegen_ao_init,
		.ao_release = tonegen_ao_release,
		.ao_open = tonegen_ao_open,
		.ao_close = tonegen_ao_close,
		.ao_read = tonegen_ao_read,
		.ao_write = tonegen_ao_write,
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

void *aoth_body(void *arg)
{
	aoip_ctx_t *ctx = (aoip_ctx_t *)arg;

	audio_cb_run(ctx);

	return NULL;
}

void *ntth_body(void *arg)
{
	aoip_ctx_t *ctx = (aoip_ctx_t *)arg;

	network_cb_run(ctx);

	return NULL;
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

	// init aoip device
	aoip_ctx_t ctx = {0};
	uint8_t txbuf[AOIP_PACKET_BUF_SIZE] = {0};
	uint8_t rxbuf[AOIP_PACKET_BUF_SIZE] = {0};

	ctx.ops = &tonegen_ops;
	ctx.txbuf = txbuf;
	ctx.rxbuf = rxbuf;

	if (aoip_create_context(&ctx, &tonegen_config) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return 1;
	}

	// audio and network threads
	pthread_t aoth, ntth;
	if (pthread_create(&ntth, NULL, ntth_body, &ctx)) {
		perror("network pthread create");
		return 1;
	}

	if (pthread_create(&aoth, NULL, aoth_body, &ctx)) {
		perror("audio pthread create");
		return 1;
	}

	if (ptpc_announce_msg_loop(&ctx.ptpc) < 0) {
		fprintf(stderr, "ptp_wait_announce_message: failed\n");
		return 1;
	}

	while (!caught_signal) {
		sleep(1);
	}

	network_cb_stop(&ctx);
	audio_cb_stop(&ctx);

	if (pthread_join(ntth, NULL) != 0) {
		perror("network thread join");
		return 1;
	}

	if (pthread_join(aoth, NULL) != 0) {
		perror("audio thread join");
		return 1;
	}

	aoip_context_destroy(&ctx);

	return 0;
}
