#ifndef MP3_ID3_H
#define MP3_ID3_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char frame_id[5];
    char *value;
} mp3_id3v2_frame_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    mp3_id3v2_frame_t *frames;
    size_t frame_count;
} mp3_id3v2_tag_t;

typedef struct {
    char title[31];
    char artist[31];
    char album[31];
    char year[5];
    char comment[30];
    uint8_t track;
    uint8_t genre;
    int has_track;
} mp3_id3v1_tag_t;

typedef struct {
    int has_v2;
    int has_v1;
    mp3_id3v2_tag_t v2;
    mp3_id3v1_tag_t v1;
} mp3_metadata_t;

int mp3_has_id3v2(const uint8_t *data, size_t size);
int mp3_has_id3v1(const uint8_t *data, size_t size);

size_t mp3_id3v2_total_size(const uint8_t *data, size_t size);
size_t mp3_id3v1_offset(const uint8_t *data, size_t size);

size_t mp3_mpeg_audio_start(const uint8_t *data, size_t size);
size_t mp3_mpeg_audio_end(const uint8_t *data, size_t size);

int mp3_extract_metadata(const char *filename, mp3_metadata_t *out);
void mp3_free_metadata(mp3_metadata_t *meta);

const char *mp3_id3v1_genre_name(uint8_t genre);

#endif
