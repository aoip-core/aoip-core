#include <aoip.h>

static int aoip_queue_init(aoip_ctx_t *ctx, struct aoip_queue *queue)
{
	int ret = 0;

	queue->head = 0;
	queue->tail = 0;
	queue->mask = DATA_QUEUE_SLOT_NUM - 1;  // TODO: temporary hard code

	/* queue_slot */
	if ((queue->slot = (queue_slot_t *)malloc(sizeof(queue_slot_t) * DATA_QUEUE_SLOT_NUM)) == NULL) {
		perror("malloc q->slot");
		ret = -1;
		goto err;
	}

	for (uint16_t i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		queue->slot[i].len = 0;
		queue->slot[i].data = (uint8_t *)calloc(ctx->rtp.rtp_packet_length, sizeof(uint8_t));
		if (queue->slot[i].data == NULL) {
			perror("malloc q->slot[i].data");
			ret = -1;
			goto err;
		}

		// rtp header
		uint8_t ptype = 97;
		uint32_t ssrc = (uint32_t)time(NULL);
		build_rtp_hdr(queue->slot[i].data, ptype, ssrc);
	}

err:
	return ret;
}

static int audio_init(aoip_ctx_t *ctx)
{
	int ret;

	if ((ret = ctx->ops->ao_init(ctx)) < 0) {
		fprintf(stderr, "ops->ao_init: failed\n");
		ret = -1;
	}

	return ret;
}

static int
audio_device_release(aoip_ctx_t *ctx)
{
	int ret = ctx->ops->ao_release(ctx);
	if (ret < 0) {
		perror("ops->ao_release: failed");
	}
	return ret;
}


