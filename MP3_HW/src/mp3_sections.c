#include "mp3_sections.h"

const char *mp3_mpeg_version_str(int version)
{
    (void)version;
    /* TODO */
    return NULL;
}

const char *mp3_layer_str(int layer)
{
    (void)layer;
    /* TODO */
    return NULL;
}

const char *mp3_channel_mode_str(int mode)
{
    (void)mode;
    /* TODO */
    return NULL;
}

const char *mp3_emphasis_str(int emphasis)
{
    (void)emphasis;
    /* TODO */
    return NULL;
}

int mp3_is_sync(const uint8_t *buf)
{
    (void)buf;
    /* TODO */
    return 0;
}

int mp3_parse_frame_header(const uint8_t *buf, mp3_frame_header_t *out)
{
    (void)buf;
    (void)out;
    /* TODO */
    return -1;
}
