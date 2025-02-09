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
    struct window *window = calloc(1, sizeof(struct window));
    if (window == NULL) {
        die("failed to allocate memory for window struct\n");
    }

    window->wl_surface = wl_compositor_create_surface(wayland.compositor);
    if (window->wl_surface == NULL) {
        die("couldn't create a wl_surface\n");
    }

    int32_t bpp = screenshot->stride / screenshot->width;
    int32_t buf_w, buf_h, buf_stride;
    switch (screenshot->output->transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
        buf_w = screenshot->output->logical_geometry.w;
        buf_h = screenshot->output->logical_geometry.h;
        buf_stride = buf_h * bpp;
        break;
    case WL_OUTPUT_TRANSFORM_90:
        buf_w = screenshot->output->logical_geometry.h;
        buf_h = screenshot->output->logical_geometry.w;
        buf_stride = buf_w * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_90);
        break;
    case WL_OUTPUT_TRANSFORM_180:
        buf_w = screenshot->output->logical_geometry.w;
        buf_h = screenshot->output->logical_geometry.h;
        buf_stride = buf_h * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_180);
        break;
    case WL_OUTPUT_TRANSFORM_270:
        buf_w = screenshot->output->logical_geometry.h;
        buf_h = screenshot->output->logical_geometry.w;
        buf_stride = buf_w * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_270);
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
        buf_w = screenshot->output->logical_geometry.w;
        buf_h = screenshot->output->logical_geometry.h;
        buf_stride = buf_h * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_FLIPPED);
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        buf_w = screenshot->output->logical_geometry.h;
        buf_h = screenshot->output->logical_geometry.w;
        buf_stride = buf_w * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_FLIPPED_90);
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        buf_w = screenshot->output->logical_geometry.w;
        buf_h = screenshot->output->logical_geometry.h;
        buf_stride = buf_h * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_FLIPPED_180);
        break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        buf_w = screenshot->output->logical_geometry.h;
        buf_h = screenshot->output->logical_geometry.w;
        buf_stride = buf_w * bpp;
        wl_surface_set_buffer_transform(window->wl_surface, WL_OUTPUT_TRANSFORM_FLIPPED_270);
        break;
    default:
        die("UNREACHABLE: wl_output_transform is %d\n", screenshot->output->transform);
    }

    window->wl_buffer = create_buffer(&window->data, screenshot->format,
                                      buf_w, buf_h, buf_stride);
    window->data_size = buf_h * buf_w * bpp;

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

    zwlr_layer_surface_v1_set_size(window->layer_surface,
                                   screenshot->output->logical_geometry.w,
                                   screenshot->output->logical_geometry.h);
    zwlr_layer_surface_v1_set_anchor(window->layer_surface, ANCHOR_ALL);
    zwlr_layer_surface_v1_set_exclusive_zone(window->layer_surface, -1);

    zwlr_layer_surface_v1_add_listener(window->layer_surface, &layer_surface_listener, window);
    wl_surface_commit(window->wl_surface);
    wl_display_roundtrip(wayland.display);

    wl_surface_attach(window->wl_surface, screenshot->wl_buffer, 0, 0);
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

