#include <ptpc/ptpc.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int ptpc_create_context(ptpc_ctx_t *ctx, const ptpc_config_t *config)
{
	int ret = 0;

	if (config->ptp_mode != PTP_MODE_MULTICAST) {
		fprintf(stderr, "currently ptpc only supports multicast mode.\n");
		ret = -1;
		goto out;
	}

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	if (config->local_addr == NULL) {
		ctx->local_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, config->local_addr, &ctx->local_addr);
	}

	// mcast_addr
	if (config->ptp_domain == 0) {
		inet_pton(AF_INET, ptp_multicast_addr[config->ptp_domain], &ctx->mcast_addr);
	} else {
		fprintf(stderr, "can't set ctx->mcast_addr\n");
		ret = -1;
		goto out;
	}

	// serverinfo.socklen
	ctx->serverinfo.addr.sin_family = AF_INET;
	ctx->serverinfo.addr.sin_port = htons(PTP_EVENT_PORT);
	ctx->serverinfo.socklen = sizeof(ctx->serverinfo.addr);

	// ptp_event_fd
	if ((ctx->event_fd = create_udp_socket_nonblock(PTP_EVENT_PORT)) < 0) {
		fprintf(stderr, "create_udp_socket_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (join_mcast_membership(ctx->event_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "join_mcast_membership: failed\n");
		ret = -1;
		goto out;
	}

	// ptp_general_fd
	if ((ctx->general_fd = create_udp_socket_nonblock(PTP_GENERAL_PORT)) < 0) {
		fprintf(stderr, "create_udp_socket_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (join_mcast_membership(ctx->general_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "join_mcast_membership: failed\n");
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
	printf("ptp_common_hdr:\n"
			"\ttsport: %u\n"
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

	switch (ptp->hdr.msgtype) {
		case PTP_MSGID_ANNOUNCE:
			printf("ptp_announce_msg:\n"
				   "\ttstamp.seconds: %u\n",
				   "\ttstamp.nanoseconds: %u\n",
				   "\ttstamp.curUtcOffset: %u\n",
				   "\ttstamp.grandmasterPriority1: %u\n",
				   "\ttstamp.grandmasterClockQuality: %u\n",
				   "\ttstamp.grandmasterPriority2: %u\n",
				   "\ttstamp.grandmasterId: %u\n",
				   "\ttstamp.stepsRemoved: %u\n",
				   "\ttstamp.timeSource: %u\n",
				   ptp->announce.tstamp.seconds,
				   ptp->announce.tstamp.nanoseconds,
				   ptp->announce.announce.curUtcOffset,
				   ptp->announce.announce.grandmasterPriority1,
				   ptp->announce.announce.grandmasterClockQuality,
				   ptp->announce.announce.grandmasterPriority2,
				   ptp->announce.announce.grandmasterId,
				   ptp->announce.announce.stepsRemoved,
				   ptp->announce.announce.timeSource);
			break;
		case PTP_MSGID_SYNC:
			printf("\nptp_sync_msg:\n");
			break;
		case PTP_MSGID_DELAY_REQ:
			printf("\nptp_delay_req_msg:\n");
			break;
		case PTP_MSGID_FOLLOW_UP:
			printf("\nptp_follow_up_msg:\n");
			break;
		case PTP_MSGID_DELAY_RESP:
			printf("\nptp_delay_resp_msg:\n");
			break;
		default:
			break;
	}
}

void build_delay_req_msg(ptpc_sync_ctx_t *ctx, ptp_delay_req_t *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->hdr.msgtype = PTP_MSGID_DELAY_REQ;
	msg->hdr.ver = 2;
	msg->hdr.seqid = (++ctx->seqid) & 0xFFFF;
	msg->hdr.msglen = sizeof(ptp_delay_req_t);
}