static int network_init(aoip_ctx_t *ctx, aoip_config_t *config)
{
	int ret = 0;

	// local_addr
	inet_pton(AF_INET, (const char *)config->local_addr, &ctx->local_addr);

	// session_name
	ctx->sess_name = config->session_name;

	// audio_format
	ctx->audio_format = config->audio_format;

	// audio_sampling_rate
	ctx->audio_sampling_rate = config->audio_sampling_rate;

	// audio_channels
	ctx->audio_channels = config->audio_channels;

	// audio_packet_time
	ctx->audio_packet_time = config->audio_packet_time;

	// aoip_operations
	ctx->ops = config->ops;

	// txbuf
	if (config->txbuf == NULL) {
		fprintf(stderr, "txbuf isn't allocated\n");
		ret = -1;
		goto out;
	} else {
		ctx->txbuf = config->txbuf;
	}

	// rxbuf
	if (config->txbuf == NULL) {
		fprintf(stderr, "rxbuf isn't allocated\n");
		ret = -1;
		goto out;
	} else {
		ctx->rxbuf = config->rxbuf;
	}

	// PTP
	if (ptpc_create_context(&ctx->ptpc, &config->ptpc, ctx->local_addr,
						 ctx->txbuf, ctx->rxbuf) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		ret = -1;
		goto out;
	}

	// SAP
	if (sap_create_context(&ctx->sap, ctx->local_addr) < 0) {
		fprintf(stderr, "sap_create_context: failed\n");
		ret = -1;
		goto out;
	}

	// RTP
	uint16_t rtp_packet_length = RTP_HDR_SIZE +
		((ctx->audio_format >> 3) * ctx->audio_channels * (ctx->audio_sampling_rate / ctx->audio_packet_time));
	if (rtp_create_context(&ctx->rtp, &config->rtp, ctx->local_addr, rtp_packet_length,
						   ctx->audio_packet_time, ctx->txbuf, ctx->rxbuf) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static void
network_device_release(aoip_ctx_t *ctx)
{
	ptpc_context_destroy(&ctx->ptpc);
	sap_context_destroy(&ctx->sap);
	rtp_context_destroy(&ctx->rtp);
}

static void aoip_core_close(aoip_ctx_t *ctx)
{
	stats_t *stats = (stats_t *)&ctx->stats;

	for (int i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		free(ctx->queue.slot[i].data);
		ctx->queue.slot[i].data = NULL;
	}

	free(ctx->queue.slot);
	ctx->queue.slot = NULL;

	printf("received_frames=%u\n", stats->received_frames);
}

int aoip_create_context(aoip_ctx_t *ctx, aoip_config_t *config)
{
	const struct aoip_operations *ops = ctx->ops;

	int ret = 0;

	assert(ops->ao_init);
	assert(ops->ao_release);
	assert(ops->ao_open);
	assert(ops->ao_close);

	assert((config->aoip_mode == AOIP_MODE_RECORD && ops->ao_write) ||
		(config->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_read));

	// ctx->aoip_mode
	ctx->aoip_mode = config->aoip_mode;

	if (network_init(ctx, config) < 0) {
		ret = -1;
		goto out;
	}

	if (audio_init(ctx) < 0) {
		ret = -1;
		goto out;
	}

	if (aoip_queue_init(ctx, &ctx->queue) < 0) {
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void aoip_context_destroy(aoip_ctx_t *ctx)
{
    audio_device_release(ctx);
	network_device_release(ctx);
	aoip_core_close(ctx);

	ctx->txbuf = NULL;
	ctx->rxbuf = NULL;
	ctx->ops = NULL;
}

static int network_recv_loop(aoip_ctx_t *ctx)
{
	sap_ctx_t *sap = &ctx->sap;

	int count, ret = 0;
	ns_t ptp_timer, sap_timer;

	// timer reset
	ns_gettime(&ptp_timer);
	ns_gettime(&sap_timer);

	// send first sap :TODO
	//count = send(sap->sap_fd, (char *)&sap->sap_msg.data, sap->sap_msg.len, 0);
	//if (count < 1) {
	//	perror("send(sap.sockfd)");
	//	ret = -1;
	//}


	return ret;
}

static int
ptpc_announce_msg_loop(aoip_ctx_t *ctx)
{
	ptpc_sync_ctx_t sync = {0};
	int ret = 0;

	ns_gettime(&sync.now);
	sync.timeout_timer = sync.now;

	while (!ctx->network_stop_flag) {
		ns_gettime(&sync.now);

		if (ptpc_recv_announce_msg(&ctx->ptpc)) {
			printf("Detected a PTPv2 Announce message. ptp_server_id=%"PRIx64"\n",
				   htobe64(ctx->ptpc.ptp_server_id));
			break;
		}

		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_PTP_ANNOUNCE_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

static int
ptpc_sync_loop(aoip_ctx_t *ctx) {
	ptpc_sync_ctx_t sync = {0};
	sync.state = S_INIT;

	int ret = 0;

	ns_gettime(&sync.now);
	sync.timeout_timer = sync.now;

	while (!ctx->network_stop_flag) {
		ns_gettime(&sync.now);

		// receive PTP event packets
		if ((ret = ptpc_recv_sync_msg(&ctx->ptpc, &sync)) < 0) {
			fprintf(stderr, "recv_ptp_sync_msg: failed\n");
			break;
		}

		// receive PTP general packets
		ret = ptpc_recv_general_packet(&ctx->ptpc, &sync);
		if (ret == 1) {
			// TODO: success
			break;
		} else if (ret < 0) {
			fprintf(stderr, "recv_ptp_general_packet: failed\n");
			break;
		}

		// PTP timeout
		if ((ret = ns_sub(sync.now, sync.timeout_timer)) >= TIMEOUT_PTP_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			break;
		}

		sched_yield();
	}

	return ret;
}

//#define AOIP_PTP_SYNC_TIMER (5 * NS_SEC)

static int
network_send_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	// wait a PTPv2 announce message
	if (ptpc_announce_msg_loop(ctx) < 0) {
		ret = -1;
		goto out;
	}

	// PTPv2 sync
	if (ptpc_sync_loop(ctx) < 0) {
		ret = -1;
		goto out;
	}

	// send a SDP message
	if (build_sap_msg(&ctx->sap.sap_msg, ctx->sess_name, ctx->local_addr, ctx->rtp.mcast_addr.sin_addr,
					  ctx->audio_format, ctx->audio_sampling_rate, ctx->audio_channels,
					  ctx->ptpc.ptp_server_id) < 0) {
		fprintf(stderr, "build_sap_msg: failed\n");
		ret = -1;
		goto out;
	}
	if (sendto(ctx->sap.sap_fd, &ctx->sap.sap_msg.data, ctx->sap.sap_msg.len, 0,
			   (struct sockaddr *)&ctx->sap.mcast_addr, sizeof(ctx->sap.mcast_addr)) < 0) {
		perror("sendto(sap_fd)");
		ret = -1;
		goto out;
	}

	ns_t now;
	ns_gettime(&now);
	ns_t sap_timeout_timer = now;

	// main loop
	while (!ctx->network_stop_flag) {
		ns_gettime(&now);

		/*
		// PTP sync
		if (ptpc_sync_loop(ctx) < 0) {
			ret = -1;
			break;
		}
		*/

		// RTP send
		queue_t *queue = &ctx->queue;
		if (!queue_empty(queue)) {
			const queue_slot_t *slot = queue_read_ptr(queue);
			if (sendto(ctx->rtp.rtp_fd, slot->data, slot->len, 0,
				(struct sockaddr *)&ctx->rtp.mcast_addr, sizeof(ctx->rtp.mcast_addr)) < 0) {
				if (errno == EAGAIN) {
					continue;
				} else {
					perror("sendto(rtp_fd)");
					ret = -1;
					break;
				}
			}
			//printf("rtp_send\n");
			queue_read_next(queue);
		}

		// send SAP message
		if (ns_sub(now, sap_timeout_timer) >= TIMEOUT_SAP_TIMER) {
			printf("%"PRIu64": send sap_msg\n", now);
			if (sendto(ctx->sap.sap_fd, &ctx->sap.sap_msg.data, ctx->sap.sap_msg.len, 0,
					   (struct sockaddr *)&ctx->sap.mcast_addr, sizeof(ctx->sap.mcast_addr)) < 0) {
				perror("sendto(sap_fd)");
				ret = -1;
				break;
			}
			sap_timeout_timer = now;
		}

		sched_yield();
	}

out:
	return ret;
}

int network_cb_run(aoip_ctx_t *ctx)
{
	int ret = 0;

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK) {
		if (network_recv_loop(ctx) < 0) {
		    ret = -1;
		}
	} else if (ctx->aoip_mode == AOIP_MODE_RECORD) {
		if (network_send_loop(ctx) < 0) {
		    ret = -1;
		}
	} else {
		fprintf(stderr, "network_cb_run: unknown ops->mode: %d\n", ctx->aoip_mode);
		ret = -1;
	}

	return ret;
}

void network_cb_stop(aoip_ctx_t *ctx)
{
	ctx->network_stop_flag = 1;
}

static int audio_record_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	while (!ctx->audio_stop_flag) {
		printf("audio_record_loop()\n");

		if ((ret = ctx->ops->ao_read(ctx)) < 0) {
			perror("ops->ao_read");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

static int audio_playback_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	while (!ctx->audio_stop_flag) {
		if ((ret = ctx->ops->ao_write(ctx)) < 0) {
			fprintf(stderr, "ops->ao_write: failed");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

int audio_cb_run(aoip_ctx_t *ctx)
{
	int ret;

	struct aoip_operations *ops = ctx->ops;

	if ((ret = ops->ao_open(ctx)) < 0) {
		fprintf(stderr, "ops->ao_open: failed\n");
		ret = -1;
		goto out;
	}

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_read) {
		if ((ret = audio_record_loop(ctx)) < 0) {
		    ret = -1;
		    goto out;
		}
	} else if (ctx->aoip_mode == AOIP_MODE_RECORD && ops->ao_write) {
		if ((ret = audio_playback_loop(ctx)) < 0) {
		    ret = -1;
		    goto out;
		}
	} else {
		fprintf(stderr, "audio_cb_run: unknown ctx->aoip_mode: %d\n", ctx->aoip_mode);
		ret = -1;
		goto out;
	}

	if ((ret = ops->ao_close(ctx)) < 0) {
		fprintf(stderr, "ops->ao_close: failed\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

void audio_cb_stop(aoip_ctx_t *ctx)
{
	ctx->audio_stop_flag = 1;
}

