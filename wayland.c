#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "wlr-screencopy-unstable-v1-client.h"
#include "wlr-layer-shell-unstable-v1-client.h"
#include "xdg-output-unstable-v1-client.h"

#include "wayland.h"
#include "window.h"
#include "common.h"

struct wayland wayland = {0};
struct window window = {0};

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
	output->name = strdup(name);
}

static void xdg_output_description_handler(void *data, struct zxdg_output_v1 *xdg_output,
                                           const char *description) {
	struct output *output = data;
	output->description = strdup(description);
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

static void registry_global(void *data, struct wl_registry *registry, uint32_t id,
	                        const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wayland.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wayland.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output *output = malloc(sizeof(struct output));
        if (output == NULL) {
            die("malloc failed\n");
        }
        wl_list_insert(&wayland.outputs, &output->link);
        output->wl_output = wl_registry_bind(registry, id, &wl_output_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        wayland.layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        wayland.screencopy_manager = wl_registry_bind(registry, id,
                                                      &zwlr_screencopy_manager_v1_interface, 1);
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        wayland.xdg_output_manager = wl_registry_bind(registry, id,
                                                      &zxdg_output_manager_v1_interface, 2);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t id) {
    //debug("got a registry global remove event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove
};

void wayland_init(void) {
    wl_list_init(&wayland.outputs);

    wayland.display = wl_display_connect(NULL);
    if (wayland.display == NULL) {
        die("unable to connect to wayland compositor\n");
    }
    //debug("connected to wl_display\n");

    wayland.registry = wl_display_get_registry(wayland.display);
    if (wayland.registry == NULL) {
        die("registry is NULL\n");
    }
    //debug("got wl_registry\n");
    wl_registry_add_listener(wayland.registry, &registry_listener, NULL);

    wl_display_dispatch(wayland.display);
    wl_display_roundtrip(wayland.display);

    if (wayland.compositor == NULL) {
        die("didn't get a wl_compositor\n");
    }
    if (wayland.shm == NULL) {
        die("didn't get a wl_shm\n");
    }
    if (wayland.layer_shell == NULL) {
        die("didn't get a zwlr_layer_shell_v1\n");
    }
    if (wayland.xdg_output_manager == NULL) {
        die("didn't get a xdg_output_manager\n");
    }
    if (wl_list_empty(&wayland.outputs)) {
        die("no outputs found\n");
    }

    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        output->xdg_output =
            zxdg_output_manager_v1_get_xdg_output(wayland.xdg_output_manager, output->wl_output);
        zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output);
    }
    if (wl_display_roundtrip(wayland.display) < 0) {
        die("wl_display_roundtrip() failed\n");
    }
}

