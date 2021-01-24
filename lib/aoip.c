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

static int audio_device_init(aoip_ctx_t *ctx, aoip_config_t *config)
{
	int ret;

	ret = ctx->ops->ao_init(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_init: failed\n");
		ret = -1;
		goto out;
	}

out:
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


static int network_device_init(aoip_ctx_t *ctx, aoip_config_t *config)
{
	int ret;

	ret = ctx->ops->nt_init(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_init: failed\n");
		ret = -1;
		goto out;
	}

	// local_addr
	inet_pton(AF_INET, (const char *)config->local_addr, &ctx->local_addr);


	// PTP
	if (ptpc_create_context(&ctx->ptpc, &config->ptpc, ctx->local_addr) < 0) {
		fprintf(stderr, "ptpc_create_context: failed\n");
		return 1;
	}

	// SAP
	if (sap_create_context(&ctx->sap, ctx->local_addr) < 0) {
		fprintf(stderr, "sap_create_context: failed\n");
		return 1;
	}

	// RTP
	if (rtp_create_context(&ctx->rtp, ctx->local_addr) < 0) {
		fprintf(stderr, "rtp_create_context: failed\n");
		return 1;
	}

out:
	return ret;
}

static int
network_device_release(aoip_ctx_t *ctx)
{
	int ret;

	ret = ctx->ops->nt_release(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_relase: failed\n");
		ret = -1;
		goto out;
	}

	ptpc_context_destroy(&ctx->ptpc);

	sap_context_destroy(&ctx->sap);

	rtp_context_destroy(&ctx->rtp);

out:
	return ret;
}

static void aoip_core_close(aoip_ctx_t *ctx)
{
	stats_t *stats = (stats_t *)&ctx->stats;

	int i;
	for (i = 0; i < DATA_QUEUE_SLOT_NUM; i++) {
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
	int ret;

	assert(ops->ao_init);
	assert(ops->ao_release);
	assert(ops->ao_open);
	assert(ops->ao_close);

	assert(ops->nt_init);
	assert(ops->nt_release);
	assert(ops->nt_open);
	assert(ops->nt_close);

	assert((config->aoip_mode == AOIP_MODE_RECORD && ops->ao_read && ops->nt_send) ||
		(config->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_write && ops->nt_recv));

	ret = aoip_queue_init(&ctx->queue);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = audio_device_init(ctx, config);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	ret = network_device_init(ctx, config);
	if (ret < 0) {
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

	while (!ctx->network_stop_flag) {
		ret = ctx->ops->nt_recv(ctx);
		if (ret < 0) {
			fprintf(stderr, "ops->nt_recv: failed\n");
			ret = -1;
			break;
		}

		sched_yield();
	}

	return ret;
}

static int network_send_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	while (!ctx->network_stop_flag) {
		printf("network_send_loop()\n");

		ret = ctx->ops->nt_send(ctx);
		if (ret < 0) {
			fprintf(stderr, "ops->nt_send: failed\n");
			ret = -1;
			break;
		}

		sleep(1);
	}

	return ret;
}

int network_cb_run(aoip_ctx_t *ctx)
{
	struct aoip_operations *ops = ctx->ops;

	int ret;

	ret = ops->nt_open(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_open: failed\n");
		ret = -1;
		goto out;
	}

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK && ops->nt_recv) {
		ret = network_recv_loop(ctx);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else if (ctx->aoip_mode == AOIP_MODE_RECORD && ops->nt_send) {
		ret = network_send_loop(ctx);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else {
		fprintf(stderr, "network_cb_run: unknown ops->mode: %d\n", ctx->aoip_mode);
		ret = -1;
		goto out;
	}

	ret = ops->nt_close(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->nt_close: failed\n");
		ret = -1;
		goto out;
	}

out:
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

		ret = ctx->ops->ao_read(ctx);
		if (ret < 0) {
			perror("ops->ao_read");
			ret = -1;
			break;
		}

		sleep(1);
	}

	return ret;
}

static int audio_playback_loop(aoip_ctx_t *ctx)
{
	int ret = 0;

	while (!ctx->audio_stop_flag) {
		ret = ctx->ops->ao_write(ctx);
		if (ret < 0) {
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

	ret = ops->ao_open(ctx);
	if (ret < 0) {
		fprintf(stderr, "ops->ao_open: failed\n");
		ret = -1;
		goto out;
	}

	if (ctx->aoip_mode == AOIP_MODE_NONE) {
		printf("Debug message: mode=MODE_NONE\n");
	} else if (ctx->aoip_mode == AOIP_MODE_PLAYBACK && ops->ao_read) {
		ret = audio_record_loop(ctx);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else if (ctx->aoip_mode == AOIP_MODE_RECORD && ops->ao_write) {
		ret = audio_playback_loop(ctx);
		if (ret < 0) {
		    ret = -1;
		    goto out;
		}
	} else {
		fprintf(stderr, "audio_cb_run: unknown ops->mode: %d\n", ctx->aoip_mode);
		ret = -1;
		goto out;
	}

	ret = ops->ao_close(ctx);
	if (ret < 0) {
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

