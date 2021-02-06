#include "myapp.h"
#include "aoip/wav.h"

int myapp_ao_init(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	int ret = 0;

    char file_name[] = "output.wav";
	audio->fd = wav_open(file_name);
	if (audio->fd < 0) {
		perror("open(ao->dev.fd)");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

int myapp_ao_release(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
	audio->fd = wav_close(audio->fd);
	return 0;
}

int myapp_ao_open(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;

	struct wav_hdr wav_hdr = {0};
	build_wav_hdr(&wav_hdr);

	int ret = 0;
	if (init_wav_hdr(audio->fd, &wav_hdr) < 1) {
		perror("write(ao->dev.fd)");
		ret = -1;
	}

	return ret;
}

int myapp_ao_close(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
    const stats_t *stats = &ctx->stats;

	return update_wav_hdr(audio->fd, stats->received_frames);
}

int myapp_ao_read(aoip_ctx_t *ctx, void *arg)
{
	return 0;
}

int myapp_ao_write(aoip_ctx_t *ctx, void *arg)
{
	struct audio_ctx *audio = (struct audio_ctx *)arg;
	stats_t *stats = &ctx->stats;
	queue_t *queue = &ctx->queue;
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
		for (int i = RTP_HDR_SIZE; i < slot->len; i=i+6) {
			tmp[0] = p[2+i]; // big-endian to little-endian
			tmp[1] = p[1+i];
			tmp[2] = p[0+i];
			tmp[3] = p[5+i];
			tmp[4] = p[4+i];
			tmp[5] = p[3+i];
			count = write(audio->fd, tmp, sizeof(tmp));
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

