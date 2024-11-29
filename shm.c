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
#include "window.h"

static int set_cloexec_or_close(int fd) {
    long flags;

    if (fd == -1) return -1;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1) goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) goto err;

    return fd;
err:
    close(fd);
    return -1;
}


static int create_tmpfile_cloexec(char *tmpname) {
    int fd;

#ifdef HAVE_MKOSTEMP
    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0)
        unlink(tmpname);
#else
    fd = mkstemp(tmpname);
    if (fd >= 0) {
        fd = set_cloexec_or_close(fd);
        unlink(tmpname);
    }
#endif

    return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 */
int os_create_anonymous_file(off_t size) {
    static const char template[] = "/weston-shared-XXXXXX";
    const char *path;
    char *name;
    int fd;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path) {
        errno = ENOENT;
        return -1;
    }

    name = malloc(strlen(path) + sizeof(template));
    if (!name) return -1;
    strcpy(name, path);
    strcat(name, template);

    fd = create_tmpfile_cloexec(name);

    free(name);

    if (fd < 0) return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

struct wl_buffer *create_buffer(void **data, enum wl_shm_format format,
                                int32_t width, int32_t height, int32_t stride) {
    size_t size = stride * height;

    struct wl_shm_pool *pool;
    struct wl_buffer *buff;
    int fd;

    fd = os_create_anonymous_file(size);
    if (fd < 0) {
        die("creating a buffer file for %d B failed: %m\n",	(int)size);
    }

    *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*data == MAP_FAILED) {
        die("mmap failed: %s\n", strerror(errno));
    }

    pool = wl_shm_create_pool(wayland.shm, fd, size);
    buff = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);

    wl_shm_pool_destroy(pool);

    return buff;
}

