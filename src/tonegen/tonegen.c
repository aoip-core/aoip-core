#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

#include <aoip.h>

struct audio_ctx {
	float_t tone_period;
	float_t tone_delta;
	uint16_t seq;
	uint8_t first_data_flg;  // temporary
};

int tonegen_ao_init(aoip_ctx_t *aoip, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	memset(audio, 0, sizeof(struct audio_ctx));

	audio->tone_period = 0.0;
	audio->tone_delta = TONE_FREQ_A4 / aoip->audio_sampling_rate;
	audio->seq = 0;
	audio->first_data_flg = 1;

	return 0;
}
int tonegen_ao_release(aoip_ctx_t *aoip, void *arg)
{
	printf("%s\n", __func__);
	return 0;
}
int tonegen_ao_open(aoip_ctx_t *aoip, void *arg)
{
	printf("%s\n", __func__);
	return 0;
}
int tonegen_ao_close(aoip_ctx_t *aoip, void *arg)
{
	printf("%s\n", __func__);
	return 0;
}
int tonegen_ao_write(aoip_queue_t *queue, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	if (!queue_full(queue)) {
		queue_slot_t *slot = queue_write_ptr(queue);
		slot->len = queue->data_len;
		slot->seq = audio->seq;
		ns_gettime(&slot->tstamp);
		slot->first_data_flg = audio->first_data_flg;

		uint8_t *wr = queue_audio_data_write_ptr(queue);
		for (uint16_t i = 0; i < 288; i+=6, wr+=6) {
			float_t tonef = generate_tone_data(audio->tone_period);
			l24_t tonei = { .i32 = float_to_i32(tonef) };
			*wr     = tonei.u8[3];
			*(wr+1) = tonei.u8[2];
			*(wr+2) = tonei.u8[1];

			*(wr+3) = tonei.u8[3];
			*(wr+4) = tonei.u8[2];
			*(wr+5) = tonei.u8[1];

			audio->tone_period += audio->tone_delta;
			if (audio->tone_period >= 1.0)
				audio->tone_period = 0;
		}

		audio->seq = (audio->seq + 1) & 0xffff;
		audio->first_data_flg = 0;

		asm("sfence;");
		queue_write_next(queue);
	}

	return 0;
}

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


volatile sig_atomic_t caught_signal;

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
	aoip_ctx_t aoip = {0};

	uint8_t txbuf[AOIP_PACKET_BUF_SIZE] = {0};
	uint8_t rxbuf[AOIP_PACKET_BUF_SIZE] = {0};
	tonegen_config.txbuf = txbuf;
	tonegen_config.rxbuf = rxbuf;

	struct audio_ctx audio_arg = {0};

	if (aoip_create_context(&aoip, &tonegen_config, &audio_arg) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return 1;
	}

	// audio and network threads
	pthread_t aoth, ntth;
	if (pthread_create(&ntth, NULL, ntth_body, &aoip)) {
		perror("network pthread create");
		return 1;
	}

	if (pthread_create(&aoth, NULL, aoth_body, &aoip)) {
		perror("audio pthread create");
		return 1;
	}

	while (!caught_signal) {
		sleep(1);
	}

	network_cb_stop(&aoip);
	audio_cb_stop(&aoip);

	if (pthread_join(ntth, NULL) != 0) {
		perror("network thread join");
		return 1;
	}

	if (pthread_join(aoth, NULL) != 0) {
		perror("audio thread join");
		return 1;
	}

	aoip_context_destroy(&aoip);

	return 0;
}
