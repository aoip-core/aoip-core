#pragma once

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int create_udp_socket_nonblock(void);
int socket_bind(int, uint16_t);
int join_mcast_membership(int, struct in_addr, struct in_addr);

