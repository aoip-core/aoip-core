#ifndef _wav_h_
#define _wav_h_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct wav_hdr {
	// riff header
	char chunk_id[4];
    uint32_t chunk_size;
	char form_type[4];
	char subchunk_fmt_id[4];
	uint32_t subchunk_fmt_size;

	// wav header
	uint16_t fmt_tag;
	uint16_t nchannels;
	uint32_t samples_per_sec;
	uint32_t avg_bytes_per_sec;
	uint16_t block_align;
	uint16_t bits_per_sample;
	char subchunk_data_id[4];
	uint32_t subchunk_data_size;
} __attribute__((packed));

/*
static inline byte_swap_2ch_24b(uint8_t *data)
{
	uint8_t tmp[3];

	tmp[0] = data[2];
	tmp[1] = data[1];
	tmp[2] = data[0];
}
*/

int wav_open(char *);
int wav_close(int);
void build_wav_hdr(struct wav_hdr *);
ssize_t init_wav_hdr(int, struct wav_hdr *);
int update_wav_hdr(int, uint32_t);

#endif  /* _wav_h_ */

