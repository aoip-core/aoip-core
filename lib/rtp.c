#include <aoip/rtp.h>

int
rtp_create_context(rtp_ctx_t *ctx, const rtp_config_t *config, struct in_addr local_addr)
{
	int ret = 0;

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	ctx->local_addr = local_addr;

	// mcast_addr
	inet_pton(AF_INET, RTP_MULTICAST_GROUP, &ctx->mcast_addr);

	// rtp_fd
	if ((ctx->rtp_fd = aoip_socket_udp_nonblock()) < 0) {
		fprintf(stderr, "aoip_socket_udp_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (config->rtp_mode == RTP_MODE_RECV) {
		if (aoip_bind(ctx->rtp_fd, RTP_PORT) < 0) {
			fprintf(stderr, "aoip_bind: failed\n");
			ret = -1;
			goto out;
		}

		if (aoip_mcast_membership(ctx->rtp_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
			fprintf(stderr, "aoip_mcast_membership: failed\n");
			ret = -1;
			goto out;
		}

		if (aoip_mcast_interface(ctx->rtp_fd, ctx->local_addr) < 0) {
			fprintf(stderr, "aoip_mcast_interface: failed\n");
			ret = -1;
			goto out;
		}
	} else if (config->rtp_mode == RTP_MODE_SEND) {
		if (aoip_mcast_interface(ctx->rtp_fd, ctx->local_addr) < 0) {
			fprintf(stderr, "aoip_mcast_interface: failed\n");
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
	close(ctx->rtp_fd);
	ctx->rtp_fd = -1;
}

int recv_rtp_packet(rtp_ctx_t *ctx)
{
	const struct rtp_hdr *rtp_hdr = (struct rtp_hdr *)&ctx->rxbuf;
	int ret = 0;

	if (recv(ctx->rtp_fd, &ctx->rxbuf, PACKET_BUF_SIZE, 0) > 0) {
		if (rtp_hdr->version != 0x2) {
			printf("this isn't RTPv2\n");
		}
		ret = 1;
	}

	return ret;
}
