#include <criterion/criterion.h>
#include <math.h>
#include <unistd.h>

#include "mp3_trim.h"
#include "mp3_codec.h"
#include "mp3_id3.h"
#include "util.h"

static const char *trim_output = "/tmp/mp3_trim_test_out.mp3";

Test(trim, get_duration_batman) {
    double duration = 0.0;
    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &duration), 0,
                 "Should read duration");
    cr_assert_gt(duration, 65.0, "Batman.mp3 should be longer than 65 seconds");
    cr_assert_lt(duration, 80.0, "Batman.mp3 should be shorter than 80 seconds");
}

Test(trim, get_duration_null_parameters) {
    double duration = 0.0;
    cr_assert_neq(mp3_get_duration(NULL, &duration), 0);
    cr_assert_neq(mp3_get_duration("tests/data/Batman.mp3", NULL), 0);
}

Test(trim, get_loudest_timestamp_finds_peak) {
    const char *path = "/tmp/mp3_loudest_peak_test.mp3";
    const int sample_rate_hz = 44100;
    const int channels = 1;
    const size_t total_frames = (size_t)sample_rate_hz * 3;
    const size_t peak_frame = (size_t)sample_rate_hz;

    int16_t *samples = (int16_t *)calloc(total_frames, sizeof(int16_t));
    cr_assert_not_null(samples, "Should allocate PCM buffer");
    samples[peak_frame] = 30000;

    mp3_pcm_buffer_t pcm = {
        samples,
        total_frames,
        channels,
        sample_rate_hz,
        320
    };

    uint8_t *encoded = NULL;
    size_t encoded_size = 0;
    cr_assert_eq(mp3_codec_encode(&pcm, &encoded, &encoded_size), 0,
                 "Should encode synthetic PCM");
    cr_assert_eq(util_write_file(path, encoded, encoded_size), 0,
                 "Should write synthetic MP3");

    mp3_pcm_buffer_t decoded;
    cr_assert_eq(mp3_codec_decode(encoded, encoded_size, &decoded), 0,
                 "Should decode synthetic MP3");

    size_t decoded_frames = decoded.sample_count / (size_t)decoded.channels;
    size_t expected_frame = 0;
    size_t expected_level = 0;
    for (size_t frame = 0; frame < decoded_frames; frame++) {
        int peak = decoded.samples[frame] < 0 ? -decoded.samples[frame]
                                              : decoded.samples[frame];
        if ((size_t)peak > expected_level) {
            expected_level = (size_t)peak;
            expected_frame = frame;
        }
    }

    double expected_timestamp =
        (double)expected_frame / (double)decoded.sample_rate_hz;
    double timestamp = 0.0;
    cr_assert_eq(mp3_get_loudest_timestamp(path, &timestamp), 0,
                 "Should find loudest timestamp");
    cr_assert_float_eq(timestamp, expected_timestamp, 0.001,
                       "Loudest timestamp should match decoded peak frame");

    free(samples);
    free(encoded);
    mp3_codec_free_pcm(&decoded);
    unlink(path);
}

Test(trim, get_loudest_timestamp_batman_in_range) {
    double duration = 0.0;
    double timestamp = 0.0;

    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &duration), 0);
    cr_assert_eq(mp3_get_loudest_timestamp("tests/data/Batman.mp3", &timestamp), 0);
    cr_assert_geq(timestamp, 0.0, "Timestamp should be non-negative");
    cr_assert_lt(timestamp, duration, "Timestamp should be within file duration");
}

Test(trim, get_loudest_timestamp_null_parameters) {
    double timestamp = 0.0;
    cr_assert_neq(mp3_get_loudest_timestamp(NULL, &timestamp), 0);
    cr_assert_neq(mp3_get_loudest_timestamp("tests/data/Batman.mp3", NULL), 0);
}

Test(trim, trim_extracts_requested_range) {
    double duration = 0.0;
    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &duration), 0);

    unlink(trim_output);
  cr_assert_eq(mp3_trim_audio("tests/data/Batman.mp3", 10.0, 20.0, trim_output), 0,
                 "Should trim audio range");

    double trimmed_duration = 0.0;
    cr_assert_eq(mp3_get_duration(trim_output, &trimmed_duration), 0,
                 "Should read trimmed duration");
    cr_assert_float_eq(trimmed_duration, 10.0, 0.25,
                       "Trimmed clip should be about 10 seconds");
    cr_assert_lt(trimmed_duration, duration, "Trimmed clip should be shorter than original");
}

Test(trim, trim_preserves_id3_regions) {
    const char *out_path = "/tmp/mp3_trim_id3_test.mp3";
    unlink(out_path);

    cr_assert_eq(mp3_trim_audio("tests/data/Batman.mp3", 5.0, 6.0, out_path), 0,
                 "Should trim tagged file");

    uint8_t *orig = NULL;
    uint8_t *trimmed = NULL;
    size_t orig_size = 0;
    size_t trim_size = 0;

    cr_assert_eq(util_read_file("tests/data/Batman.mp3", &orig, &orig_size), 0);
    cr_assert_eq(util_read_file(out_path, &trimmed, &trim_size), 0);

    size_t id3v2_end = mp3_id3v2_total_size(orig, orig_size);
    size_t id3v1_start = mp3_id3v1_offset(orig, orig_size);

    cr_assert_eq(memcmp(orig, trimmed, id3v2_end), 0, "ID3v2 region unchanged");
    cr_assert_geq(trim_size, 128, "Trimmed file should include ID3v1 tag");
    cr_assert_eq(memcmp(orig + id3v1_start, trimmed + trim_size - 128, 128),
                 0, "ID3v1 region unchanged");

    free(orig);
    free(trimmed);
}

Test(trim, trim_rejects_invalid_range) {
    cr_assert_neq(mp3_trim_audio("tests/data/Batman.mp3", -1.0, 5.0, trim_output), 0,
                  "Negative start should fail");
    cr_assert_neq(mp3_trim_audio("tests/data/Batman.mp3", 10.0, 10.0, trim_output), 0,
                  "Equal start/end should fail");
    cr_assert_neq(mp3_trim_audio("tests/data/Batman.mp3", 20.0, 10.0, trim_output), 0,
                  "Inverted range should fail");

    double duration = 0.0;
    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &duration), 0);
    cr_assert_neq(mp3_trim_audio("tests/data/Batman.mp3", 0.0, duration + 10.0, trim_output), 0,
                  "End past duration should fail");
}

Test(trim, trim_null_parameters) {
    cr_assert_neq(mp3_trim_audio(NULL, 0.0, 1.0, trim_output), 0);
    cr_assert_neq(mp3_trim_audio("tests/data/Batman.mp3", 0.0, 1.0, NULL), 0);
}
