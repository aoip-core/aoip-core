#include <aoip/rtp.h>

int
rtp_create_context(rtp_ctx_t *ctx, uint8_t *local_addr)
{
	int ret = 0;

	memset(ctx, 0, sizeof(*ctx));

	// local_addr
	if (local_addr == NULL) {
		ctx->local_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, (const char *)local_addr, &ctx->local_addr);
	}

	// mcast_addr
	inet_pton(AF_INET, RTP_MULTICAST_GROUP, &ctx->mcast_addr);

	// rtp_fd
	if ((ctx->rtp_fd = create_udp_socket_nonblock()) < 0) {
		fprintf(stderr, "create_udp_socket_nonblock: failed\n");
		ret = -1;
		goto out;
	}

	if (socket_bind(ctx->rtp_fd, RTP_PORT) < 0) {
		fprintf(stderr, "socket_bind: failed\n");
		ret = -1;
		goto out;
	}

	if (join_mcast_membership(ctx->rtp_fd, ctx->local_addr, ctx->mcast_addr) < 0) {
		fprintf(stderr, "join_mcast_membership: failed\n");
		ret = -1;
		goto out;
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
