#ifndef SHM_H
#define SHM_H

#include <wayland-client.h>
#include <stdint.h>

#include "wayland.h"

int create_buffer(struct buffer *buffer, enum wl_shm_format format,
                  uint32_t width, uint32_t height, uint32_t stride);

#endif /* #ifndef SHM_H */

