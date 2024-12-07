#ifndef UTILS_H
#define UTILS_H

#include <wayland-client.h>

enum rotate_image_angle {
    ROTATE_0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270
};

void rotate_image(void *dest, const void *src, int w, int h,
                  int bytes_per_pixel, enum rotate_image_angle angle);
enum rotate_image_angle rotate_angle_from_transform(enum wl_output_transform);

#endif /* #ifndef UTILS_H */

