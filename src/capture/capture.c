#include "myapp.h"

static aoip_config_t myapp_config = {
	.aoip_mode = AOIP_MODE_RECORD,

	.audio_format = AUDIO_FORMAT_L24,
	.audio_sampling_rate = 48,
	.audio_channels = AUDIO_CHANNEL_STEREO,
	.audio_packet_time = 1,

	.session_name = "aoip-core v0.0.0",

	.local_addr = "10.0.1.104",

	.ptpc.ptp_mode = PTP_MODE_MULTICAST,
	.ptpc.ptp_domain = 0,
};

static struct aoip_operations myapp_ops = {
	.ao_init = myapp_ao_init,
	.ao_release = myapp_ao_release,
	.ao_open = myapp_ao_open,
	.ao_close = myapp_ao_close,
	.ao_read = myapp_ao_read,
	.ao_write = myapp_ao_write,

	.nt_init = myapp_nt_init,
	.nt_release = myapp_nt_release,
	.nt_open = myapp_nt_open,
	.nt_close = myapp_nt_close,
	.nt_recv = myapp_nt_recv,
	.nt_send = myapp_nt_send,
};

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
	int ret;

	// signal
	struct sigaction sa = {};
	caught_signal = 0;
	ret = set_signal(&sa, SIGINT);
	if (ret < 0) {
		fprintf(stderr, "set_signal: failed\n");
		return 1;
	}

	// init aoip device
	aoip_ctx_t ctx = {0};
	ctx.ops = &myapp_ops;

	// audio and network threads
	pthread_t aoth, ntth;

	ret = aoip_create_context(&ctx, &myapp_config);
	if (ret < 0) {
		fprintf(stderr, "register_aoip_device: failed\n");
		return 1;
	}

	ret = pthread_create(&ntth, NULL, ntth_body, &ctx);
	if (ret) {
		perror("network pthread create");
		return 1;
	}

	ret = pthread_create(&aoth, NULL, aoth_body, &ctx);
	if (ret) {
		perror("audio pthread create");
		return 1;
	}

	while (!caught_signal) {
		sleep(1);
	}

	network_cb_stop(&ctx);
	audio_cb_stop(&ctx);

	ret = pthread_join(ntth, NULL);
	if (ret != 0) {
		perror("network thread join");
		return 1;
	}
	ret = pthread_join(aoth, NULL);
	if (ret != 0) {
		perror("audio thread join");
		return 1;
	}

	aoip_context_destroy(&ctx);

	return 0;
}

