#include <aoip/rtp.h>

int
rtp_create_context(rtp_ctx_t *ctx, const rtp_config_t *config, struct in_addr local_addr,
		uint16_t rtp_packet_length, uint16_t audio_packet_time, uint8_t *txbuf, uint8_t *rxbuf)
{
	int ret = 0;

	memset(ctx, 0, sizeof(*ctx));

	// rtp_mode
	ctx->rtp_mode = config->rtp_mode;

	// rtp_send_interval
	ctx->rtp_send_interval = audio_packet_time * 1000000;  // msec to nsec

	// rtp_packet_length
	ctx->rtp_packet_length = rtp_packet_length;

	// local_addr
	ctx->local_addr = local_addr;

	// txbuf
	if (txbuf == NULL) {
		fprintf(stderr, "txbuf isn't allocated\n");
		ret = -1;
		goto out;
	} else {
		ctx->txbuf = txbuf;
	}

	// rxbuf
	if (rxbuf == NULL) {
		fprintf(stderr, "rxbuf isn't allocated\n");
		ret = -1;
		goto out;
	} else {
		ctx->rxbuf = rxbuf;
	}

	// mcast_addr
	ctx->mcast_addr.sin_family = AF_INET;
	ctx->mcast_addr.sin_port = htons(RTP_PORT);
	inet_pton(AF_INET, RTP_MULTICAST_GROUP, &ctx->mcast_addr.sin_addr);

	// rtp_fd
	if ((ctx->rtp_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (aoip_mcast_interface(ctx->rtp_fd, ctx->local_addr) < 0) {
		fprintf(stderr, "aoip_mcast_interface: failed\n");
		ret = -1;
		goto out;
	}

	if (ctx->rtp_mode == RTP_MODE_RECV) {
		if (aoip_bind(ctx->rtp_fd, RTP_PORT) < 0) {
			fprintf(stderr, "aoip_bind: failed\n");
			ret = -1;
			goto out;
		}

		if (aoip_mcast_membership(ctx->rtp_fd, ctx->local_addr, ctx->mcast_addr.sin_addr) < 0) {
			fprintf(stderr, "aoip_mcast_membership: failed\n");
			ret = -1;
			goto out;
		}
	}

out:
	return ret;
}

void
rtp_context_destroy(rtp_ctx_t *ctx)
{
	if (ctx->rtp_mode == RTP_MODE_RECV) {
		if (aoip_drop_mcast_membership(ctx->rtp_fd, ctx->local_addr, ctx->mcast_addr.sin_addr) < 0) {
			fprintf(stderr, "aoip_drop_mcast_membership: failed\n");
		}
	}

	close(ctx->rtp_fd);
	ctx->rtp_fd = -1;
}

int recv_rtp_packet(rtp_ctx_t *ctx)
{
	const struct rtp_hdr *rtp_hdr = (struct rtp_hdr *)ctx->rxbuf;
	int ret = 0;

	if (recv(ctx->rtp_fd, ctx->rxbuf, RTP_PACKET_BUF_SIZE, 0) > 0) {
		if (rtp_hdr->version != 0x2) {
			printf("this isn't RTPv2\n");
		}
		ret = 1;
	}

	return ret;
}

void build_rtp_hdr(uint8_t *buf, uint8_t ptype, uint32_t ssrc)
{
	struct rtp_hdr *rtp = (struct rtp_hdr *)buf;

	memset(buf, 0, sizeof(struct rtp_hdr));

	rtp->version = 0x2;
	rtp->payload_type = ptype;
	rtp->ssrc = htonl(ssrc);
}
