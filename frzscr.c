#include "screenshot.h"
#include "common.h"
#include "wayland.h"
#include "window.h"

unsigned int DEBUG_LEVEL = 1;

int main(int argc, char **argv) {
    wl_list_init(&windows);
    wl_list_init(&screenshots);

    wayland_init();

    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        wl_list_insert(&screenshots, &take_screenshot(output)->link);
    }

    struct screenshot *screenshot;
    wl_list_for_each(screenshot, &screenshots, link) {
        wl_list_insert(&windows, &create_window_from_screenshot(screenshot)->link);
    }

    while (wl_display_dispatch(wayland.display) != -1) {}

    wl_display_disconnect(wayland.display);
    info("disconnected from display\n");

    exit(0);
}

