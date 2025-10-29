#ifndef WAYLAND_H
#define WAYLAND_H

#include <stdint.h>
#include <wayland-client.h>

struct wayland {
    int fd;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;
    struct ext_image_copy_capture_manager_v1 *image_copy_capture_manager;
    struct ext_output_image_capture_source_manager_v1 *output_image_capture_source_manager;
    struct zxdg_output_manager_v1 *xdg_output_manager;
    struct wp_viewporter *viewporter;

    struct wl_list outputs;
    struct wl_list overlays;
    struct wl_list screenshots;
};
extern struct wayland wayland;

struct buffer {
    struct wl_buffer *wl_buffer;
    void *data;
    int32_t width, height, stride;
};

struct output {
    struct wl_output *wl_output;
    struct zxdg_output_v1 *xdg_output;
    struct {
        int32_t x, y, w, h;
    } logical_geometry;
    enum wl_output_transform transform;
    char *name;

    struct wl_list link;
};

void wayland_init(void);
void wayland_cleanup(void);

#endif /* #ifndef WAYLAND_H */
