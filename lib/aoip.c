#include <aoip.h>

static int aoip_queue_init(aoip_ctx_t *ctx, struct aoip_queue *queue)
{
	int ret;

	queue->head = 0;
	queue->tail = 0;
	queue->mask = DATA_QUEUE_SLOT_NUM - 1;  // TODO: temporary hard code
	queue->data_len = ctx->rtp.rtp_packet_length;

	/* queue_slot */
	if ((queue->slot = (queue_slot_t *)malloc(sizeof(queue_slot_t) * DATA_QUEUE_SLOT_NUM)) == NULL) {
		perror("malloc q->slot");
		return -1;
	}

	for (uint16_t i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		queue->slot[i].len = 0;
		queue->slot[i].seq = 0;
		queue->slot[i].tstamp = 0;
		queue->slot[i].data = (uint8_t *)calloc(queue->data_len, sizeof(uint8_t));
		if (queue->slot[i].data == NULL) {
			perror("calloc q->slot[i].data");
			return -1;
		}

		// rtp header
		uint8_t ptype = 98;
		uint32_t ssrc = (uint32_t)time(NULL);
		build_rtp_hdr(queue->slot[i].data, ptype, ssrc);
	}

	return 0;
}

static int audio_init(aoip_ctx_t *ctx, void *arg)
{
	int ret = ctx->ops->ao_init(ctx, arg);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_init: failed\n");
		ret = -1;
	}

	return ret;
}

static int
audio_device_release(aoip_ctx_t *ctx, void *arg)
{
	int ret = ctx->ops->ao_release(ctx, arg);
	if (ret < 0) {
		perror("ops->ao_release: failed");
	}
	return ret;
}


static int
network_init(aoip_ctx_t *ctx, aoip_config_t *config)
{
	int ret;

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
		return -1;
	} else {
		ctx->txbuf = config->txbuf;
	}

	// rxbuf
	if (config->txbuf == NULL) {
		fprintf(stderr, "rxbuf isn't allocated\n");
		return -1;
	} else {
		ctx->rxbuf = config->rxbuf;
	}

	// PTP
	if ((ret = ptpc_create_context(&ctx->ptpc, &config->ptpc, ctx->local_addr,
						 ctx->txbuf, ctx->rxbuf)) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return ret;
	}

	// SAP
	if ((ret = sap_create_context(&ctx->sap, ctx->local_addr)) < 0) {
		fprintf(stderr, "sap_create_context: failed\n");
		return ret;
	}

	// RTP
	uint16_t rtp_packet_length = RTP_HDR_SIZE +
		((ctx->audio_format >> 3) * ctx->audio_channels * (ctx->audio_sampling_rate / ctx->audio_packet_time));
	if ((ret = rtp_create_context(&ctx->rtp, &config->rtp, ctx->local_addr, rtp_packet_length,
						   ctx->audio_packet_time, ctx->txbuf, ctx->rxbuf)) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		return ret;
	}

	return 0;
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
	for (int i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
		free(ctx->queue.slot[i].data);
		ctx->queue.slot[i].data = NULL;
	}

	free(ctx->queue.slot);
	ctx->queue.slot = NULL;
}

int aoip_create_context(aoip_ctx_t *ctx, aoip_config_t *config, void *audio_arg)
{
	const struct aoip_operations *ops = ctx->ops;

	int ret;

	assert(ops->ao_init);
	assert(ops->ao_release);
	assert(ops->ao_open);
	assert(ops->ao_close);

	assert((config->aoip_mode == AOIP_MODE_RECORD && ops->ao_write) ||
		(config->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_read));

	// ctx->aoip_mode
	ctx->aoip_mode = config->aoip_mode;

	// audio_arg
	ctx->audio_arg = audio_arg;

	if ((ret = network_init(ctx, config)) < 0) {
		return ret;
	}

	if ((ret = audio_init(ctx, ctx->audio_arg)) < 0) {
		return ret;
	}

	if ((ret = aoip_queue_init(ctx, &ctx->queue)) < 0) {
		return ret;
	}

	return 0;
}

