#pragma once

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ptp.h"

int create_udp_socket_nonblock(uint16_t);
int join_mcast_membership(int, struct in_addr, struct in_addr);
