#include <aoip/ptpc.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>

int ptpc_create_context(ptpc_ctx_t *ctx, const ptpc_config_t *config, struct in_addr local_addr)
{
	int ret = 0;

	if (config->ptp_mode != PTP_MODE_MULTICAST) {
		fprintf(stderr, "currently ptpc only supports multicast mode.\n");
		ret = -1;
		goto out;
	}

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	ctx->local_addr = local_addr;

	// mcast_addr
	if (config->ptp_domain == 0) {
		inet_pton(AF_INET, ptp_multicast_addr[config->ptp_domain], &ctx->mcast_addr);
	} else {
		fprintf(stderr, "can't set ctx->mcast_addr\n");
		ret = -1;
		goto out;
	}

	// server_addr
	ctx->server_addr.sin_family = AF_INET;
	ctx->server_addr.sin_port = htons(PTP_EVENT_PORT);

	// ptp_event_fd
	if ((ctx->event_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_bind(ctx->event_fd, PTP_EVENT_PORT) < 0) {
		fprintf(stderr, "aoip_bind: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_interface(ctx->event_fd, ctx->local_addr) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_membership(ctx->event_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
		ret = -1;
		goto out;
	}

	// ptp_general_fd
	if ((ctx->general_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_bind(ctx->general_fd, PTP_GENERAL_PORT) < 0) {
		fprintf(stderr, "aoip_bind: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_interface(ctx->general_fd, ctx->local_addr) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_membership(ctx->general_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "aoip_mcast_membership: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void ptpc_context_destroy(ptpc_ctx_t *ctx)
{
	close(ctx->event_fd);
	ctx->event_fd = -1;

	close(ctx->general_fd);
	ctx->general_fd = -1;
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

int ptpc_recv_announce_msg(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	const ptp_msg_t *msg = (ptp_msg_t *)&ctx->rxbuf;

	struct sockaddr_in sender = {0};
	socklen_t slen = sizeof(sender);
	int ret = 0;

	if (recvfrom(ctx->general_fd, &ctx->rxbuf, PACKET_BUF_SIZE, 0,
			  (struct sockaddr *)&sender, &slen) > 0) {

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
	const ptp_msg_t *msg = (ptp_msg_t *)&ctx->rxbuf;

	int ret = 0;

	if (recv(ctx->event_fd, &ctx->rxbuf, PACKET_BUF_SIZE, 0) > 0) {
		ns_gettime(&sync->recv_ts);

		if (msg->hdr.ver == 0x2 && msg->hdr.ndomain == 0 && msg->hdr.msgtype == PTP_MSGID_SYNC) {
			if (sync->state == S_INIT) {
				sync->seqid = msg->hdr.seqid;
				sync->t1 = sync->recv_ts;
				sync->state = S_SYNC;
				sync->timeout_timer = sync->now;
				//printf("SYNC_MSG:  hdr.clockId=%04X\n", htons(msg->hdr.seqid));
			}
		}
	}

	return ret;
}

int ptpc_recv_general_packet(ptpc_ctx_t *ctx, ptpc_sync_ctx_t *sync)
{
	const ptp_msg_t *msg = (ptp_msg_t *)&ctx->rxbuf;

	int ret = 0;

	if (recv(ctx->general_fd, &ctx->rxbuf, PACKET_BUF_SIZE, 0) > 0) {
		if (msg->hdr.ver == 0x2 && msg->hdr.ndomain == 0 && msg->hdr.msgtype != PTP_MSGID_ANNOUNCE) {
			// detected new ptp server?
			if (msg->hdr.clockId != ctx->ptp_server_id) {
				fprintf(stderr, "detected new ptp server.\n");
				ret = -1;
				goto out;
			}

			//printf("mismatch: state=%d, type=%d, hdr.clockId=%04X, ctx-seqid=%04X\n",
			//	   sync->state, msg->hdr.msgtype, htons(msg->hdr.seqid), htons(sync->seqid));
			if (msg->hdr.seqid > sync->seqid) {
				ptp_sync_state_reset(sync);
				goto out;
			}

			if (sync->state == S_SYNC && msg->hdr.msgtype == PTP_MSGID_FOLLOW_UP) {
				sync->t2 = tstamp_to_ns((tstamp_t *) &msg->follow_up.tstamp);

				// send DELAY_REP
				build_ptp_delay_req_msg(sync, (ptp_delay_req_t *) &ctx->txbuf);
				sendto(ctx->event_fd, &ctx->txbuf, sizeof(ptp_delay_req_t), 0,
					   (struct sockaddr *)&ctx->server_addr, sizeof(ctx->server_addr));
				ns_gettime(&sync->t3);

				sync->state = S_FOLLOW_UP;
				sync->timeout_timer = sync->now;
			} else if (sync->state == S_FOLLOW_UP && msg->hdr.msgtype == PTP_MSGID_DELAY_RESP) {
				sync->t4 = tstamp_to_ns((tstamp_t *) &msg->delay_resp.tstamp);

				// offset
				ctx->ptp_offset = calc_ptp_offset(sync);

				printf("Synced. sequence=%hu, offset=%"PRId64"\n", sync->seqid, ctx->ptp_offset);
				ptp_sync_state_reset(sync);
				sync->timeout_timer = sync->now;
			}
		}
	}

out:
	return ret;
}

