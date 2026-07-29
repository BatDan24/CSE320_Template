#include "mp3_id3.h"

int mp3_has_id3v2(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

int mp3_has_id3v1(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

size_t mp3_id3v2_total_size(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

size_t mp3_id3v1_offset(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

size_t mp3_mpeg_audio_start(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

size_t mp3_mpeg_audio_end(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    /* TODO */
    return 0;
}

int mp3_extract_metadata(const char *filename, mp3_metadata_t *out)
{
    (void)filename;
    (void)out;
    /* TODO */
    return -1;
}

void mp3_free_metadata(mp3_metadata_t *meta)
{
    (void)meta;
    /* TODO */
}

const char *mp3_id3v1_genre_name(uint8_t genre)
{
    (void)genre;
    /* TODO */
    return NULL;
}
