#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

uint32_t read_u32_be(const uint8_t *buf);
uint32_t read_u32_syncsafe(const uint8_t *buf);

int read_exact(FILE *fp, uint8_t *buf, size_t len);

int util_read_file(const char *path, uint8_t **out_data, size_t *out_size);
int util_write_file(const char *path, const uint8_t *data, size_t size);

#endif
