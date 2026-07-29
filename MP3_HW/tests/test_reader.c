#include <criterion/criterion.h>
#include <string.h>

#include "mp3_reader.h"
#include "mp3_sections.h"
#include "mp3_id3.h"
#include "util.h"

Test(reader, mp3_open_valid) {
    FILE *fp = mp3_open("tests/data/frames_only.mp3");
    cr_assert_not_null(fp, "Should open valid MP3 file");
    fclose(fp);
}

Test(reader, mp3_summary_frames_only) {
    mp3_section_t *summary = NULL;
    size_t count = 0;
    cr_assert_eq(mp3_summary("tests/data/frames_only.mp3", &summary, &count), 0,
                 "Should summarize frames-only MP3");
    cr_assert_gt(count, 0, "Should have at least one section");

    int mpeg_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(summary[i].type, "MPEG") == 0) {
            mpeg_count++;
        }
    }
    cr_assert_eq(mpeg_count, 8, "Should find 8 MPEG frames");

    mp3_free_sections(summary, count);
}

Test(reader, mp3_summary_with_id3) {
    mp3_section_t *summary = NULL;
    size_t count = 0;
    cr_assert_eq(mp3_summary("tests/data/with_id3v2.mp3", &summary, &count), 0,
                 "Should summarize MP3 with ID3v2");

    cr_assert_str_eq(summary[0].type, "ID3H", "First section should be ID3 header");
    cr_assert_str_eq(summary[1].type, "ID3F", "Second section should be ID3 frame");
    cr_assert_str_eq(summary[1].id, "TIT2", "First frame should be title");

    mp3_free_sections(summary, count);
}

Test(reader, mp3_extract_metadata_full_tags) {
    mp3_metadata_t metadata;
    cr_assert_eq(mp3_extract_metadata("tests/data/full_tags.mp3", &metadata), 0,
                 "Should extract metadata");

    cr_assert(metadata.has_v2, "Should have ID3v2");
    cr_assert(metadata.has_v1, "Should have ID3v1");
    cr_assert_eq(metadata.v2.frame_count, 2, "Should have two ID3v2 frames");
    cr_assert_str_eq(metadata.v2.frames[0].frame_id, "TIT2", "First frame is title");
    cr_assert_str_eq(metadata.v2.frames[0].value, "Full Track", "Title value");
    cr_assert_str_eq(metadata.v1.title, "Full Track", "ID3v1 title");
    cr_assert_str_eq(metadata.v1.artist, "Fixture Band", "ID3v1 artist");

    mp3_free_metadata(&metadata);
}

Test(reader, mp3_mpeg_audio_bounds_skip_id3v1) {
    uint8_t *data = NULL;
    size_t size = 0;
    cr_assert_eq(util_read_file("tests/data/full_tags.mp3", &data, &size), 0,
                 "Should read file");

    cr_assert_gt(mp3_mpeg_audio_start(data, size), 0, "Should skip ID3v2");
    cr_assert_lt(mp3_mpeg_audio_end(data, size), size, "Should stop before ID3v1");
    cr_assert_eq(mp3_mpeg_audio_end(data, size), size - 128, "ID3v1 offset");

    free(data);
}

Test(reader, mp3_extract_frame_header) {
    mp3_frame_header_t hdr;
    cr_assert_eq(mp3_extract_frame_header("tests/data/frames_only.mp3", &hdr), 0,
                 "Should extract frame header");
    cr_assert_eq(hdr.mpeg_version, 3, "Should be MPEG 1");
    cr_assert_eq(hdr.layer, 1, "Should be Layer III");
    cr_assert_eq(hdr.bitrate_kbps, 128, "Should be 128 kbps");
    cr_assert_eq(hdr.sample_rate_hz, 44100, "Should be 44100 Hz");
}

Test(sections, mp3_parse_frame_header) {
    uint8_t header[4] = {0xFF, 0xFB, 0x90, 0x00};
    mp3_frame_header_t hdr;
    cr_assert_eq(mp3_parse_frame_header(header, &hdr), 0, "Should parse valid header");
    cr_assert_eq(hdr.frame_size, 417, "Frame size should match 128kbps/44.1kHz");
}

Test(sections, mp3_is_sync) {
    uint8_t sync[4] = {0xFF, 0xFB, 0x90, 0x00};
    uint8_t bad[4] = {0x00, 0x00, 0x00, 0x00};
    cr_assert(mp3_is_sync(sync), "Should detect sync word");
    cr_assert_not(mp3_is_sync(bad), "Should reject non-sync data");
}
