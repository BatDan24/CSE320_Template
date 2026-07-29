#ifndef MP3_TRIM_H
#define MP3_TRIM_H

/* Returns audio duration in seconds via out_seconds. */
int mp3_get_duration(const char *path, double *out_seconds);

/* Returns timestamp (seconds) of the loudest PCM frame in the file. */
int mp3_get_loudest_timestamp(const char *path, double *out_seconds);

/* Writes a new MP3 containing only audio from [start_sec, end_sec). */
int mp3_trim_audio(const char *input_path, double start_sec, double end_sec,
                   const char *output_path);

#endif
