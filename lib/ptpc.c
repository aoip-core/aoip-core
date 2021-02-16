#include <aoip/ptpc.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int
ptpc_create_context(ptpc_ctx_t *ctx, const ptpc_config_t *config,
						struct in_addr local_addr, uint8_t *txbuf, uint8_t *rxbuf)
{
	int ret;

	if (config->ptp_mode != PTP_MODE_MULTICAST) {
		fprintf(stderr, "currently ptpc only supports multicast mode.\n");
		return -1;
	}

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	ctx->local_addr = local_addr;

	// mcast_addr
	if (config->ptp_domain == 0) {
		inet_pton(AF_INET, ptp_multicast_addr[config->ptp_domain], &ctx->mcast_addr);
	} else {
		fprintf(stderr, "can't set ctx->mcast_addr\n");
		return -1;
	}

	// txbuf
	if (txbuf == NULL) {
		fprintf(stderr, "txbuf isn't allocated\n");
		return -1;
	} else {
		ctx->txbuf = txbuf;
	}
	// rxbuf
	if (rxbuf == NULL) {
		fprintf(stderr, "rxbuf isn't allocated\n");
		return -1;
	} else {
		ctx->rxbuf = rxbuf;
	}

	// server_addr
	ctx->server_addr.sin_family = AF_INET;
	ctx->server_addr.sin_port = htons(PTP_EVENT_PORT);

