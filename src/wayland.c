#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "wlr-screencopy-unstable-v1.h"
#include "wlr-layer-shell-unstable-v1.h"
#include "xdg-output-unstable-v1.h"
#include "ext-image-copy-capture-v1.h"
#include "ext-image-capture-source-v1.h"
#include "viewporter.h"

#include "wayland.h"
#include "common.h"
#include "xmalloc.h"

struct wayland wayland = {0};

static void xdg_output_logical_position_handler(void *data, struct zxdg_output_v1 *xdg_output,
                                                int32_t x, int32_t y) {
	struct output *output = data;

	output->logical_geometry.x = x;
	output->logical_geometry.y = y;
}

static void xdg_output_logical_size_handler(void *data, struct zxdg_output_v1 *xdg_output,
                                            int32_t width, int32_t height) {
	struct output *output = data;

	output->logical_geometry.w = width;
	output->logical_geometry.h = height;
}

static void xdg_output_name_handler(void *data, struct zxdg_output_v1 *xdg_output,
                                    const char *name) {
	struct output *output = data;

	output->name = xstrdup(name);
}

static void xdg_output_description_handler(void *data, struct zxdg_output_v1 *xdg_output,
                                           const char *description) {
    // no-op
}

static void xdg_output_done_handler(void *data, struct zxdg_output_v1 *xdg_output) {
	// no-op
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_logical_position_handler,
	.logical_size = xdg_output_logical_size_handler,
	.name = xdg_output_name_handler,
	.description = xdg_output_description_handler,
	.done = xdg_output_done_handler,
};

static void output_geometry_handler(void *data, struct wl_output *wl_output,
                                    int32_t x, int32_t y, int32_t phys_w, int32_t phys_h,
                                    int32_t subpixel,
                                    const char *make, const char *model,
                                    int32_t transform) {
    struct output *output = data;

    output->transform = transform;
}

static void output_mode_handler(void *data, struct wl_output *wl_output, uint32_t flags,
                                int32_t width, int32_t height, int32_t refresh) {
    // no-op
}

static void output_done_handler(void *data, struct wl_output *wl_output) {
    // no-op
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry_handler,
    .mode = output_mode_handler,
    .done = output_done_handler,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t id,
	                        const char *interface, uint32_t version) {
    #define MATCH_INTERFACE(i) STREQ(interface, i.name)
    #define BIND_INTERFACE(i, ver) (wl_registry_bind(registry, id, &i, ver))

    if (MATCH_INTERFACE(wl_compositor_interface)) {
        wayland.compositor = BIND_INTERFACE(wl_compositor_interface, 6);
    } else if (MATCH_INTERFACE(wl_shm_interface)) {
        wayland.shm = BIND_INTERFACE(wl_shm_interface, 1);
    } else if (MATCH_INTERFACE(wl_output_interface)) {
        struct output *output = xcalloc(1, sizeof(*output));

        wl_list_insert(&wayland.outputs, &output->link);
        output->wl_output = BIND_INTERFACE(wl_output_interface, 1);
        wl_output_add_listener(output->wl_output, &output_listener, output);
    } else if (MATCH_INTERFACE(zwlr_layer_shell_v1_interface)) {
        wayland.layer_shell = BIND_INTERFACE(zwlr_layer_shell_v1_interface, 1);
    } else if (MATCH_INTERFACE(zwlr_screencopy_manager_v1_interface)) {
        wayland.screencopy_manager = BIND_INTERFACE(zwlr_screencopy_manager_v1_interface, 1);
    } else if (MATCH_INTERFACE(zxdg_output_manager_v1_interface)) {
        wayland.xdg_output_manager = BIND_INTERFACE(zxdg_output_manager_v1_interface, 2);
    } else if (MATCH_INTERFACE(wp_viewporter_interface)) {
        wayland.viewporter = BIND_INTERFACE(wp_viewporter_interface, 1);
    } else if (MATCH_INTERFACE(ext_image_copy_capture_manager_v1_interface)) {
        wayland.image_copy_capture_manager = BIND_INTERFACE(ext_image_copy_capture_manager_v1_interface, 1);
    } else if (MATCH_INTERFACE(ext_output_image_capture_source_manager_v1_interface)) {
        wayland.output_image_capture_source_manager = BIND_INTERFACE(ext_output_image_capture_source_manager_v1_interface, 1);
    }

    #undef MATCH_INTERFACE
    #undef BIND_INTERFACE
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t id) {
    // no-op
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove
};

