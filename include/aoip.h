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

#define BIT_RATE        3            // 2: 16bit, 3: 24bit
#define N_CHANNEL       2            // 2: stereo
#define PACKET_SAMPLES  48           // 48kHz
#define PACKET_TIME     1000         // 1ms

#define DATA_QUEUE_SLOT_SIZE  (BIT_RATE * N_CHANNEL * PACKET_SAMPLES)
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
} aoip_config_t;

typedef struct aoip_stats {
	uint32_t received_frames;
} stats_t;

typedef struct aoip_audio_dev {
	int fd;
} audio_t;


struct aoip_operations;

typedef struct {
	uint8_t aoip_mode;
	struct in_addr local_addr;

	stats_t stats;

	queue_t queue;
	uint8_t *txbuf;
	uint8_t *rxbuf;

	audio_t audio;

	ptpc_ctx_t ptpc;
	sap_ctx_t sap;
	rtp_ctx_t rtp;

	int audio_stop_flag;
	int network_stop_flag;

	struct aoip_operations *ops;
} aoip_ctx_t;

struct aoip_operations {
	int (*ao_init)(aoip_ctx_t *ctx);
	int (*ao_release)(aoip_ctx_t *ctx);
	int (*ao_open)(aoip_ctx_t *ctx);
	int (*ao_close)(aoip_ctx_t *ctx);
	int (*ao_read)(aoip_ctx_t *ctx);
	int (*ao_write)(aoip_ctx_t *ctx);

	int (*nt_recv)(aoip_ctx_t *ctx);
	int (*nt_send)(aoip_ctx_t *ctx);
};


#define AOIP_API    extern

AOIP_API int aoip_create_context(aoip_ctx_t *, aoip_config_t *);
AOIP_API void aoip_context_destroy(aoip_ctx_t *);

AOIP_API int network_cb_run(aoip_ctx_t *);
AOIP_API void network_cb_stop(aoip_ctx_t *);

AOIP_API int audio_cb_run(aoip_ctx_t *);
AOIP_API void audio_cb_stop(aoip_ctx_t *);

