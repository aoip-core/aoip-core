#ifndef AOIP_PCAP_H
#define AOIP_PCAP_H

#include <stdint.h>

#define PCAP_MAGIC_NANOSEC (0xa1b2c3d4)
#define PCAP_VERSION_MAJOR (0x2)
#define PCAP_VERSION_MINOR (0x4)
#define PCAP_SNAPLEN       (0xFFFF)
#define PCAP_NETWORK       (0x1)      // linktype: ethernet

struct pcap_global_hdr {
	uint32_t magic_number;
	uint16_t version_major;
	uint16_t version_minor;
	int thiszone;
	uint32_t sigfigs;
	uint32_t snaplen;
	uint32_t network;
} __attribute__((packed));

struct pcaprec_hdr {
	uint32_t ts_sec;
	uint32_t ts_usec;
	uint32_t incl_len;
	uint32_t orig_len;
} __attribute__((packed));

void build_pcap_global_hdr(struct pcap_global_hdr *);
void build_pcaprec_hdr(struct pcaprec_hdr *);

#endif //AOIP_PCAP_H