void wayland_init(void) {
    wl_list_init(&wayland.outputs);
    wl_list_init(&wayland.overlays);
    wl_list_init(&wayland.screenshots);

    wayland.display = wl_display_connect(NULL);
    if (wayland.display == NULL) {
        DIE("unable to connect to wayland compositor");
    }
    wayland.fd = wl_display_get_fd(wayland.display);

    wayland.registry = wl_display_get_registry(wayland.display);
    if (wayland.registry == NULL) {
        DIE("registry is NULL");
    }
    wl_registry_add_listener(wayland.registry, &registry_listener, NULL);

    wl_display_dispatch(wayland.display);
    wl_display_roundtrip(wayland.display);

    if (wayland.compositor == NULL) {
        DIE("didn't get a wl_compositor");
    }
    if (wayland.shm == NULL) {
        DIE("didn't get a wl_shm");
    }
    if (wayland.screencopy_manager == NULL && wayland.image_copy_capture_manager == NULL) {
        DIE("didn't get zwlr_screencopy_manager_v1 or ext_image_copy_capture_manager_v1");
    }
    if (wayland.image_copy_capture_manager != NULL && wayland.output_image_capture_source_manager == NULL) {
        DIE("got ext_image_copy_capture_manager_v1 but didn't get ext_output_image_capture_source_manager_v1");
    }
    if (wayland.layer_shell == NULL) {
        DIE("didn't get a zwlr_layer_shell_v1");
    }
    if (wayland.xdg_output_manager == NULL) {
        DIE("didn't get a xdg_output_manager");
    }
    if (wayland.viewporter == NULL) {
        DIE("didn't get a viewporter");
    }
    if (wl_list_empty(&wayland.outputs)) {
        DIE("no outputs found");
    }

    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        output->xdg_output =
            zxdg_output_manager_v1_get_xdg_output(wayland.xdg_output_manager, output->wl_output);
        zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output);
    }
    if (wl_display_roundtrip(wayland.display) < 0) {
        DIE("wl_display_roundtrip() failed");
    }
}

void wayland_cleanup(void) {
    struct output *output, *output_tmp;
    wl_list_for_each_safe(output, output_tmp, &wayland.outputs, link) {
        if (output->xdg_output) {
            zxdg_output_v1_destroy(output->xdg_output);
        }
        if (output->wl_output) {
            wl_output_destroy(output->wl_output);
        }
        free(output->name);
        wl_list_remove(&output->link);
        free(output);
    }
    if (wayland.viewporter) {
        wp_viewporter_destroy(wayland.viewporter);
    }
    if (wayland.xdg_output_manager) {
        zxdg_output_manager_v1_destroy(wayland.xdg_output_manager);
    }
    if (wayland.screencopy_manager) {
        zwlr_screencopy_manager_v1_destroy(wayland.screencopy_manager);
    }
    if (wayland.image_copy_capture_manager) {
        ext_image_copy_capture_manager_v1_destroy(wayland.image_copy_capture_manager);
    }
    if (wayland.output_image_capture_source_manager) {
        ext_output_image_capture_source_manager_v1_destroy(wayland.output_image_capture_source_manager);
    }
    if (wayland.layer_shell) {
        zwlr_layer_shell_v1_destroy(wayland.layer_shell);
    }
    if (wayland.shm) {
        wl_shm_destroy(wayland.shm);
    }
    if (wayland.compositor) {
        wl_compositor_destroy(wayland.compositor);
    }
    if (wayland.registry) {
        wl_registry_destroy(wayland.registry);
    }
    if (wayland.display) {
        wl_display_flush(wayland.display);
        wl_display_disconnect(wayland.display);
    }
}
