#pragma once

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TOS_CLASS_CLOCK 0xb8
#define TOS_CLASS_MEDIA 0x88

int aoip_socket_udp_nonblock(void);
int aoip_bind(int, uint16_t);
int aoip_mcast_interface(int, struct in_addr);
int aoip_mcast_membership(int, struct in_addr, struct in_addr);
int aoip_drop_mcast_membership(int, struct in_addr, struct in_addr);
int aoip_set_tos(int, int);
int aoip_set_mcast_ttl(int, uint8_t);
