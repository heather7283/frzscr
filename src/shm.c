#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "common.h"
#include "wayland.h"
#include "xmalloc.h"

static int create_tmpfile_cloexec(char *tmpname) {
    int fd = mkstemp(tmpname);

    if (fd < 0) {
        die("failed to create temporary file %s: %s\n", tmpname, strerror(errno));
    }

    if (unlink(tmpname) < 0) {
        warn("failed to remove temporary file %s: %s\n", tmpname, strerror(errno));
    };

    int flags = fcntl(fd, F_GETFD);
    if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        die("fcntl failed: %s\n", strerror(errno));
    }

    return fd;
}

static int os_create_anonymous_file(off_t size) {
    static const char template[] = "/frzscr-shared-XXXXXX";

    const char *path = getenv("XDG_RUNTIME_DIR");
    if (path == NULL) {
        die("XDG_RUNTIME_DIR is unset (how?)\n");
    }

    char *name = xmalloc(strlen(path) + sizeof(template));

    strcpy(name, path);
    strcat(name, template);

    int fd = create_tmpfile_cloexec(name);

    free(name);

    if (ftruncate(fd, size) < 0) {
        die("failed to ftruncate file to %d bytes: %s\n", (int)size, strerror(errno));
    }

    return fd;
}

struct wl_buffer *create_buffer(void **data, enum wl_shm_format format,
                                uint32_t width, uint32_t height, uint32_t stride) {
    size_t size = stride * height;
    int fd = os_create_anonymous_file(size);

    *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*data == MAP_FAILED) {
        die("mmap failed: %s\n", strerror(errno));
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(wayland.shm, fd, size);
    struct wl_buffer *buff = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);

    wl_shm_pool_destroy(pool);
    close(fd);

    return buff;
}

