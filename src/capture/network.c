#include "myapp.h"

#define TIMEOUT_PTP_TIMER    (2 * 1000000000UL)
#define TIMEOUT_SAP_TIMER    (30 * 1000000000UL)
int myapp_nt_recv(aoip_ctx_t *ctx)
{
	queue_t *queue = &ctx->queue;

	ns_t now, ptp_timer = 0, sap_timer = 0;
	int ret = 0, count = 0;
	char buf[2048] = {};

       	ns_gettime(&now);
	//printf("now=%lu\n", (uint64_t)now);

	// receive PTP packets
	count = recv(ctx->ptpc.event_fd, buf, sizeof(buf), 0);
	if (count > 0) {
		//printf("received PTP packets\n");
		ptp_timer = now;
	}

	// receive RTP packets
	queue_slot_t *slot = queue_write_ptr(queue);
	count = recv(ctx->rtp.rtp_fd, slot->data,
			(RTP_HDR_SIZE + DATA_QUEUE_SLOT_SIZE), 0);
	if (count > 0) {
		if (!queue_full(queue)) {
			struct rtp_hdr *rtp = (struct rtp_hdr *)&slot->data[0];
			slot->tstamp = ntohl(rtp->timestamp);
			slot->len = count;
			queue_write_next(queue);
		} else {
			// packet drop
			printf("queue is full: %d %d\n",
					queue->head, queue->tail);
		}
	}

	// timeout PTP timer
	if (ns_sub(now, ptp_timer) >= TIMEOUT_PTP_TIMER) {
		printf("ptp_timeout\n");
		ret = -1;
	}

	// timeout SAP timer
	if (ns_sub(now, sap_timer) >= TIMEOUT_SAP_TIMER) {
		struct sap_msg *sap_msg = &ctx->sap.sap_msg;

		printf("sap_timeout\n");

		count = send(ctx->sap.sap_fd, (char *)&sap_msg->data,
				sap_msg->len, 0);
		if (count < 1) {
			perror("send(sap.sockfd)");
			ret = -1;
		};

		sap_timer = now;
	}

	return ret;
}

int myapp_nt_send(aoip_ctx_t *aoip)
{
	ns_t now;

    ns_gettime(&now);

	printf("myrecord_send: %" PRIu64 "\n", (uint64_t)now);

	return 0;
}

