#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

#include <aoip.h>

int tonegen_ao_init(aoip_ctx_t *ctx)
{
	ctx->audio.tone_period = 0.0;
	ctx->audio.tone_delta = TONE_FREQ_A4 / ctx->audio_sampling_rate;
	ctx->audio.rtp_tstamp = 0;

	return 0;
}
int tonegen_ao_release(aoip_ctx_t *ctx)
{
	int ret = 0;
	printf("%s\n", __func__);
	return ret;
}
int tonegen_ao_open(aoip_ctx_t *ctx)
{
	int ret = 0;
	printf("%s\n", __func__);
	return ret;
}
int tonegen_ao_close(aoip_ctx_t *ctx)
{
	int ret = 0;
	printf("%s\n", __func__);
	return ret;
}
int tonegen_ao_write(aoip_ctx_t *ctx)
{
	queue_t *queue = &ctx->queue;

	int ret = 0;

	if (!queue_full(queue)) {
		queue_slot_t *slot = queue_write_ptr(queue);

		// rtp payload
		slot->len = ctx->rtp.rtp_packet_length;
		uint8_t *wr = (uint8_t *)(slot->data + sizeof(struct rtp_hdr));
		for (uint16_t i = 0; i < 288; i+=6, wr+=6) {
			float_t tonef = generate_tone_data(ctx->audio.tone_period);
			l24_t tonei = { .i32 = float_to_i32(tonef) };
			*wr     = tonei.u8[3];
			*(wr+1) = tonei.u8[2];
			*(wr+2) = tonei.u8[1];

			*(wr+3) = tonei.u8[3];
			*(wr+4) = tonei.u8[2];
			*(wr+5) = tonei.u8[1];

			ctx->audio.tone_period += ctx->audio.tone_delta;
			if (ctx->audio.tone_period >= 1.0)
				ctx->audio.tone_period = 0;
		}

		// rtp header
		struct rtp_hdr *rtp_hdr = (struct rtp_hdr *)slot->data;
		rtp_hdr->sequence = htons(ctx->rtp.sequence++);

		if (ctx->audio.rtp_tstamp == 0) {
			uint32_t current_ptp_time = ptp_time(ctx->ptpc.ptp_offset);
			ctx->audio.rtp_tstamp = (current_ptp_time * 48) & 0xffffffff;
			rtp_hdr->timestamp = htonl(ctx->audio.rtp_tstamp);
		} else {
			rtp_hdr->timestamp = htonl(ctx->audio.rtp_tstamp);
			ctx->audio.rtp_tstamp = (ctx->audio.rtp_tstamp + 48) & 0xffffffff;
		}

		queue_write_next(queue);
	}

	usleep(1000);

	return ret;
}

volatile sig_atomic_t caught_signal;

static struct aoip_operations tonegen_ops = {
		.ao_init = tonegen_ao_init,
		.ao_release = tonegen_ao_release,
		.ao_open = tonegen_ao_open,
		.ao_close = tonegen_ao_close,
		.ao_read = NULL,
		.ao_write = tonegen_ao_write,
};

static aoip_config_t tonegen_config = {
		.aoip_mode = AOIP_MODE_RECORD,

		.audio_format = AUDIO_FORMAT_L24,  // 24 bit
		.audio_sampling_rate = 48000,  // 48 kHz
		.audio_channels = AUDIO_CHANNEL_STEREO,  // 2ch
		.audio_packet_time = 1000,  // 1 ms

		.session_name = "aoip-core v0.0.0",

		.local_addr = "10.0.1.104",

		.ptpc.ptp_mode = PTP_MODE_MULTICAST,
		.ptpc.ptp_domain = 0,

		.rtp.rtp_mode = RTP_MODE_SEND,

		.txbuf = NULL,
		.rxbuf = NULL,

		.ops = &tonegen_ops,
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
	tonegen_config.txbuf = txbuf;
	tonegen_config.rxbuf = rxbuf;

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
