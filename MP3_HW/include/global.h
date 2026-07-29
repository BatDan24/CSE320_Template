#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>

#include "mp3_sections.h"
#include "mp3_id3.h"

/* Usage/Help Messages */
#define PRINT_USAGE(prog_name) do { \
    fprintf(stdout, "Usage: %s -f mp3_file [options]\n", prog_name); \
    fprintf(stdout, "Options:\n"); \
    fprintf(stdout, "  -f mp3_file               Input MP3 file (required)\n"); \
    fprintf(stdout, "  -h                        Print this help message\n"); \
    fprintf(stdout, "  -s                        Print section summary\n"); \
    fprintf(stdout, "  -t                        Print ID3 metadata\n"); \
    fprintf(stdout, "  -i                        Print MPEG frame header fields\n"); \
    fprintf(stdout, "  -l                        Print audio duration in seconds\n"); \
    fprintf(stdout, "  -p                        Print timestamp of loudest audio\n"); \
    fprintf(stdout, "  -c start end -o out_file  Extract audio from start to end seconds\n"); \
    fprintf(stdout, "  -m file2 -a start -o out  Overlay file2 at start seconds in input\n"); \
} while(0)

/* Error Messages */
#define PRINT_ERROR_OPEN_FILE(filename) fprintf(stderr, "Error: Failed to open file %s\n", filename)
#define PRINT_ERROR_READ_SECTIONS() fprintf(stderr, "Error: Failed to read sections\n")
#define PRINT_ERROR_READ_METADATA() fprintf(stderr, "Error: No ID3 metadata found or failed to parse\n")
#define PRINT_ERROR_READ_FRAME() fprintf(stderr, "Error: Failed to read MPEG frame header\n")
#define PRINT_ERROR_PARSE_FRAME() fprintf(stderr, "Error: Failed to parse MPEG frame header\n")
#define PRINT_ERROR_DURATION_FAILED() fprintf(stderr, "Error: Failed to read audio duration\n")
#define PRINT_ERROR_LOUDEST_FAILED() fprintf(stderr, "Error: Failed to find loudest audio timestamp\n")
#define PRINT_ERROR_TRIM_REQUIRES() fprintf(stderr, "Error: -c requires <start> <end> -o <output_file>\n")
#define PRINT_ERROR_TRIM_FAILED() fprintf(stderr, "Error: Failed to extract audio range\n")
#define PRINT_ERROR_OVERLAY_REQUIRES() fprintf(stderr, "Error: -m requires <file2> -a <start> -o <output_file>\n")
#define PRINT_ERROR_OVERLAY_FAILED() fprintf(stderr, "Error: Failed to overlay audio files\n")
#define PRINT_ERROR_UNKNOWN_OPTION(option) fprintf(stderr, "Error: Unknown option %s\n", option)
#define PRINT_ERROR_MISSING_F_FLAG() fprintf(stderr, "Error: -f flag with filename is required\n")
#define PRINT_ERROR_F_REQUIRES_FILENAME() fprintf(stderr, "Error: -f requires a filename\n")

/* Section Summary Messages */
#define PRINT_SECTION_SUMMARY_HEADER(filename) printf("Section Summary for %s:\n", filename)
#define PRINT_SECTION_INFO(i, section) do { \
    if ((section).type[0] == 'I' && strcmp((section).type, "ID3F") == 0) { \
        printf("  Section %d: Type=%s, ID=%s, Offset=%zu, Length=%u\n", \
               i, (section).type, (section).id, (section).offset, (section).length); \
    } else { \
        printf("  Section %d: Type=%s, Offset=%zu, Length=%u\n", \
               i, (section).type, (section).offset, (section).length); \
    } \
} while(0)

/* Frame Header Messages */
#define PRINT_FRAME_HEADER(filename, hdr) do { \
    printf("MPEG Frame Header for %s:\n", filename); \
    printf("  MPEG Version: %s\n", mp3_mpeg_version_str((hdr).mpeg_version)); \
    printf("  Layer: %s\n", mp3_layer_str((hdr).layer)); \
    printf("  Protection: %s\n", (hdr).protection ? "none" : "CRC"); \
    printf("  Bitrate: %d kbps\n", (hdr).bitrate_kbps); \
    printf("  Sample Rate: %d Hz\n", (hdr).sample_rate_hz); \
    printf("  Padding: %u\n", (hdr).padding); \
    printf("  Channel Mode: %s\n", mp3_channel_mode_str((hdr).channel_mode)); \
    printf("  Copyright: %u\n", (hdr).copyright); \
    printf("  Original: %u\n", (hdr).original); \
    printf("  Emphasis: %s\n", mp3_emphasis_str((hdr).emphasis)); \
    printf("  Frame Size: %zu bytes\n", (hdr).frame_size); \
} while(0)

/* Metadata Messages */
#define PRINT_METADATA_HEADER(filename) printf("ID3 Metadata for %s:\n", filename)
#define PRINT_METADATA_V2_VERSION(major, minor) \
    printf("  ID3v2 Version: %u.%u\n", (unsigned)(major), (unsigned)(minor))
#define PRINT_METADATA_V2_FRAME(id, value) printf("  ID3v2 %s: %s\n", id, value)
#define PRINT_METADATA_V1_TITLE(value) printf("  ID3v1 Title: %s\n", value)
#define PRINT_METADATA_V1_ARTIST(value) printf("  ID3v1 Artist: %s\n", value)
#define PRINT_METADATA_V1_ALBUM(value) printf("  ID3v1 Album: %s\n", value)
#define PRINT_METADATA_V1_YEAR(value) printf("  ID3v1 Year: %s\n", value)
#define PRINT_METADATA_V1_COMMENT(value) printf("  ID3v1 Comment: %s\n", value)
#define PRINT_METADATA_V1_TRACK(track) printf("  ID3v1 Track: %u\n", (unsigned)(track))
#define PRINT_METADATA_V1_GENRE(num, name) printf("  ID3v1 Genre: %u (%s)\n", (unsigned)(num), name)

/* Success Messages */
#define PRINT_DURATION(seconds) printf("Duration: %.3f seconds\n", seconds)
#define PRINT_LOUDEST_TIMESTAMP(seconds) printf("Loudest audio at: %.3f seconds\n", seconds)
#define PRINT_TRIM_SUCCESS(output_file) printf("Audio extracted successfully to %s\n", output_file)
#define PRINT_OVERLAY_SUCCESS(output_file) printf("Overlay completed successfully to %s\n", output_file)

#endif /* GLOBAL_H */
