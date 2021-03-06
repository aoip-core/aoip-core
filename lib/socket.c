#include "aoip/socket.h"

int
aoip_socket_udp_nonblock(void) {
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		perror("socket(SOCK_DGRAM)");
		goto out;
	}

	// non-blocking
	int flags;
	if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
		perror("fcntl");
		goto err;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		perror("fcntl(O_NONBLOCK)");
		goto err;
	}

/*
	// SO_REUSEADDR
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		goto err;
	}
	 */

out:
	return fd;

err:
	close(fd);
	return -1;
}

int
aoip_bind(int fd, uint16_t local_port) {
	struct sockaddr_in saddr = {0};

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(local_port);
	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		perror("bind");
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

int
aoip_mcast_interface(int fd, struct in_addr local_addr)
{
	int ret = 0;

	// Multicast interface
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &local_addr, sizeof(local_addr)) < 0) {
		perror("setsockopt(IP_MULTICAST_IF)");
		ret = -1;
	}

	return ret;
}

int
aoip_mcast_membership(int fd, struct in_addr local_addr, struct in_addr mcast_addr)
{
	int ret = 0;

	// Multicast membership
	// idf_edp (lwip) doesn't support ip_mreqn
	struct ip_mreq req = {0};
	req.imr_interface = local_addr;
	req.imr_multiaddr = mcast_addr;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		ret = -1;
	}

	return ret;
}

int
aoip_drop_mcast_membership(int fd, struct in_addr local_addr, struct in_addr mcast_addr)
{
	int ret = 0;

	// Multicast membership
	// idf_edp (lwip) doesn't support ip_mreqn
	struct ip_mreq req = {0};
	req.imr_interface = local_addr;
	req.imr_multiaddr = mcast_addr;
	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &req, sizeof(req)) < 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		ret = -1;
	}

	return ret;
}

int
aoip_set_tos(int fd, int tos)
{
	int ret = 0;

	if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
		perror("setsockopt(IP_TOS)");
		ret = -1;
	}

	return ret;
}

int
aoip_set_mcast_ttl(int fd, uint8_t ttl)
{
	int ret = 0;

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0) {
		perror("setsockopt(IP_MULTICAST_TTL)");
		ret = -1;
	}

	return ret;
}