void aoip_context_destroy(aoip_ctx_t *ctx)
{
    audio_device_release(ctx, ctx->audio_arg);
	network_device_release(ctx);
	aoip_core_close(ctx);

	ctx->txbuf = NULL;
	ctx->rxbuf = NULL;
	ctx->ops = NULL;
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
			ret = 0;
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

		// success the PTP sync
		if (ctx->ptpc.ptp_offset != 0) {
			break;
		}

		// receive PTP event packets
		if ((ret = ptpc_recv_sync_msg(&ctx->ptpc, &sync)) < 0) {
			fprintf(stderr, "recv_ptp_sync_msg: failed\n");
			break;
		}

		// receive PTP general packets
		if ((ret = ptpc_recv_general_packet(&ctx->ptpc, &sync)) < 0) {
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

static int
network_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	// wait a PTPv2 announce message
	if ((ret = ptpc_announce_msg_loop(ctx)) < 0) {
		return ret;
	}

	// PTPv2 sync
	if ((ret = ptpc_sync_loop(ctx)) < 0) {
		return ret;
	}

	// send a SDP/SAP announce message
	if ((ret = aoip_build_sap_msg(ctx, SAP_FLAGS_ANNOUNCE)) < 0) {
		fprintf(stderr, "build_sap_msg: failed\n");
		return ret;
	}
	if ((ret = sap_send(&ctx->sap)) < 0) {
		perror("sendto(sap_fd)");
		return ret;
	}

	// main loop
	ptpc_sync_ctx_t sync = {0};
	sync.state = S_INIT;

	uint16_t seq = 0;
	uint32_t tstamp = 0; // Temp

	ns_gettime(&sync.now);
	sync.timeout_timer = sync.now;
	sync.sap_timeout_timer = sync.now;
	while (!ctx->network_stop_flag) {
		queue_t *queue = &ctx->queue;

		ns_gettime(&sync.now);

		// receive PTP event packets
		if ((ret = ptpc_recv_sync_msg(&ctx->ptpc, &sync)) < 0) {
			fprintf(stderr, "recv_ptp_sync_msg: failed\n");
			break;
		}

		// receive PTP general packets
		if ((ret = ptpc_recv_general_packet(&ctx->ptpc, &sync)) < 0) {
			fprintf(stderr, "recv_ptp_general_packet: failed\n");
			break;
		}

		// RTP
		if (ctx->aoip_mode == AOIP_MODE_PLAYBACK) {
			queue_slot_t *slot = queue_write_ptr(queue);
			ssize_t count;
			if ((count = recv(ctx->rtp.rtp_fd, slot->data, ctx->rtp.rtp_packet_length, 0)) > 0) {
				if (!queue_full(queue)) {
					slot->len = count;
					slot->seq = seq;
					ns_gettime(&slot->tstamp);
					seq = (seq + 1) & 0xffff;

					wmb();
					queue_write_next(queue);
				} else {
					// packet drop
					printf("queue is full: %d %d\n", queue->head, queue->tail);
				}
			}
		} else if (ctx->aoip_mode == AOIP_MODE_RECORD) {
			if (!queue_empty(queue)) {
				const queue_slot_t *slot = queue_read_ptr(queue);
				struct rtp_hdr *rtp = (struct rtp_hdr *) slot->data;

				// rtp_hdr timestamp field
				//rtp->timestamp = htonl(ns_to_rtp_tstamp(slot->tstamp, ctx->audio_sampling_rate));
				if (slot->seq == 0) {
					tstamp = ns_to_rtp_tstamp(slot->tstamp, ctx->audio_sampling_rate);
				}
				rtp->timestamp = htonl(tstamp);
				tstamp += 48;

				// rtp_hdr sequence field
				rtp->sequence = htons(slot->seq);

				// send a rtp packet
				if (sendto(ctx->rtp.rtp_fd, slot->data, slot->len, 0,
						   (struct sockaddr *) &ctx->rtp.mcast_addr, sizeof(ctx->rtp.mcast_addr)) < 0) {
					if (errno == EAGAIN) {
						continue;
					} else {
						perror("sendto(rtp_fd)");
						ret = -1;
						break;
					}
				}

				wmb();
				queue_read_next(queue);
			}
		}

		// send SAP message
		if (ns_sub(sync.now, sync.sap_timeout_timer) >= TIMEOUT_SAP_TIMER) {
			if ((ret = sap_send(&ctx->sap)) < 0) {
				perror("sendto(sap_fd)");
				break;
			}
			sync.sap_timeout_timer = sync.now;
		}

		// PTP timeout
		if (ns_sub(sync.now, sync.timeout_timer) >= TIMEOUT_PTP_TIMER) {
			fprintf(stderr, "ptp_timeout\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	// send a SDP/SAP deletion message
	if ((ret = aoip_build_sap_msg(ctx, SAP_FLAGS_DELETION)) < 0) {
		fprintf(stderr, "build_sap_msg: failed\n");
		return ret;
	}
	if ((ret = sap_send(&ctx->sap)) < 0) {
		perror("sendto(sap_fd)");
		return ret;
	}

	return ret;
}

int network_cb_run(aoip_ctx_t *ctx)
{
	int ret = 0;

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK || ctx->aoip_mode == AOIP_MODE_RECORD) {
		if (network_loop(ctx) < 0) {
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

	ns_t now;
	ns_gettime(&now);
	ns_t audio_timer = now;
	while (!ctx->audio_stop_flag) {
		ns_gettime(&now);

		if (ns_sub(now, audio_timer) >= NS_MSEC) {  // TODO
			if (ctx->ops->ao_read(&ctx->queue, ctx->audio_arg) < 0) {
				perror("ops->ao_read");
				ret = -1;
				break;
			}
			audio_timer = now;
		}

		sched_yield();
	}

	return ret;
}

static int audio_playback_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	ns_t now;
	ns_gettime(&now);
	ns_t audio_timer = now;
	while (!ctx->audio_stop_flag) {
		ns_gettime(&now);

		if (ns_sub(now, audio_timer) >= NS_MSEC) {  // TODO
			if (ctx->ops->ao_write(&ctx->queue, ctx->audio_arg) < 0) {
				fprintf(stderr, "ops->ao_write: failed");
				ret = -1;
				break;
			}
			audio_timer = now;
		}

		sched_yield();
	}

	return ret;
}

int audio_cb_run(aoip_ctx_t *ctx)
{
	struct aoip_operations *ops = ctx->ops;

	int ret;

	if ((ret = ops->ao_open(ctx, ctx->audio_arg)) < 0) {
		fprintf(stderr, "ops->ao_open: failed\n");
		return ret;
	}

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_read) {
		if ((ret = audio_record_loop(ctx)) < 0) {
			return ret;
		}
	} else if (ctx->aoip_mode == AOIP_MODE_RECORD && ops->ao_write) {
		if ((ret = audio_playback_loop(ctx)) < 0) {
			return ret;
		}
	} else {
		fprintf(stderr, "audio_cb_run: unknown ctx->aoip_mode: %d\n", ctx->aoip_mode);
		return -1;
	}

	if ((ret = ops->ao_close(ctx, ctx->audio_arg)) < 0) {
		fprintf(stderr, "ops->ao_close: failed\n");
		return ret;
	}

	return 0;
}

void audio_cb_stop(aoip_ctx_t *ctx)
{
	ctx->audio_stop_flag = 1;
}

