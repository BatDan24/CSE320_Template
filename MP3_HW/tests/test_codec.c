#include <criterion/criterion.h>
#include <string.h>

#include "mp3_codec.h"
#include "mp3_id3.h"
#include "util.h"

Test(codec, decode_and_encode_batman) {
    uint8_t *file = NULL;
    size_t file_size = 0;
    cr_assert_eq(util_read_file("tests/data/Batman.mp3", &file, &file_size), 0,
                 "Should read Batman.mp3");

    size_t audio_start = mp3_mpeg_audio_start(file, file_size);
    size_t audio_end = mp3_mpeg_audio_end(file, file_size);

    mp3_pcm_buffer_t pcm;
    cr_assert_eq(mp3_codec_decode(file + audio_start, audio_end - audio_start, &pcm), 0,
                 "Should decode MPEG audio");
    cr_assert_gt(pcm.sample_count, 0, "Should produce PCM samples");
    cr_assert_eq(pcm.channels, 2, "Batman should be stereo");

    uint8_t *encoded = NULL;
    size_t encoded_size = 0;
    cr_assert_eq(mp3_codec_encode(&pcm, &encoded, &encoded_size), 0,
                 "Should re-encode PCM");
    cr_assert_gt(encoded_size, 0, "Encoded MPEG should be non-empty");

    mp3_pcm_buffer_t roundtrip;
    cr_assert_eq(mp3_codec_decode(encoded, encoded_size, &roundtrip), 0,
                 "Should decode re-encoded audio");
    cr_assert_gt(roundtrip.sample_count, 0, "Roundtrip should produce samples");

    mp3_codec_free_pcm(&roundtrip);
    mp3_codec_free_pcm(&pcm);
    free(encoded);
    free(file);
}
