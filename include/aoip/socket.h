#pragma once

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int aoip_socket_udp_nonblock(void);
int aoip_bind(int, uint16_t);
int aoip_mcast_interface(int, struct in_addr);
int aoip_mcast_membership(int, struct in_addr, struct in_addr);
int aoip_drop_mcast_membership(int, struct in_addr, struct in_addr);

