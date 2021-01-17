#include "myapp.h"
#include "aoip/wav.h"

int myapp_ao_init(aoip_t *aoip)
{
	audio_t *ao = &aoip->audio;
	audio_config_t *config = &ao->config;

	int ret = 0;

    config->format = AUDIO_FORMAT_L24;  // 24bit
    config->sampling_rate = (PACKET_SAMPLES * PACKET_TIME);    // 48kHz
    config->channels = AUDIO_CHANNEL_STEREO;  // 2ch
    config->packet_time = PACKET_TIME;    // 1ms

    char file_name[] = "output.wav";
	ao->dev.fd = wav_open(file_name);
	if (ao->dev.fd < 0) {
		perror("open(ao->dev.fd)");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

int myapp_ao_release(aoip_t *aoip)
{
	audio_t *ao = &aoip->audio;

	return ao->dev.fd = wav_close(ao->dev.fd);
}

int myapp_ao_open(aoip_t *aoip)
{
	audio_t *ao = &aoip->audio;

	int ret = 0;
	ssize_t count;

	struct wav_hdr wav_hdr = {};
	build_wav_hdr(&wav_hdr);

	count = init_wav_hdr(ao->dev.fd, &wav_hdr);
	if (count < 1) {
		perror("write(ao->dev.fd)");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

int myapp_ao_close(aoip_t *aoip)
{
	const audio_t *ao = &aoip->audio;
    const stats_t *stats = &aoip->stats;

	return update_wav_hdr(ao->dev.fd, stats->received_frames);
}

int myapp_ao_read(aoip_t *aoip)
{
    int ret = 0;

	return ret;
}

int myapp_ao_write(aoip_t *aoip)
{
	stats_t *stats = &aoip->stats;
	queue_t *queue = &aoip->queue;
	audio_t *ao = &aoip->audio;
	uint8_t tmp[6];

	int count = 0, ret = 0;

	const queue_slot_t *slot = queue_read_ptr(queue);

	if (!queue_empty(queue)) {
//		struct rtp_hdr *rtp = (struct rtp_hdr *)&slot->data[0];
//		printf( "rtp.version=%u, rtp.payload_type=%u, "
//			"rtp.sequence=%u, rtp.timestamp=%u\n",
//				rtp->version, rtp->payload_type,
//				ntohs(rtp->sequence), ntohl(rtp->timestamp));
//
//		count = write(ao->dev.fd, (char *)slot->data + RTP_HDR_SIZE, 
//				slot->len - RTP_HDR_SIZE);
//		if (count < 1) {
//			perror("write");
//			ret = -1;
//		}
//		uint8_t *list = (uint8_t *)&slot->data[0];
//		printf( "%02X %02X %02X %02X "
//			"%02X %02X %02X %02X\n",
//				list[12], list[13], list[14], list[15],
//				list[16], list[17], list[18], list[19]);

		char *p = (char *)&slot->data[0];
		int i;
		for (i = RTP_HDR_SIZE; i < slot->len; i=i+6) {
			tmp[0] = p[2+i]; // big-endian to little-endian
			tmp[1] = p[1+i];
			tmp[2] = p[0+i];
			tmp[3] = p[5+i];
			tmp[4] = p[4+i];
			tmp[5] = p[3+i];
			count = write(ao->dev.fd, tmp, sizeof(tmp));
			if (count < 1) {
				perror("write(ao->dev.fd)");
				ret = -1;
			}
			++stats->received_frames;
		}

		queue_read_next(queue);
	}

	return ret;
}

