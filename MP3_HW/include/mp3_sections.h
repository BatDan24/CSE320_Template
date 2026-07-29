#ifndef MP3_SECTIONS_H
#define MP3_SECTIONS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int mpeg_version;
    int layer;
    int protection;
    int bitrate_kbps;
    int sample_rate_hz;
    int padding;
    int private_bit;
    int channel_mode;
    int mode_extension;
    int copyright;
    int original;
    int emphasis;
    size_t frame_size;
} mp3_frame_header_t;

typedef struct {
    size_t offset;
    char type[8];
    char id[5];
    uint32_t length;
    uint8_t *data;
} mp3_section_t;

const char *mp3_mpeg_version_str(int version);
const char *mp3_layer_str(int layer);
const char *mp3_channel_mode_str(int mode);
const char *mp3_emphasis_str(int emphasis);

int mp3_is_sync(const uint8_t *buf);
int mp3_parse_frame_header(const uint8_t *buf, mp3_frame_header_t *out);

#endif
