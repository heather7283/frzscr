#ifndef WINDOW_H
#define WINDOW_H

#include <wayland-util.h>

#include "screenshot.h"

struct window {
    void *data;
    struct wl_buffer *wl_buffer;
    struct wl_surface *wl_surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_list link;
};
extern struct wl_list windows;

struct window *create_window_from_screenshot(struct screenshot *screenshot);
void window_cleanup(struct window *window);

#endif /* #ifndef WINDOW_H */

