#ifndef UTILS_H
#define UTILS_H

#include <wayland-client.h>

void rotate_image(void *dest, const void *src, int w, int h,
                  int bytes_per_pixel, enum wl_output_transform transform);

#endif /* #ifndef UTILS_H */

