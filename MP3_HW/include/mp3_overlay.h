#ifndef MP3_OVERLAY_H
#define MP3_OVERLAY_H

/*
 * Replace base_path audio with overlay_path beginning at start_sec.
 * During the overlay, only the new audio plays; the original is skipped.
 * After the overlay ends, the base audio resumes when present.
 */
int mp3_overlay_audio(const char *base_path, const char *overlay_path, double start_sec,
                      const char *output_path);

#endif
