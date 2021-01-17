#include <aoip/wav.h>

#define WAV_HDR_CHUNK_ID           "RIFF"
#define WAV_HDR_FORM_TYPE          "WAVE"
#define WAV_HDR_SUBCHUNK_FMT_ID    "fmt "
#define WAV_HDR_SUBCHUNK_DATA_ID   "data"

int wav_open(char *file_name)
{
    return open(file_name, O_CREAT | O_RDWR, 0666);
}

int wav_close(int fd)
{
    return close(fd);
}

void build_wav_hdr(struct wav_hdr *hdr)
{
    memcpy(&hdr->chunk_id[0], WAV_HDR_CHUNK_ID, sizeof(WAV_HDR_CHUNK_ID));
    hdr->chunk_size = 0;
    memcpy(&hdr->form_type[0], WAV_HDR_FORM_TYPE, sizeof(WAV_HDR_FORM_TYPE));
    memcpy(&hdr->subchunk_fmt_id[0], WAV_HDR_SUBCHUNK_FMT_ID,
            sizeof(WAV_HDR_SUBCHUNK_FMT_ID));
    hdr->subchunk_fmt_size = 16;
    hdr->fmt_tag = 1;
    hdr->nchannels = 2;
    hdr->samples_per_sec = 48000;
    hdr->avg_bytes_per_sec = 48000 * 3 * 2;
    hdr->block_align = 6;
    hdr->bits_per_sample = 24;
    memcpy(&hdr->subchunk_data_id[0], WAV_HDR_SUBCHUNK_DATA_ID,
            sizeof(WAV_HDR_SUBCHUNK_DATA_ID));
    hdr->subchunk_data_size = 0;
}

ssize_t init_wav_hdr(int fd, struct wav_hdr *hdr)
{
    return write(fd, hdr, sizeof(struct wav_hdr));
}

#define WAV_HDR_CHUNK_SIZE_OFFSET    4
static ssize_t update_wav_hdr_chunk_size_field(int fd, uint32_t received_frames)
{
    uint32_t chunk_size = received_frames * 6 + sizeof(struct wav_hdr) - 8;
    lseek(fd, WAV_HDR_CHUNK_SIZE_OFFSET, SEEK_SET);
    return write(fd, &chunk_size, sizeof(chunk_size));
}

#define WAV_HDR_SUBCHUNK_DATA_SIZE_OFFSET    40
static ssize_t update_wav_hdr_subchunk_data_size_field(int fd, uint32_t received_frames)
{
    uint32_t subchunk_data_size = received_frames * 6;
    lseek(fd, WAV_HDR_SUBCHUNK_DATA_SIZE_OFFSET, SEEK_SET);
    return write(fd, &subchunk_data_size, sizeof(subchunk_data_size));
}

int update_wav_hdr(int fd, uint32_t received_frames)
{
	ssize_t count;
	int ret = 0;

    // wav header: chunk_size field
    count = update_wav_hdr_chunk_size_field(fd, received_frames);
    if (count < 1) {
        perror("write(chunk_size)");
        ret = -1;
        goto out;
    }

    // wav header: subchunk_data_size field
    count = update_wav_hdr_subchunk_data_size_field(fd, received_frames);
    if (count < 1) {
        perror("write(subchunk_data_size)");
        ret = -1;
        goto out;
    }

out:
    return ret;
}
