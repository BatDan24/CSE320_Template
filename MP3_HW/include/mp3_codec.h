#ifndef MP3_CODEC_H
#define MP3_CODEC_H

/*
 * MP3 compression / decompression helpers for audio editing.
 *
 * This module is intended to be distributed separately so all students share the
 * same decode/encode implementation. Decoding uses minimp3 (third_party/).
 * Encoding uses LAME (link with -lmp3lame).
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *samples;
    size_t sample_count;
    int channels;
    int sample_rate_hz;
    int bitrate_kbps;
} mp3_pcm_buffer_t;

/* Decode raw MPEG frame bytes (no ID3 tags) to interleaved PCM samples. */
int mp3_codec_decode(const uint8_t *mpeg_data, size_t mpeg_size, mp3_pcm_buffer_t *out);

void mp3_codec_free_pcm(mp3_pcm_buffer_t *pcm);

/* Encode interleaved PCM samples to MPEG Layer III bytes. */
int mp3_codec_encode(const mp3_pcm_buffer_t *pcm, uint8_t **out_data, size_t *out_size);

#endif
