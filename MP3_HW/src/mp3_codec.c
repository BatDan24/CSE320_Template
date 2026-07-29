/*
 * Instructor-provided MP3 codec layer.
 *
 * Decode: minimp3 (CC0, third_party/minimp3.h)
 * Encode: LAME    (libmp3lame)
 */

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#include "mp3_codec.h"

#include <lame/lame.h>
#include <stdlib.h>
#include <string.h>

int mp3_codec_decode(const uint8_t *mpeg_data, size_t mpeg_size, mp3_pcm_buffer_t *out)
{
    if (mpeg_data == NULL || out == NULL || mpeg_size == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    mp3dec_t dec;
    mp3dec_init(&dec);

    mp3dec_file_info_t info;
    if (mp3dec_load_buf(&dec, mpeg_data, mpeg_size, &info, NULL, NULL) != 0) {
        return -1;
    }

    if (info.samples == 0 || info.buffer == NULL) {
        free(info.buffer);
        return -1;
    }

    size_t bytes = info.samples * sizeof(int16_t);
    int16_t *samples = (int16_t *)malloc(bytes);
    if (samples == NULL) {
        free(info.buffer);
        return -1;
    }

    memcpy(samples, info.buffer, bytes);
    free(info.buffer);

    out->samples = samples;
    out->sample_count = info.samples;
    out->channels = info.channels;
    out->sample_rate_hz = info.hz;
    out->bitrate_kbps = (info.avg_bitrate_kbps > 0) ? info.avg_bitrate_kbps : 128;
    return 0;
}

void mp3_codec_free_pcm(mp3_pcm_buffer_t *pcm)
{
    if (pcm == NULL) {
        return;
    }
    free(pcm->samples);
    pcm->samples = NULL;
    pcm->sample_count = 0;
}

int mp3_codec_encode(const mp3_pcm_buffer_t *pcm, uint8_t **out_data, size_t *out_size)
{
    if (pcm == NULL || out_data == NULL || out_size == NULL ||
        pcm->samples == NULL || pcm->sample_count == 0 ||
        pcm->channels < 1 || pcm->channels > 2 ||
        pcm->sample_rate_hz <= 0) {
        return -1;
    }

    lame_t lame = lame_init();
    if (lame == NULL) {
        return -1;
    }

    lame_set_in_samplerate(lame, pcm->sample_rate_hz);
    lame_set_num_channels(lame, pcm->channels);
    lame_set_VBR(lame, vbr_off);
    if (pcm->bitrate_kbps < 320) {
        lame_set_brate(lame, 320);
    } else {
        lame_set_brate(lame, pcm->bitrate_kbps);
    }
    lame_set_quality(lame, 0);
    if (lame_init_params(lame) < 0) {
        lame_close(lame);
        return -1;
    }

    size_t mp3_capacity = pcm->sample_count + (pcm->sample_count / 2) + 7200;
    uint8_t *mp3buf = (uint8_t *)malloc(mp3_capacity);
    if (mp3buf == NULL) {
        lame_close(lame);
        return -1;
    }

    size_t total_written = 0;
    size_t offset = 0;
    size_t frame_samples = (size_t)pcm->sample_count / (size_t)pcm->channels;

    while (offset < frame_samples) {
        size_t chunk = frame_samples - offset;
        if (chunk > 1152) {
            chunk = 1152;
        }

        int written;
        if (pcm->channels == 1) {
            written = lame_encode_buffer(
                lame,
                pcm->samples + offset,
                pcm->samples + offset,
                (int)chunk,
                mp3buf + total_written,
                (int)(mp3_capacity - total_written));
        } else {
            written = lame_encode_buffer_interleaved(
                lame,
                pcm->samples + offset * 2,
                (int)chunk,
                mp3buf + total_written,
                (int)(mp3_capacity - total_written));
        }

        if (written < 0) {
            free(mp3buf);
            lame_close(lame);
            return -1;
        }

        total_written += (size_t)written;
        offset += chunk;
    }

    int flushed = lame_encode_flush(lame, mp3buf + total_written,
                                    (int)(mp3_capacity - total_written));
    lame_close(lame);

    if (flushed < 0) {
        free(mp3buf);
        return -1;
    }

    total_written += (size_t)flushed;
    *out_data = mp3buf;
    *out_size = total_written;
    return 0;
}
