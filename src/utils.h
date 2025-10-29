#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <wayland-client.h>

void rotate_image(void *dest, const void *src, int w, int h,
                  int bytes_per_pixel, enum wl_output_transform transform);

bool str_to_ulong(const char *str, unsigned long *res);

bool is_valid_signal(int sig);

uint32_t get_bytes_per_pixel(uint32_t format);

#endif /* #ifndef UTILS_H */
