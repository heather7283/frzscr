#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "shm.h"
#include "common.h"
#include "wayland.h"

int create_buffer(struct buffer *buffer, enum wl_shm_format format,
                  uint32_t width, uint32_t height, uint32_t stride) {
    buffer->height = height;
    buffer->width = width;
    buffer->stride = stride;

    size_t size = stride * height;

    int fd = memfd_create("frzscr-wayland-shm", MFD_CLOEXEC);
    if (fd < 0) {
        EDIE("failed to crate memfd");
    }

    if (posix_fallocate(fd, 0, size) < 0) {
        EDIE("posix_fallocate() failed");
    };

    buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer->data == MAP_FAILED) {
        EDIE("mmap failed");
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(wayland.shm, fd, size);
    buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);

    wl_shm_pool_destroy(pool);
    close(fd);

    return 0;
}

void destroy_buffer(struct buffer *buffer) {
    wl_buffer_destroy(buffer->wl_buffer);
    if (munmap(buffer->data, buffer->stride * buffer->height) < 0) {
        EWARN("munmap() failed");
    }
}

