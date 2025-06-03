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
    ( ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    \
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   \
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT  )

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface,
                                    uint32_t serial, uint32_t width, uint32_t height) {
    struct window *window = data;

    zwlr_layer_surface_v1_ack_configure(window->layer_surface, serial);
    wl_surface_commit(window->wl_surface);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface) {
    die("layer_surface closed unexpectedly\n");
}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    layer_surface_configure,
    layer_surface_closed
};

struct window *create_window_from_screenshot(struct screenshot *screenshot) {
    struct window *window = xcalloc(1, sizeof(*window));

    window->wl_surface = wl_compositor_create_surface(wayland.compositor);
    if (window->wl_surface == NULL) {
        die("couldn't create a wl_surface\n");
    }

    int32_t bpp = screenshot->buffer.stride / screenshot->buffer.width;
    int32_t buf_w, buf_h, buf_stride;
    switch (screenshot->output->transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        buf_w = screenshot->buffer.width;
        buf_h = screenshot->buffer.height;
        break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        buf_w = screenshot->buffer.height;
        buf_h = screenshot->buffer.width;
        break;
    default:
        die("UNREACHABLE: wl_output_transform is %d\n", screenshot->output->transform);
    }
    buf_stride = buf_w * bpp;

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

    int bytes_per_pixel = screenshot->buffer.stride / screenshot->buffer.width;

    debug("creating buffer %ix%i stride %i\n", buf_w, buf_h, buf_stride);
    create_buffer(&window->buffer, screenshot->format, buf_w, buf_h, buf_stride);

    rotate_image(window->buffer.data, screenshot->buffer.data,
                 screenshot->buffer.width, screenshot->buffer.height,
                 bytes_per_pixel,
                 screenshot->output->transform);

    wl_surface_attach(window->wl_surface, window->buffer.wl_buffer, 0, 0);
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
    if (window->buffer.wl_buffer) {
        wl_buffer_destroy(window->buffer.wl_buffer);
    }
    if (window->buffer.data != NULL) {
        if (munmap(window->buffer.data, window->buffer.stride * window->buffer.height) < 0) {
            warn("munmap() failed during cleanup: %s\n", strerror(errno));
        }
    }
    wl_list_remove(&window->link);
    free(window);
}

