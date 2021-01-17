#ifndef _aoip_h_
#define _aoip_h_

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

#include <timer.h>
#include <aoip/queue.h>
#include <aoip/rtp.h>
#include <aoip/sap.h>

#define BIT_RATE        3            // 2: 16bit, 3: 24bit
#define N_CHANNEL       2            // 2: stereo
#define PACKET_SAMPLES  48           // 48kHz
#define PACKET_TIME     1000         // 1ms

#define DATA_QUEUE_SLOT_SIZE  (BIT_RATE * N_CHANNEL * PACKET_SAMPLES)
#define DATA_QUEUE_SLOT_NUM  512


/*
 * structure describing AES67 context
 */
#define AUDIO_MODE_NONE    0
#define AUDIO_MODE_FILE    1

#define AUDIO_FORMAT_L16      16
#define AUDIO_FORMAT_L24      24

#define AUDIO_CHANNEL_MONO    1
#define AUDIO_CHANNEL_STEREO  2
typedef struct audio_configuration {
	char driver;
        char format;
        unsigned int sampling_rate;
        unsigned int channels;
        unsigned int packet_time;
} audio_config_t;

typedef struct audio_device {
	int fd;
} aodev_t;

typedef struct aoip_audio_dev {
	audio_config_t config;
	aodev_t dev;
} audio_t;

#define MAX_DEVICE_NAME_SIZ     32

#define NETWORK_MODE_NONE      0
#define NETWORK_MODE_MULTICAST 1
#define NETWORK_MODE_UNICAST   2

#define MY_ADDR                "10.0.1.105"  // temp

#define PTP_MULTICAST_GROUP    "224.0.1.129"
#define PTP_ANNOUNCE_PORT      320
#define PTP_SYNC_PORT          319

#define RTP_MULTICAST_GROUP    "239.69.179.201"   // temp
#define RTP_PORT               5004

#define SAP_MULTICAST_GROUP    "239.255.255.255"
#define SAP_PORT               9875
typedef struct network_configuration {
	char device_name[MAX_DEVICE_NAME_SIZ];

	uint8_t network_mode;

	struct in_addr multicast_interface;

	struct in_addr ptp_multicast_addr;
	uint16_t ptp_announce_msg_port;
	uint16_t ptp_sync_msg_port;

	struct in_addr rtp_multicast_addr;
	uint16_t rtp_port;

	struct in_addr sap_multicast_addr;
	uint16_t sap_port;
} net_config_t;

typedef struct network_device {
	int sockfd;

	struct sockaddr_in src_addr;
	struct ip_mreq mreq;

	struct sockaddr_in dst_addr;
} netdev_t;

typedef struct network_timer {
	ns_t ptp_timer;
	ns_t sap_timer;
} network_timer_t;

typedef struct aoip_stats {
	uint32_t received_frames;
} stats_t;

#define MAX_SAP_MSG_SIZE    512
typedef struct aoip_network_dev {
	net_config_t config;
	network_timer_t timer;

	netdev_t ptp;
	netdev_t rtp;
	netdev_t sap;

	struct sap_msg sap_msg;
} network_t;

typedef enum {
	STATE_START = 0,
	DEVICE_INIT,
	PTP_LISTEN,
	PTP_SYNC,
	DISCOVERY,
	MAIN_LOOP,
	STATE_END
} state_t;

struct aoip_operations;

typedef struct aoip_dev {
	stats_t stats;

	state_t state;

	ns_t network_clock;

	queue_t queue;

	audio_t audio;
	network_t net;

	int audio_stop_flag;
	int network_stop_flag;
	struct aoip_operations *ops;
} aoip_t;

#define MODE_NONE      0
#define MODE_PLAYBACK  1
#define MODE_RECORD    2
struct aoip_operations {
	uint8_t mode;

	int (*ao_init)(aoip_t *aoip);
	int (*ao_release)(aoip_t *aoip);
	int (*ao_open)(aoip_t *aoip);
	int (*ao_close)(aoip_t *aoip);
	int (*ao_read)(aoip_t *aoip);
	int (*ao_write)(aoip_t *aoip);

	int (*nt_init)(aoip_t *aoip);
	int (*nt_release)(aoip_t *aoip);
	int (*nt_open)(aoip_t *aoip);
	int (*nt_close)(aoip_t *aoip);
	int (*nt_recv)(aoip_t *aoip);
	int (*nt_send)(aoip_t *aoip);
};


/*
typedef struct aoip_network_cb {
	int network_stop_flag;

	int (*recv)(aoip_t *aoip);
	int (*send)(aoip_t *aoip);
} network_cb_t;

typedef struct aoip_audio_cb {
	int audio_stop_flag;

	int (*record)(aoip_t *aoip);
	int (*playback)(aoip_t *aoip);
} audio_cb_t;
*/

typedef struct aoip_state_ops {
	int (*s_start)(aoip_t *aoip);
	int (*s_device_init)(aoip_t *aoip);
	int (*s_ptp_listen)(aoip_t *aoip);
	int (*s_ptp_sync)(aoip_t *aoip);
	int (*s_discovery)(aoip_t *aoip);
	int (*s_main_loop)(aoip_t *aoip);
	int (*s_end)(aoip_t *aoip);
} aoip_ops_t;


#define AOIP_API    extern

AOIP_API int register_aoip_device(aoip_t *, struct aoip_operations *);
AOIP_API void unregister_aoip_device(aoip_t *);

AOIP_API int network_cb_run(aoip_t *);
AOIP_API void network_cb_stop(aoip_t *);

AOIP_API int audio_cb_run(aoip_t *);
AOIP_API void audio_cb_stop(aoip_t *);


#endif  /* _aoip_h_ */

