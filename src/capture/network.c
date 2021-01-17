#include "myapp.h"

int myapp_nt_init(aoip_t *aoip)
{
	net_config_t *config = &aoip->net.config;

	int ret = 0;

	// device name
	snprintf(config->device_name, MAX_DEVICE_NAME_SIZ,
			"aoip-core v%d.%d.%d", 0, 0, 1);

	// mode
	config->network_mode = NETWORK_MODE_MULTICAST;

	// src_addr
	config->multicast_interface.s_addr = inet_addr(MY_ADDR);
	
	// ptp
	config->ptp_multicast_addr.s_addr = inet_addr(PTP_MULTICAST_GROUP);
	config->ptp_announce_msg_port = htons(PTP_ANNOUNCE_PORT);
	config->ptp_sync_msg_port = htons(PTP_SYNC_PORT);

	// rtp
	config->rtp_multicast_addr.s_addr = inet_addr(RTP_MULTICAST_GROUP);
	config->rtp_port = htons(RTP_PORT);

	// sap
	config->sap_multicast_addr.s_addr = inet_addr(SAP_MULTICAST_GROUP);
	config->sap_port = htons(SAP_PORT);

	return ret;
}

int myapp_nt_release(aoip_t *aoip)
{
	int ret = 0;

	return ret;
}

int myapp_nt_open(aoip_t *aoip)
{
	int ret = 0;

	return ret;
}

int myapp_nt_close(aoip_t *aoip)
{
	int ret = 0;

	return ret;
}

#define TIMEOUT_PTP_TIMER    (2 * 1000000000UL)
#define TIMEOUT_SAP_TIMER    (30 * 1000000000UL)
int myapp_nt_recv(aoip_t *aoip)
{
	queue_t *queue = &aoip->queue;
	network_t *net = &aoip->net;
	network_timer_t *timer = &net->timer;

	int ret = 0, count = 0;
	char buf[2048] = {};
	tv_t now = 0;

       	tv_gettime(&now);
	//printf("now=%lu\n", (uint64_t)now);

	// receive PTP packets
	count = recv(net->ptp.sockfd, buf, sizeof(buf), 0);
	if (count > 0) {
		//printf("received PTP packets\n");
		timer->ptp_timer = now;
	}

	// receive RTP packets
	queue_slot_t *slot = queue_write_ptr(queue);
	count = recv(net->rtp.sockfd, slot->data, 
			(RTP_HDR_SIZE + DATA_QUEUE_SLOT_SIZE), 0);
	if (count > 0) {
		if (!queue_full(queue)) {
			struct rtp_hdr *rtp = (struct rtp_hdr *)&slot->data[0];
			//printf( "rtp.version=%u, rtp.payload_type=%u, "
			//	"rtp.sequence=%u, rtp.timestamp=%u\n",
			//		rtp->version, rtp->payload_type,
			//		ntohs(rtp->sequence), ntohl(rtp->timestamp));
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
	if (tv_sub(now, timer->ptp_timer) >= TIMEOUT_PTP_TIMER) {
		printf("ptp_timeout\n");
		ret = -1;
	}

	// timeout SAP timer
	if (tv_sub(now, timer->sap_timer) >= TIMEOUT_SAP_TIMER) {
		struct sap_msg *sap_msg = &net->sap_msg;

		printf("sap_timeout\n");

		count = send(net->sap.sockfd, (char *)&sap_msg->payload,
				sap_msg->len, 0);
		if (count < 1) {
			perror("send(sap.sockfd)");
			ret = -1;
		};

		timer->sap_timer = now;
	}

	return ret;
}

int myapp_nt_send(aoip_t *aoip)
{
	tv_t now;

       	tv_gettime(&now);

	printf("myrecord_send: %" PRIu64 "\n", (uint64_t)now);

	return 0;
}

