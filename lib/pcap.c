#include "aoip/pcap.h"

void build_pcap_global_hdr(struct pcap_global_hdr *hdr)
{
	hdr->magic_number = PCAP_MAGIC_NANOSEC;
	hdr->version_major = PCAP_VERSION_MAJOR;
	hdr->version_minor = PCAP_VERSION_MINOR;
	hdr->thiszone = 0;
	hdr->sigfigs = 0;
	hdr->snaplen = PCAP_SNAPLEN;
	hdr->network = 0;
}

void build_pcaprec_hdr(struct pcaprec_hdr *hdr)
{
	hdr->ts_sec = 0;
	hdr->ts_usec = 0;
	hdr->incl_len = 0;
	hdr->orig_len = 0;
}
