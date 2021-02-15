#pragma once

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <math.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "aoip/utils.h"
#include "aoip/timer.h"
#include "aoip/socket.h"
#include "aoip/ptp.h"
#include "aoip/ptpc.h"
#include "aoip/queue.h"
#include "aoip/sap.h"
#include "aoip/rtp.h"

#define DATA_QUEUE_SLOT_NUM  512
#define AOIP_PACKET_BUF_SIZE 512

/*
 * structure describing AES67 context
 */
#define AOIP_MODE_NONE      0
#define AOIP_MODE_PLAYBACK  1
#define AOIP_MODE_RECORD    2

#define AUDIO_FORMAT_L16      16
#define AUDIO_FORMAT_L24      24

#define AUDIO_CHANNEL_MONO    1
#define AUDIO_CHANNEL_STEREO  2

typedef struct {
	uint8_t aoip_mode;

	uint8_t audio_format;
	uint32_t audio_sampling_rate;
	uint8_t audio_channels;
	uint16_t audio_packet_time;

	uint8_t session_name[MAX_DEVICE_NAME_SIZ];

	// Network interface for multicast groups.
	// this parameter is used in imr_interface.
	uint8_t local_addr[MAX_IPV4_ASCII_SIZE+1];

	ptpc_config_t ptpc;
	rtp_config_t rtp;

	uint8_t *txbuf;
	uint8_t *rxbuf;
	struct aoip_operations *ops;
} aoip_config_t;

struct aoip_operations;

typedef struct {
	uint8_t aoip_mode;

	struct in_addr local_addr;
	uint8_t *sess_name;

	uint8_t audio_format;
	uint32_t audio_sampling_rate;
	uint8_t audio_channels;
	uint16_t audio_packet_time;

	queue_t queue;

	ptpc_ctx_t ptpc;
	sap_ctx_t sap;
	rtp_ctx_t rtp;

	int audio_stop_flag;
	int network_stop_flag;

	uint8_t *txbuf;
	uint8_t *rxbuf;
	struct aoip_operations *ops;

	void *audio_arg;
} aoip_ctx_t;

struct aoip_operations {
	int (*ao_init)(aoip_ctx_t *ctx, void *arg);
	int (*ao_release)(aoip_ctx_t *ctx, void *arg);
	int (*ao_open)(aoip_ctx_t *ctx, void *arg);
	int (*ao_close)(aoip_ctx_t *ctx, void *arg);
	int (*ao_read)(queue_t *queue, void *arg);
	int (*ao_write)(queue_t *queue, void *arg);
};

static inline int
aoip_build_sap_msg(aoip_ctx_t *ctx, uint8_t flags)
{
	return build_sap_msg(&ctx->sap.sap_msg, flags, ctx->sess_name, ctx->local_addr,
						 ctx->rtp.mcast_addr.sin_addr, ctx->audio_format,
						 ctx->audio_sampling_rate, ctx->audio_channels,
						 ctx->ptpc.ptp_server_id);
}


#define AOIP_API    extern

AOIP_API int aoip_create_context(aoip_ctx_t *, aoip_config_t *, void *);
AOIP_API void aoip_context_destroy(aoip_ctx_t *);

AOIP_API int network_cb_run(aoip_ctx_t *);
AOIP_API void network_cb_stop(aoip_ctx_t *);

AOIP_API int audio_cb_run(aoip_ctx_t *);
AOIP_API void audio_cb_stop(aoip_ctx_t *);