	// ptp_event_fd
	if ((ctx->event_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		return ctx->event_fd;
	}

	if ((ret = aoip_bind(ctx->event_fd, PTP_EVENT_PORT)) < 0) {
		fprintf(stderr, "aoip_bind: failed\n");
		return ret;
	}

	if ((ret = aoip_set_tos(ctx->event_fd, TOS_CLASS_CLOCK)) < 0) {
		fprintf(stderr, "aoip_set_tos: failed\n");
		return ret;
	}

	if ((ret = aoip_mcast_interface(ctx->event_fd, ctx->local_addr)) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		return ret;
	}

	if ((ret = aoip_mcast_membership(ctx->event_fd, ctx->local_addr, ctx->mcast_addr)) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
		return ret;
	}

	// ptp_general_fd
	if ((ctx->general_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		return ctx->general_fd;
	}

	if ((ret = aoip_bind(ctx->general_fd, PTP_GENERAL_PORT)) < 0) {
		fprintf(stderr, "aoip_bind: failed\n");
		return ret;
	}

	if ((ret = aoip_mcast_interface(ctx->general_fd, ctx->local_addr)) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		return ret;
	}

	if ((ret = aoip_mcast_membership(ctx->general_fd, ctx->local_addr, ctx->mcast_addr)) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
		return ret;
	}

	return ret;
}

void ptpc_context_destroy(ptpc_ctx_t *ctx)
{
	if (aoip_drop_mcast_membership(ctx->general_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
	}
	close(ctx->general_fd);
	ctx->general_fd = -1;

	if (aoip_drop_mcast_membership(ctx->event_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
	}
	close(ctx->event_fd);
	ctx->event_fd = -1;
}

void print_ptp_header(ptp_msg_t *ptp)
{
	switch (ptp->hdr.msgtype) {
		case PTP_MSGID_ANNOUNCE:
			printf("ptp_announce_msg:\n");
			break;
		case PTP_MSGID_SYNC:
			printf("ptp_sync_msg:\n");
			break;
		case PTP_MSGID_DELAY_REQ:
			printf("ptp_delay_req_msg:\n");
			break;
		case PTP_MSGID_FOLLOW_UP:
			printf("ptp_follow_up_msg:\n");
			break;
		case PTP_MSGID_DELAY_RESP:
			printf("ptp_delay_resp_msg:\n");
			break;
		default:
			break;
	}

	printf( "\ttsport: %u\n"
			"\tmsgtype: %u\n"
			"\tver: %u\n"
			"\tmsglen: %u\n"
			"\tndomain: %u\n"
			"\tflag: %hu\n"
			"\tcorrection: %ld\n"
			"\tclockId: %lx\n"
			"\tportNum: %u\n"
			"\tseqid: %u\n"
			"\tctrl: %u\n"
			"\tlogmsgintval: %d\n",
			ptp->hdr.tsport,
			ptp->hdr.msgtype,
			ptp->hdr.ver,
			(unsigned int)ptp->hdr.msglen,
			ptp->hdr.ndomain,
			ptp->hdr.flag,
			ptp->hdr.correction,
			ptp->hdr.clockId,
			(unsigned int)ptp->hdr.portNum,
			(unsigned int)ptp->hdr.seqid,
			ptp->hdr.ctrl,
			ptp->hdr.logmsgintval);
}

void build_ptp_delay_req_msg(ptpc_sync_ctx_t *ctx, ptp_delay_req_t *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->hdr.msgtype = PTP_MSGID_DELAY_REQ;
	msg->hdr.ver = 2;
	msg->hdr.seqid = ctx->seqid; //(++ctx->seqid) & 0xFFFF;
	msg->hdr.msglen = htons(sizeof(ptp_delay_req_t));
}

int ptpc_recv_announce_msg(ptpc_ctx_t *ctx)
{
	const ptp_msg_t *msg = (ptp_msg_t *)ctx->rxbuf;

	struct sockaddr_in sender = {0};
	socklen_t slen = sizeof(sender);
	int ret = 0;

	if (recvfrom(ctx->general_fd, ctx->rxbuf, PTP_PACKET_BUF_SIZE, 0, (struct sockaddr *)&sender, &slen) > 0) {
		if (msg->hdr.ver == 0x2 && msg->hdr.ndomain == 0 && msg->hdr.msgtype == PTP_MSGID_ANNOUNCE) {
			if (flag_is_set(msg->hdr.flag, PTP_FLAG_TWO_STEP) && !flag_is_set(msg->hdr.flag, PTP_FLAG_UNICAST)) {
				ctx->server_addr.sin_addr.s_addr = sender.sin_addr.s_addr;
				ctx->ptp_server_id = msg->hdr.clockId;
				ret = 1;
			}
		}
	}

	return ret;
}


int ptpc_recv_sync_msg(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	const ptp_msg_t *msg = (ptp_msg_t *)ctx->rxbuf;

	if (recv(ctx->event_fd, ctx->rxbuf, PTP_PACKET_BUF_SIZE, 0) < 0) {
		if (errno == EAGAIN) {
			return 0;
		} else {
			perror("recv(ctx->event_fd)");
			return -1;
		}
	}

	if (sync->state == S_INIT) {
		if (msg->hdr.ver == 0x2 && msg->hdr.ndomain == 0 && msg->hdr.msgtype == PTP_MSGID_SYNC) {
			ns_gettime(&sync->t2);

			sync->seqid = msg->hdr.seqid;
			sync->state = S_SYNC;
			sync->timeout_timer = sync->now;
		}
	}

	return 0;
}

int ptpc_recv_general_packet(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	const ptp_msg_t *msg = (ptp_msg_t *)ctx->rxbuf;

	int ret;

	if ((ret = recv(ctx->general_fd, ctx->rxbuf, PTP_PACKET_BUF_SIZE, 0)) < 0) {
		if (errno == EAGAIN) {
			return 0;
		} else {
			perror("recv(ctx->general_fd)");
			return ret;
		}
	}

	if (msg->hdr.ver == 0x2 && msg->hdr.ndomain == 0 && msg->hdr.msgtype != PTP_MSGID_ANNOUNCE) {
		// detected new ptp server?
		if (msg->hdr.clockId != ctx->ptp_server_id) {
			fprintf(stderr, "detected new ptp server.\n");
			return -1;
		}

		if (msg->hdr.seqid != sync->seqid) {
			ptp_sync_state_reset(sync);
			sync->timeout_timer = sync->now;
			return 0;
		}

		if (sync->state == S_SYNC && msg->hdr.msgtype == PTP_MSGID_FOLLOW_UP) {
			sync->t1 = tstamp_to_ns((tstamp_t *)&msg->follow_up.tstamp);

			// send DELAY_REP
			build_ptp_delay_req_msg(sync, (ptp_delay_req_t *)ctx->txbuf);
			if (sendto(ctx->event_fd, ctx->txbuf, sizeof(ptp_delay_req_t), 0,
				   (struct sockaddr *)&ctx->server_addr, sizeof(ctx->server_addr)) < 0) {
				if (errno != EAGAIN) {
					perror("sendto(ptp->event_fd)");
					return -1;
				}
			}
			ns_gettime(&sync->t3);

			sync->state = S_FOLLOW_UP;
			sync->timeout_timer = sync->now;
		} else if (sync->state == S_FOLLOW_UP && msg->hdr.msgtype == PTP_MSGID_DELAY_RESP) {
			sync->t4 = tstamp_to_ns((tstamp_t *)&msg->delay_resp.tstamp);

			// update meanPathDelay
			uint64_t new_mean_path_delay = ptp_mean_path_delay(sync);
			if (ctx->mean_path_delay == 0 || new_mean_path_delay <= ctx->mean_path_delay) {
				ctx->mean_path_delay = new_mean_path_delay;
				ctx->ptp_offset = ptp_offset(ctx, sync);
			}
			++sync->synced_count;

			sync->timeout_timer = sync->now;

			/*
			//printf("Synced. sequence=%hu, offset=%"PRId64"\n", sync->seqid, ctx->ptp_offset);
			ns_t hoge;
			ns_gettime(&hoge);
			printf("PTP:"
			    "now=%"PRIu64"\t"
			    "sync->t4=%"PRIu64"\t"
			    "path_delay=%"PRId64"\t"
			    "offset=%"PRId64"\t"
			    "ptp_time=%"PRIu64"\n",
		  			hoge, sync->t4, ctx->mean_path_delay, ctx->ptp_offset, ptp_time(hoge, ctx->ptp_offset));
			ptp_sync_state_reset(sync);
			*/
		}
	}

	return 0;
}


