#ifndef SHM_H
#define SHM_H

#include <wayland-client.h>
#include <stdint.h>

struct wl_buffer *create_buffer(void **data, enum wl_shm_format format,
                                int32_t width, int32_t height, int32_t stride);

#endif /* #ifndef SHM_H */

