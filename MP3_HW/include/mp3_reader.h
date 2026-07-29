#ifndef MP3_READER_H
#define MP3_READER_H

#include <stdio.h>
#include <stddef.h>

#include "mp3_sections.h"

FILE *mp3_open(const char *path);

int mp3_summary(const char *filename, mp3_section_t **out_summary, size_t *out_count);

int mp3_extract_frame_header(const char *filename, mp3_frame_header_t *out);

void mp3_free_section(mp3_section_t *section);
void mp3_free_sections(mp3_section_t *sections, size_t count);

#endif
