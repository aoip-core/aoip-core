#include "ptpc/ptpc_socket.h"

int create_udp_socket_nonblock(uint16_t local_port)
{
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		perror("socket(SOCK_DGRAM)");
		goto out;
	}

	// non-blocking
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
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

	// bind
	struct sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(local_port);
	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		perror("bind");
		goto err;
	}

out:
	return fd;

err:
	close(fd);
	return -1;
}

int join_mcast_membership(int fd, struct in_addr local_addr, struct in_addr mcast_addr)
{
	int ret = 0;

	// Multicast interface
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &local_addr, sizeof(local_addr)) < 0) {
		perror("setsockopt(IP_MULTICAST_IF)");
		ret = -1;
		goto out;
	}

	// Multicast membership
	// idf_edp (lwip) doesn't support ip_mreqn
	struct ip_mreq req = {0};
	req.imr_interface = local_addr;
	req.imr_multiaddr = mcast_addr;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

