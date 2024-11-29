#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "wlr-screencopy-unstable-v1-client.h"
#include "wlr-layer-shell-unstable-v1-client.h"
#include "wayland.h"
#include "window.h"
#include "common.h"

struct wayland wayland = {0};
struct window window = {0};

static void output_geometry_handler(void *data, struct wl_output *wl_output, int32_t x, int32_t y,
                                    int32_t phys_width, int32_t phys_height, int32_t subpixel,
                                    const char *make, const char *model, int32_t transform) {
    struct output *output = data;
    output->geometry.x = x;
    output->geometry.y = y;
    output->geometry.phys_w = phys_width;
    output->geometry.phys_h = phys_height;
    output->geometry.subpixel = subpixel;
    output->geometry.make = strdup(make);
    output->geometry.model = strdup(model);
    output->geometry.transform = transform;
}

static void output_mode_handler(void *data, struct wl_output *wl_output, uint32_t flags,
                                int32_t width, int32_t height, int32_t refresh) {
    // I'll ignore this for now, too much work (maybe I won't need it :xdd:)
}

static void output_scale_handler(void *data, struct wl_output *wl_output, int32_t factor) {
    struct output *output = data;
    output->scale = factor;
}

static void output_name_handler(void *data, struct wl_output *wl_output, const char *name) {
    struct output *output = data;
    output->name = strdup(name);
}

static void output_description_handler(void *data, struct wl_output *wl_output,
                                       const char *description) {
    struct output *output = data;
    output->description = strdup(description);
}

static void output_done_handler(void *data, struct wl_output *wl_output) {
	// Nothing to do here? idk
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry_handler,
	.mode = output_mode_handler,
	.scale = output_scale_handler,
    .name = output_name_handler,
    .description = output_description_handler,
	.done = output_done_handler,
};

//static void shm_format_handler(void *data, struct wl_shm *wl_shm, uint32_t format) {
//    // TODO
//}
//
//static struct wl_shm_listener shm_listener = {
//	shm_format_handler
//};

static void registry_global(void *data, struct wl_registry *registry, uint32_t id,
	                        const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wayland.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wayland.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        //wl_shm_add_listener(wayland.shm, &shm_listener, NULL);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output *output = malloc(sizeof(struct output));
        if (output == NULL) {
            die("malloc failed\n");
        }
        wl_list_insert(&wayland.outputs, &output->link);
        output->wl_output = wl_registry_bind(registry, id, &wl_output_interface, 4);
        wl_output_add_listener(output->wl_output, &output_listener, output);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        wayland.layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        wayland.screencopy_manager = wl_registry_bind(registry, id,
                                                      &zwlr_screencopy_manager_v1_interface, 1);
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
    if (wl_list_empty(&wayland.outputs)) {
        die("no outputs found\n");
    }

    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        printf("Output %s (%s)\n", output->name, output->description);
    }
}

