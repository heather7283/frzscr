#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client.h"

#include "common.h"
#include "window.h"
#include "wayland.h"
#include "shm.h"
#include "utils.h"
#include "xmalloc.h"

#define ANCHOR_ALL \
    (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | \
    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)

struct wl_list windows;

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
                                    uint32_t serial, uint32_t width, uint32_t height) {
    struct window *window = data;
    zwlr_layer_surface_v1_ack_configure(window->layer_surface, serial);
    wl_surface_commit(window->wl_surface);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1) {
    die("layer_surface closed unexpectedly\n");
}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    layer_surface_configure,
    layer_surface_closed
};

struct window *create_window_from_screenshot(struct screenshot *screenshot) {
    struct window *window = xcalloc(1, sizeof(struct window));

    window->wl_surface = wl_compositor_create_surface(wayland.compositor);
    if (window->wl_surface == NULL) {
        die("couldn't create a wl_surface\n");
    }

    window->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        wayland.layer_shell,
        window->wl_surface,
        screenshot->output->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "frzscr"
    );
    if (window->layer_surface == NULL) {
        die("couldn't create a zwlr_layer_surface\n");
    }

    int32_t output_w = screenshot->output->logical_geometry.w;
    int32_t output_h = screenshot->output->logical_geometry.h;

    zwlr_layer_surface_v1_set_size(window->layer_surface, output_w, output_h);
    zwlr_layer_surface_v1_set_anchor(window->layer_surface, ANCHOR_ALL);
    zwlr_layer_surface_v1_set_exclusive_zone(window->layer_surface, -1);

    zwlr_layer_surface_v1_add_listener(window->layer_surface, &layer_surface_listener, window);
    wl_surface_commit(window->wl_surface);
    wl_display_roundtrip(wayland.display);

    int bytes_per_pixel = screenshot->stride / screenshot->width;

    window->wl_buffer = create_buffer(&window->data, screenshot->format,
                                      output_w, output_h, output_w * bytes_per_pixel);
    window->data_size = output_h * output_w * bytes_per_pixel;

    rotate_image(window->data, screenshot->data,
                 screenshot->width, screenshot->height,
                 bytes_per_pixel,
                 screenshot->output->transform);

    wl_surface_attach(window->wl_surface, window->wl_buffer, 0, 0);
    wl_surface_commit(window->wl_surface);

    return window;
}

void window_cleanup(struct window *window) {
    if (window->layer_surface) {
        zwlr_layer_surface_v1_destroy(window->layer_surface);
    }
    if (window->wl_surface) {
        wl_surface_destroy(window->wl_surface);
    }
    if (window->wl_buffer) {
        wl_buffer_destroy(window->wl_buffer);
    }
    if (window->data != NULL) {
        if (munmap(window->data, window->data_size) < 0) {
            warn("munmap() failed during cleanup: %s\n", strerror(errno));
        }
    }
    wl_list_remove(&window->link);
    free(window);
}

