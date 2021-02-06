#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

#include <aoip.h>
#include "aoip/wav.h"

struct audio_ctx {
	int fd;
	uint32_t received_frames;
};

int rtpdump_ao_init(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	int ret = 0;

	char file_name[] = "output.wav";
	if ((audio->fd = wav_open(file_name)) < 0) {
		perror("open(ao->dev.fd)");
		ret = -1;
	}

	return ret;
}

int rtpdump_ao_release(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
	audio->fd = wav_close(audio->fd);
	return 0;
}

int rtpdump_ao_open(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	struct wav_hdr wav_hdr = {0};
	build_wav_hdr(&wav_hdr);

	int ret = 0;
	if (init_wav_hdr(audio->fd, &wav_hdr) < 1) {
		perror("write(ao->dev.fd)");
		ret = -1;
	}

	return ret;
}

int rtpdump_ao_close(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
	update_wav_hdr(audio->fd, audio->received_frames);
	return 0;
}

int rtpdump_ao_read(aoip_queue_t *queue, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
	uint8_t tmp[6];

	int ret = 0;

	const queue_slot_t *slot = queue_read_ptr(queue);

	if (!queue_empty(queue)) {
//		struct rtp_hdr *rtp = (struct rtp_hdr *)&slot->data[0];
//		printf( "rtp.version=%u, rtp.payload_type=%u, "
//			"rtp.sequence=%u, rtp.timestamp=%u\n",
//				rtp->version, rtp->payload_type,
//				ntohs(rtp->sequence), ntohl(rtp->timestamp));
//
//		count = write(ao->dev.fd, (char *)slot->data + RTP_HDR_SIZE,
//				slot->len - RTP_HDR_SIZE);
//		if (count < 1) {
//			perror("write");
//			ret = -1;
//		}
//		uint8_t *list = (uint8_t *)&slot->data[0];
//		printf( "%02X %02X %02X %02X "
//			"%02X %02X %02X %02X\n",
//				list[12], list[13], list[14], list[15],
//				list[16], list[17], list[18], list[19]);

		uint8_t *rd = queue_audio_data_read_ptr(queue);
		for (uint16_t i = RTP_HDR_SIZE; i < slot->len; i+=6) {
			tmp[0] = rd[2+i];
			tmp[1] = rd[1+i];
			tmp[2] = rd[0+i];

			tmp[3] = rd[5+i];
			tmp[4] = rd[4+i];
			tmp[5] = rd[3+i];
			if (write(audio->fd, tmp, sizeof(tmp)) < 1) {
				perror("write(ao->dev.fd)");
				ret = -1;
			}
			audio->received_frames = (audio->received_frames + 1) & 0xffffffff;
		}

		asm("sfence;");
		queue_read_next(queue);
	}

	return ret;
}


static struct aoip_operations rtpdump_ops = {
		.ao_init = rtpdump_ao_init,
		.ao_release = rtpdump_ao_release,
		.ao_open = rtpdump_ao_open,
		.ao_close = rtpdump_ao_close,
		.ao_write = NULL,
		.ao_read = rtpdump_ao_read,
};

static aoip_config_t rtpdump_config = {
	.aoip_mode = AOIP_MODE_PLAYBACK,

	.audio_format = AUDIO_FORMAT_L24,
	.audio_sampling_rate = 48000,
	.audio_channels = AUDIO_CHANNEL_STEREO,
	.audio_packet_time = 1000,

	.session_name = "aoip-core v0.0.0",

	.local_addr = "10.0.1.104",

	.ptpc.ptp_mode = PTP_MODE_MULTICAST,
	.ptpc.ptp_domain = 0,

	.rtp.rtp_mode = RTP_MODE_RECV,

	.txbuf = NULL,
	.rxbuf = NULL,

	.ops = &rtpdump_ops,
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

int main(void)
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
	rtpdump_config.txbuf = txbuf;
	rtpdump_config.rxbuf = rxbuf;

	struct audio_ctx audio_arg = {0};

	if (aoip_create_context(&ctx, &rtpdump_config, &audio_arg) < 0) {
		fprintf(stderr, "register_aoip_device: failed\n");
		return 1;
	}

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

