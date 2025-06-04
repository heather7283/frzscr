#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client.h"
#include "viewporter-client.h"

#include "common.h"
#include "overlay.h"
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
    struct overlay *overlay = data;

    zwlr_layer_surface_v1_ack_configure(overlay->layer_surface, serial);
    wl_surface_commit(overlay->wl_surface);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface) {
    DIE("layer_surface closed unexpectedly");
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    layer_surface_configure,
    layer_surface_closed
};

static void surface_enter(void *data, struct wl_surface *surface, struct wl_output *output) {
    // no-op
}

static void surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
    // no-op
}

static void surface_preferred_buffer_scale(void *data, struct wl_surface *surface,
                                           int32_t factor) {
    // no-op
}

static void surface_preferred_buffer_transform(void *data, struct wl_surface *surface,
                                               uint32_t transform) {
    // no-op
}

static const struct wl_surface_listener surface_listener = {
    .enter = surface_enter,
    .leave = surface_leave,
    .preferred_buffer_scale = surface_preferred_buffer_scale,
    .preferred_buffer_transform = surface_preferred_buffer_transform,
};

struct overlay *create_overlay_from_screenshot(struct screenshot *screenshot) {
    struct overlay *overlay = xcalloc(1, sizeof(*overlay));

    overlay->wl_surface = wl_compositor_create_surface(wayland.compositor);
    if (overlay->wl_surface == NULL) {
        DIE("couldn't create a wl_surface");
    }
    wl_surface_add_listener(overlay->wl_surface, &surface_listener, overlay);

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
        DIE("UNREACHABLE: wl_output_transform is %d", screenshot->output->transform);
    }
    buf_stride = buf_w * bpp;

    overlay->viewport = wp_viewporter_get_viewport(wayland.viewporter, overlay->wl_surface);
    if (overlay->viewport == NULL) {
        DIE("could not create viewport");
    }
    wp_viewport_set_destination(overlay->viewport,
                                screenshot->output->logical_geometry.w,
                                screenshot->output->logical_geometry.h);
    wp_viewport_set_source(overlay->viewport,
                           wl_fixed_from_int(0), wl_fixed_from_int(0),
                           wl_fixed_from_int(buf_w), wl_fixed_from_int(buf_h));

    overlay->layer_surface =
        zwlr_layer_shell_v1_get_layer_surface(wayland.layer_shell,
                                              overlay->wl_surface,
                                              screenshot->output->wl_output,
                                              ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
                                              "frzscr");
    if (overlay->layer_surface == NULL) {
        DIE("couldn't create a zwlr_layer_surface");
    }
    zwlr_layer_surface_v1_add_listener(overlay->layer_surface, &layer_surface_listener, overlay);

    int32_t output_w = screenshot->output->logical_geometry.w;
    int32_t output_h = screenshot->output->logical_geometry.h;

    zwlr_layer_surface_v1_set_size(overlay->layer_surface, output_w, output_h);
    zwlr_layer_surface_v1_set_anchor(overlay->layer_surface, ANCHOR_ALL);
    zwlr_layer_surface_v1_set_exclusive_zone(overlay->layer_surface, -1);

    wl_surface_commit(overlay->wl_surface);
    wl_display_roundtrip(wayland.display);

    int bytes_per_pixel = screenshot->buffer.stride / screenshot->buffer.width;

    DEBUG("creating buffer %ix%i stride %i", buf_w, buf_h, buf_stride);
    create_buffer(&overlay->buffer, screenshot->format, buf_w, buf_h, buf_stride);

    rotate_image(overlay->buffer.data, screenshot->buffer.data,
                 screenshot->buffer.width, screenshot->buffer.height,
                 bytes_per_pixel,
                 screenshot->output->transform);

    wl_surface_attach(overlay->wl_surface, overlay->buffer.wl_buffer, 0, 0);
    wl_surface_commit(overlay->wl_surface);

    return overlay;
}

void overlay_cleanup(struct overlay *overlay) {
    if (overlay->layer_surface) {
        zwlr_layer_surface_v1_destroy(overlay->layer_surface);
    }
    if (overlay->viewport) {
        wp_viewport_destroy(overlay->viewport);
    }
    if (overlay->wl_surface) {
        wl_surface_destroy(overlay->wl_surface);
    }
    destroy_buffer(&overlay->buffer);
    wl_list_remove(&overlay->link);
    free(overlay);
}

