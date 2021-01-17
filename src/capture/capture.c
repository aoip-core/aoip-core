#include "myapp.h"

static struct aoip_operations myapp_ops = {
	.mode = MODE_PLAYBACK,

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
	aoip_t *aoip = (aoip_t *)arg;

	audio_cb_run(aoip);

	return NULL;
}

void *ntth_body(void *arg)
{
	aoip_t *aoip = (aoip_t *)arg;

	network_cb_run(aoip);

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
	aoip_t aoip = {0};

	// audio and network threads
	pthread_t aoth, ntth;

	ret = register_aoip_device(&aoip, &myapp_ops);
	if (ret < 0) {
		fprintf(stderr, "register_aoip_device: failed\n");
		return 1;
	}

	ret = pthread_create(&ntth, NULL, ntth_body, &aoip);
	if (ret) {
		perror("network pthread create");
		return 1;
	}

	ret = pthread_create(&aoth, NULL, aoth_body, &aoip);
	if (ret) {
		perror("audio pthread create");
		return 1;
	}

	while (!caught_signal) {
		sleep(1);
	}

	network_cb_stop(&aoip);
	audio_cb_stop(&aoip);

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

	unregister_aoip_device(&aoip);

	return 0;
}

