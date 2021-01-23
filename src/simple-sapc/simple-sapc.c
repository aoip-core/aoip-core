#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <endian.h>
#include <malloc.h>

#include <aoip/sap.h>
#include <aoip/timer.h>

volatile sig_atomic_t caught_signal;

static uint8_t local_addr[] = "10.0.1.104";

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

#define TIMEOUT_SAP_TIMER (30 * NS_SEC)
int sap_loop(sap_ctx_t *ctx)
{
	ns_t timeout_timer, now;
	int ret = 0;

	ns_gettime(&now);
	timeout_timer = now;

	while(!caught_signal) {
		ns_gettime(&now);

		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_SAP_TIMER) {
			if (send_sap_msg(ctx)) {
			}
			timeout_timer = now;
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

	sap_ctx_t ctx = {0};
	if ((ctx.txbuf = (uint8_t *)calloc(PACKET_BUF_SIZE, sizeof(uint8_t))) == NULL) {
		perror("calloc");
		return 1;
	}

	if (sap_create_context(&ctx, (uint8_t *)&local_addr) < 0) {
		fprintf(stderr, "sap_create_context: failed\n");
		return 1;
	}

	if (sap_loop(&ctx) < 0) {
		fprintf(stderr, "ptp_sync_loop: failed\n");
		return 1;
	}

	free(ctx.txbuf);
	sap_context_destroy(&ctx);

	return 0;
}
