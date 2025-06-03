#include <stdint.h>
#include <string.h>

#include "utils.h"

void rotate_image(void *dest, const void *src, int w, int h,
                  int bytes_per_pixel, enum wl_output_transform transform) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    int x, y, new_x, new_y;

    switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
        memcpy(dest, src, w * h * bytes_per_pixel);
        break;

    case WL_OUTPUT_TRANSFORM_90:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = h - 1 - y;
                new_y = x;

                memcpy(d + (new_y * h + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_180:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = w - 1 - x;
                new_y = h - 1 - y;

                memcpy(d + (new_y * w + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_270:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = y;
                new_y = w - 1 - x;

                memcpy(d + (new_y * h + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = w - 1 - x;
                new_y = y;

                memcpy(d + (new_y * w + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = y;
                new_y = x;

                memcpy(d + (new_y * h + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = x;
                new_y = h - 1 - y;

                memcpy(d + (new_y * w + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                new_x = h - 1 - y;
                new_y = w - 1 - x;

                memcpy(d + (new_y * h + new_x) * bytes_per_pixel,
                       s + (y * w + x) * bytes_per_pixel,
                       bytes_per_pixel);
            }
        }
        break;
    }
}

