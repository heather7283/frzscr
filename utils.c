#include <stdint.h>
#include <string.h>
#include <wayland-client-protocol.h>

#include "common.h"
#include "utils.h"

void rotate_image(void *dest, const void *src, int w, int h,
                  int bytes_per_pixel, enum rotate_image_angle angle) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    int x, y, new_x, new_y;

    switch (angle) {
        case ROTATE_0:
            memcpy(dest, src, w * h * bytes_per_pixel);
            break;

        case ROTATE_90:
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    new_x = h - 1 - y;
                    new_y = x;

                    memcpy(
                        d + (new_y * h + new_x) * bytes_per_pixel,
                        s + (y * w + x) * bytes_per_pixel,
                        bytes_per_pixel
                    );
                }
            }
            break;

        case ROTATE_180:
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    new_x = w - 1 - x;
                    new_y = h - 1 - y;

                    memcpy(
                        d + (new_y * w + new_x) * bytes_per_pixel,
                        s + (y * w + x) * bytes_per_pixel,
                        bytes_per_pixel
                    );
                }
            }
            break;

        case ROTATE_270:
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    new_x = y;
                    new_y = w - 1 - x;

                    memcpy(
                        d + (new_y * h + new_x) * bytes_per_pixel,
                        s + (y * w + x) * bytes_per_pixel,
                        bytes_per_pixel
                    );
                }
            }
            break;
    }
}

int bytes_per_pixel_from_format(enum wl_shm_format format) {
    // this is incomplete, but I cba to write 1000 lines long switch case
    // so I will add more formats as needed
    switch (format) {
        case WL_SHM_FORMAT_ARGB8888:
        case WL_SHM_FORMAT_XRGB8888:
        case WL_SHM_FORMAT_ABGR8888:
        case WL_SHM_FORMAT_XBGR8888:
            return 4;

        case WL_SHM_FORMAT_BGR888:
        case WL_SHM_FORMAT_RGB888:
            return 3;

        case WL_SHM_FORMAT_RGB565:
            return 2;

        case WL_SHM_FORMAT_C8:
        case WL_SHM_FORMAT_R8:
            return 1;

        default:
            return 0;
    }
}

enum rotate_image_angle rotate_angle_from_transform(enum wl_output_transform transform) {
    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            return ROTATE_0;
        case WL_OUTPUT_TRANSFORM_90:
            return ROTATE_90;
        case WL_OUTPUT_TRANSFORM_180:
            return ROTATE_180;
        case WL_OUTPUT_TRANSFORM_270:
            return ROTATE_270;
        default:
            warn("%d is not a known transform, defaulting to 0 (FIXME)\n", transform);
            return ROTATE_0;
    }
}

