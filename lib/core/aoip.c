#include <aoip.h>

static int aoip_queue_init(struct aoip_queue *q)
{
	int i, ret = 0;

	q->head = 0;
	q->tail = 0;
	q->mask = DATA_QUEUE_SLOT_NUM - 1;

	/* queue_slot */
	q->slot = (queue_slot_t *)malloc(sizeof(queue_slot_t) * DATA_QUEUE_SLOT_NUM);
	if (q->slot == NULL) {
		perror("malloc q->slot");
		ret = -1;
		goto err;
	}

	for (i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		q->slot[i].tstamp = 0;
		q->slot[i].len = 0;
		q->slot[i].data = (uint8_t *)calloc(RTP_HDR_SIZE + DATA_QUEUE_SLOT_SIZE, sizeof(uint8_t));
		if (q->slot[i].data == NULL) {
			perror("malloc q->slot[i].data");
			ret = -1;
			goto err;
		}
	}

err:
	return ret;
}

static int audio_device_init(aoip_t *aoip)
{
	int ret;

	ret = aoip->ops->ao_init(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_init: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static int
audio_device_release(aoip_t *aoip)
{
	int ret = aoip->ops->ao_release(aoip);
	if (ret < 0) {
		perror("ops->ao_release: failed");
	}
	return ret;
}

static int udp_multicast_open(netdev_t *dev,
		struct in_addr multicast_interface, struct in_addr maddr,
		uint16_t mport)
{
	int ret;

	dev->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (dev->sockfd < 0) {
		perror("socket(dev->sockfd)");
		ret = -1;
		goto out;
	}

	dev->src_addr.sin_family = AF_INET;
	dev->src_addr.sin_port = mport;
	dev->src_addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(dev->sockfd, (struct sockaddr *)&dev->src_addr,
			sizeof(dev->src_addr));
	if (ret < 0) {
		perror("bind(dev->sockfd)");
		ret = -1;
		goto out;
	}

	// set the multicast interface
	ret = setsockopt(dev->sockfd, IPPROTO_IP, IP_MULTICAST_IF, 
	    (char *)&multicast_interface, sizeof(multicast_interface));
	if(ret < 0) {
		perror("setsockopt(IP_MULTICAST_IF)");
		ret = -1;
		goto out;
	}

	// join the multicast group
	dev->mreq.imr_interface.s_addr = multicast_interface.s_addr;
	dev->mreq.imr_multiaddr.s_addr = maddr.s_addr;
	if (setsockopt(dev->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char *)&dev->mreq, sizeof(dev->mreq)) != 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		ret = -1;
		goto out;
	}

	// non-blocking
	if (fcntl(dev->sockfd, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		ret = -1;
		goto out;
	}
out:
	return ret;
}

static int udp_multicast_client_open(netdev_t *dev,
		struct in_addr multicast_interface, struct in_addr maddr,
		uint16_t mport)
{
	int ret;


	dev->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (dev->sockfd < 0) {
		perror("socket(dev->sockfd)");
		ret = -1;
		goto out;
	}

	dev->dst_addr.sin_family = AF_INET;
	dev->dst_addr.sin_port = mport;
	dev->dst_addr.sin_addr.s_addr = maddr.s_addr;

	// set the multicast interface
	ret = setsockopt(dev->sockfd, IPPROTO_IP, IP_MULTICAST_IF, 
	    (char *)&multicast_interface, sizeof(multicast_interface));
	if(ret < 0) {
		perror("setsockopt(IP_MULTICAST_IF)");
		ret = -1;
		goto out;
	}

	ret = connect(dev->sockfd, (struct sockaddr *)&dev->dst_addr,
			sizeof(dev->dst_addr));
	if(ret < 0) {
		perror("connect");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static int network_device_init(aoip_t *aoip)
{
	int ret;

	net_config_t *config = &aoip->net.config;
	netdev_t *ptp = &aoip->net.ptp;
	netdev_t *rtp = &aoip->net.rtp;
	netdev_t *sap = &aoip->net.sap;

	ret = aoip->ops->nt_init(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_init: failed\n");
		ret = -1;
		goto out;
	}

	// ptpfd
	ret = udp_multicast_open(ptp, config->multicast_interface,
			config->ptp_multicast_addr,
			config->ptp_sync_msg_port);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = udp_multicast_open(rtp, config->multicast_interface,
			config->rtp_multicast_addr,
			config->rtp_port);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	// sapfd
	ret = udp_multicast_client_open(sap, config->multicast_interface,
			config->sap_multicast_addr, config->sap_port);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static int
network_device_release(aoip_t *aoip)
{
	netdev_t *ptp = &aoip->net.ptp;
	netdev_t *rtp = &aoip->net.rtp;
	netdev_t *sap = &aoip->net.sap;

	int ret;

	ret = aoip->ops->nt_release(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_relase: failed\n");
		ret = -1;
		goto out;
	}

	close(ptp->sockfd);
	ptp->sockfd = -1;

	close(rtp->sockfd);
	rtp->sockfd = -1;

	close(sap->sockfd);
	sap->sockfd = -1;

out:
	return ret;
}

static void aoip_core_close(aoip_t *aoip)
{
	stats_t *stats = (stats_t *)&aoip->stats;

	int i;
	for (i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		free(aoip->queue.slot[i].data);
		aoip->queue.slot[i].data = NULL;
	}

	free(aoip->queue.slot);
	aoip->queue.slot = NULL;

	printf("received_frames=%u\n", stats->received_frames);
}

static int sap_msg_init(network_t *net)
{
	struct sap_msg *sap_msg = &net->sap_msg;
	int ret;

	const struct in_addr multicast_interface = net->config.multicast_interface;
	const char *session_name = &net->config.device_name[0];
	const struct in_addr maddr = net->config.rtp_multicast_addr;
	const uint16_t mport = net->config.rtp_port;

	ret = build_sap_payload(&sap_msg->payload, multicast_interface,
			session_name, maddr, mport);
	if (ret < 0) {
		fprintf(stderr, "build_sap_message: failed\n");
		ret = -1;
		goto out;
	} else {
		sap_msg->len = (uint16_t)ret;
	}

out:
	return ret;
}

int register_aoip_device(aoip_t *aoip, struct aoip_operations *ops)
{
	int ret;

	assert(ops->ao_init);
	assert(ops->ao_release);
	assert(ops->ao_open);
	assert(ops->ao_close);

	assert(ops->nt_init);
	assert(ops->nt_release);
	assert(ops->nt_open);
	assert(ops->nt_close);

	assert((ops->mode == MODE_RECORD && ops->ao_read && ops->nt_send) ||
		(ops->mode == MODE_PLAYBACK && ops->ao_write && ops->nt_recv));

	aoip->ops = ops;

	ret = aoip_queue_init(&aoip->queue);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = audio_device_init(aoip);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = network_device_init(aoip);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = sap_msg_init(&aoip->net);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void unregister_aoip_device(aoip_t *aoip)
{
    audio_device_release(aoip);
	network_device_release(aoip);
	aoip_core_close(aoip);

	aoip->ops = NULL;
}

static int network_recv_loop(aoip_t *aoip)
{
	int count, ret = 0;

	network_t *net = &aoip->net;
	network_timer_t *timer = &net->timer;
	struct sap_msg *sap_msg = &net->sap_msg;

	// timer reset
	ns_gettime(&timer->ptp_timer);
	ns_gettime(&timer->sap_timer);

	// send first sap :TODO
	count = send(net->sap.sockfd, (char *)&sap_msg->payload,
			sap_msg->len, 0);
	if (count < 1) {
		perror("send(sap.sockfd)");
		ret = -1;
	}

	while (!aoip->network_stop_flag) {
		ret = aoip->ops->nt_recv(aoip);
		if (ret < 0) {
			fprintf(stderr, "ops->nt_recv: failed\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

static int network_send_loop(aoip_t *aoip)
{
	int ret = 0;

	while (!aoip->network_stop_flag) {
		printf("network_send_loop()\n");

		ret = aoip->ops->nt_send(aoip);
		if (ret < 0) {
			fprintf(stderr, "ops->nt_send: failed\n");
			ret = -1;
			break;
		}

		sleep(1);
	}

	return ret;
}

int network_cb_run(aoip_t *aoip)
{
	int ret;

	struct aoip_operations *ops = aoip->ops;

	ret = ops->nt_open(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_open: failed\n");
		ret = -1;
		goto out;
	}

	if (ops->mode == MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ops->mode == MODE_PLAYBACK && ops->nt_recv) {
		ret = network_recv_loop(aoip);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else if (ops->mode == MODE_RECORD && ops->nt_send) {
		ret = network_send_loop(aoip);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else {
		fprintf(stderr, "network_cb_run: unknown ops->mode: %d\n", ops->mode);
		ret = -1;
		goto out;
	}

	ret = ops->nt_close(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_close: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void network_cb_stop(aoip_t *aoip)
{
	aoip->network_stop_flag = 1;
}

static int audio_record_loop(aoip_t *aoip)
{
	int ret = 0;

	while (!aoip->audio_stop_flag) {
		printf("audio_record_loop()\n");

		ret = aoip->ops->ao_read(aoip);
		if (ret < 0) {
			perror("ops->ao_read");
			ret = -1;
			break;
		}

		sleep(1);
	}

	return ret;
}

static int audio_playback_loop(aoip_t *aoip)
{
	int ret = 0;

	while (!aoip->audio_stop_flag) {
		ret = aoip->ops->ao_write(aoip);
		if (ret < 0) {
			fprintf(stderr, "ops->ao_write: failed");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

int audio_cb_run(aoip_t *aoip)
{
	int ret;

	struct aoip_operations *ops = aoip->ops;

	ret = ops->ao_open(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_open: failed\n");
		ret = -1;
		goto out;
	}

	if (ops->mode == MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ops->mode == MODE_PLAYBACK && ops->ao_read) {
		ret = audio_record_loop(aoip);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else if (ops->mode == MODE_RECORD && ops->ao_write) {
		ret = audio_playback_loop(aoip);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else {
		fprintf(stderr, "audio_cb_run: unknown ops->mode: %d\n", ops->mode);
		ret = -1;
		goto out;
	}

	ret = ops->ao_close(aoip);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_close: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void audio_cb_stop(aoip_t *aoip)
{
	aoip->audio_stop_flag = 1;
}

