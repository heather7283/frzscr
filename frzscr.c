#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <wayland-util.h>

#include "screenshot.h"
#include "wlr-layer-shell-unstable-v1-client.h"
#include "common.h"
#include "wayland.h"
#include "window.h"

unsigned int DEBUG_LEVEL = 1;

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface = NULL;
struct wl_shell *shell = NULL;
struct zwlr_layer_shell_v1 *layer_shell = NULL;
struct wl_shell_surface *shell_surface = NULL;
struct zwlr_layer_surface_v1 *layer_surface = NULL;
struct wl_shm *shm = NULL;
struct wl_buffer *buffer = NULL;

void *shm_data = NULL;

int main(int argc, char **argv) {
    wayland_init();

    wl_list_init(&windows);

    wl_list_init(&screenshots);
    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        wl_list_insert(&screenshots, &take_screenshot(output)->link);
    }

    struct screenshot *screenshot;
    wl_list_for_each(screenshot, &screenshots, link) {
        wl_list_insert(&windows, &create_window_from_screenshot(screenshot)->link);
    }

    while (wl_display_dispatch(wayland.display) != -1) {
    }

    wl_display_disconnect(wayland.display);
    printf("disconnected from display\n");

    exit(0);
}
