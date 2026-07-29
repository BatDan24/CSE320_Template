#include <criterion/criterion.h>
#include <math.h>
#include <unistd.h>

#include "mp3_overlay.h"
#include "mp3_trim.h"
#include "mp3_id3.h"
#include "util.h"

static const char *overlay_output = "/tmp/mp3_overlay_test_out.mp3";

Test(overlay, replaces_short_clip_in_batman) {
    double base_duration = 0.0;
    double overlay_duration = 0.0;

    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &base_duration), 0);
    cr_assert_eq(mp3_get_duration("tests/data/short_mono.mp3", &overlay_duration), 0);

    unlink(overlay_output);
    cr_assert_eq(mp3_overlay_audio("tests/data/Batman.mp3", "tests/data/short_mono.mp3",
                                   10.0, overlay_output),
                 0, "Should overlay audio");

    double out_duration = 0.0;
    cr_assert_eq(mp3_get_duration(overlay_output, &out_duration), 0,
                 "Should read overlaid duration");
    cr_assert_float_eq(out_duration, base_duration, 0.25,
                       "Short overlay in the middle should not change total duration");
    cr_assert_gt(overlay_duration, 0.0, "Overlay clip should have non-zero duration");
}

Test(overlay, extends_when_overlay_runs_past_base_end) {
    double base_duration = 0.0;
    double overlay_duration = 0.0;

    cr_assert_eq(mp3_get_duration("tests/data/Batman.mp3", &base_duration), 0);
    cr_assert_eq(mp3_get_duration("tests/data/short_mono.mp3", &overlay_duration), 0);

    unlink(overlay_output);
    cr_assert_eq(mp3_overlay_audio("tests/data/Batman.mp3", "tests/data/short_mono.mp3",
                                   base_duration - 0.01, overlay_output),
                 0, "Should overlay near end of base track");

    double out_duration = 0.0;
    cr_assert_eq(mp3_get_duration(overlay_output, &out_duration), 0);
    cr_assert_gt(out_duration, base_duration - 0.05,
                 "Output should be at least as long as the base track");
    cr_assert_lt(out_duration, base_duration + overlay_duration + 0.25,
                 "Output should not be much longer than base plus overlay");
}

Test(overlay, preserves_base_id3_regions) {
    const char *out_path = "/tmp/mp3_overlay_id3_test.mp3";
    unlink(out_path);

    cr_assert_eq(mp3_overlay_audio("tests/data/Batman.mp3", "tests/data/short_mono.mp3",
                                   5.0, out_path),
                 0, "Should overlay tagged base file");

    uint8_t *orig = NULL;
    uint8_t *mixed = NULL;
    size_t orig_size = 0;
    size_t mixed_size = 0;

    cr_assert_eq(util_read_file("tests/data/Batman.mp3", &orig, &orig_size), 0);
    cr_assert_eq(util_read_file(out_path, &mixed, &mixed_size), 0);

    size_t id3v2_end = mp3_id3v2_total_size(orig, orig_size);
    size_t id3v1_start = mp3_id3v1_offset(orig, orig_size);

    cr_assert_eq(memcmp(orig, mixed, id3v2_end), 0, "ID3v2 region unchanged");
    cr_assert_geq(mixed_size, 128, "Output should include ID3v1 tag");
    cr_assert_eq(memcmp(orig + id3v1_start, mixed + mixed_size - 128, 128),
                 0, "ID3v1 region unchanged");

    free(orig);
    free(mixed);
}

Test(overlay, rejects_invalid_parameters) {
    cr_assert_neq(mp3_overlay_audio(NULL, "tests/data/short_mono.mp3", 0.0, overlay_output), 0);
    cr_assert_neq(mp3_overlay_audio("tests/data/Batman.mp3", NULL, 0.0, overlay_output), 0);
    cr_assert_neq(mp3_overlay_audio("tests/data/Batman.mp3", "tests/data/short_mono.mp3",
                                    0.0, NULL),
                  0);
    cr_assert_neq(mp3_overlay_audio("tests/data/Batman.mp3", "tests/data/short_mono.mp3",
                                    -1.0, overlay_output),
                  0, "Negative start time should fail");
}
