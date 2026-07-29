#include "mp3_reader.h"

FILE *mp3_open(const char *path)
{
    (void)path;
    /* TODO */
    return NULL;
}

int mp3_summary(const char *filename, mp3_section_t **out_summary, size_t *out_count)
{
    (void)filename;
    (void)out_summary;
    (void)out_count;
    /* TODO */
    return -1;
}

int mp3_extract_frame_header(const char *filename, mp3_frame_header_t *out)
{
    (void)filename;
    (void)out;
    /* TODO */
    return -1;
}

void mp3_free_sections(mp3_section_t *sections, size_t count)
{
    (void)sections;
    (void)count;
    /* TODO */
}
