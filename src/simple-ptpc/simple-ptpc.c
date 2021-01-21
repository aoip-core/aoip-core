#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>
#include <endian.h>

#include <ptpc/ptpc.h>

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
	ssize_t count;
	int ret = -1;

	ns_gettime(&now);
	timer = now;

#define TIMEOUT_PTP_ANNOUNCE_TIMER (3 * NS_SEC)
	while(ns_sub(now, timer) < TIMEOUT_PTP_ANNOUNCE_TIMER) {
		const ptp_msg_t *msg = (ptp_msg_t *)&buf;

		// get current time
		ns_gettime(&now);

		// receive PTP general packets
		count = recvfrom(ctx->general_fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&sender, &slen);
		if (count > 0) {
			if (msg->hdr.ver == 0x2 && msg->hdr.msgtype == PTP_MSGID_ANNOUNCE) {
				if (msg->hdr.ndomain == 0 &&
				   flag_is_set(msg->hdr.flag, PTP_FLAG_TWO_STEP) &&
				   !flag_is_set(msg->hdr.flag, PTP_FLAG_UNICAST)) {
					printf("Detected a PTPv2 Announce message. ptp_server_id=%"PRIx64"\n", htobe64(msg->hdr.clockId));

					// server_addr
					ctx->server_addr.sin_addr.s_addr = sender.sin_addr.s_addr;

					// ctx->ptp_server_id
					ctx->ptp_server_id = msg->hdr.clockId;

					ret = 1;
				} else {
					fprintf(stderr, "PTPc only supports multicast , domain 0 and two step mode. flag=0x%04X\n",
							msg->hdr.flag);
				}
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

	uint8_t sendbuf[256] = {0}, recvbuf[256] = {0};
	ns_t now = 0, recv_ts = 0, timer = 0;
	ssize_t count = 0;
	int ret = 0;

	// get current time
	ns_gettime(&now);
	// set the initial timeout timer
	timer = now;

	while(!caught_signal) {
		const ptp_msg_t *msg = (ptp_msg_t *)&recvbuf;

		// receive PTP event packets
		count = recv(ctx->event_fd, recvbuf, sizeof(recvbuf), 0);
		if (count > 0) {
			//printf("ptp_event: count=%zd\n", count);

			// get the received timestamp
			ns_gettime(&recv_ts);

			if (msg->hdr.ver != 2 || msg->hdr.ndomain != 0 || msg->hdr.msgtype != PTP_MSGID_SYNC) {
				continue;
			}

			if (sync.state == S_INIT) {
				sync.seqid = msg->hdr.seqid;
				sync.t1 = recv_ts;
				sync.state = S_SYNC;
			}

			// update the timeout timer
			timer = now;
		}

		// receive PTP general packets
		count = recv(ctx->general_fd, recvbuf, sizeof(recvbuf), 0);
		if (count > 0) {
			//printf("ptp_general: count=%zd\n", count);

			// get the received timestamp
			ns_gettime(&recv_ts);

			if (msg->hdr.ver != 2 || msg->hdr.ndomain != 0) {
				continue;
			}

			// detected new ptp server?
			if (msg->hdr.clockId != ctx->ptp_server_id) {
				fprintf(stderr, "detected new ptp server.\n");
				ret = -1;
				break;
			}

			if (msg->hdr.seqid != sync.seqid) {
				ptp_sync_state_reset(&sync);
			}

			 if (sync.state == S_SYNC && msg->hdr.msgtype == PTP_MSGID_FOLLOW_UP) {
			 	sync.t2 = tstamp_to_ns((tstamp_t *)&msg->follow_up.tstamp);

			 	// send DELAY_REP
			 	build_delay_req_msg(&sync, (ptp_delay_req_t *)&sendbuf);
			 	sendto(ctx->event_fd, sendbuf, sizeof(ptp_delay_req_t), 0,
						(struct sockaddr *)&ctx->server_addr, sizeof(ctx->server_addr));
				ns_gettime(&sync.t3);
				sync.state = S_FOLLOW_UP;
			 }

			if (sync.state == S_FOLLOW_UP && msg->hdr.msgtype == PTP_MSGID_DELAY_RESP) {
				sync.t4 = tstamp_to_ns((tstamp_t *)&msg->delay_resp.tstamp);

				// offset
				ctx->ptp_offset = calc_ptp_offset(&sync);

				printf("Synced. sequence=%hu, offset=%"PRId64"\n", sync.seqid, ctx->ptp_offset);
				ptp_sync_state_reset(&sync);
			}

			// update the timeout timer
			timer = now;
		}

		// timeout PTP timer
#define TIMEOUT_PTP_TIMER (2 * NS_SEC)
		if (ns_sub(now, timer) >= TIMEOUT_PTP_TIMER) {
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

	ptpc_context_destroy(&ctx);

	return 0;
}
