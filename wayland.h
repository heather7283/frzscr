#ifndef WAYLAND_H
#define WAYLAND_H

#include <stdint.h>
#include <wayland-client.h>

struct wayland {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct wl_list outputs;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;
};
extern struct wayland wayland;

struct output_geometry {
    int32_t x, y, phys_w, phys_h;
    enum wl_output_subpixel subpixel;
    char *make, *model;
    enum wl_output_transform transform;
};

//struct output_mode {
//    enum wl_output_mode mode;
//    int32_t w, h, refresh;
//};

struct output {
    struct wl_output *wl_output;
    struct output_geometry geometry;
    //struct output_mode mode;
    char *name, *description;
    int32_t scale;
    struct wl_list link;
};

void wayland_init(void);

#endif /* #ifndef WAYLAND_H */

