#ifndef WINDOW_H
#define WINDOW_H

#include <wayland-util.h>

#include "screenshot.h"

struct overlay {
    struct buffer buffer;
    struct wl_surface *wl_surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wp_viewport *viewport;

    struct wl_list link;
};

struct overlay *create_overlay_from_screenshot(struct screenshot *screenshot);
void overlay_cleanup(struct overlay *overlay);

#endif /* #ifndef WINDOW_H */

