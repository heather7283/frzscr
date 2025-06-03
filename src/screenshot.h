#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>

#include "wayland.h"

struct screenshot {
    struct buffer buffer;
    struct output *output;

    uint32_t flags;
    enum wl_shm_format format;
    bool ready;

    struct wl_list link;
};

struct screenshot *take_screenshot(struct output *output);
void screenshot_cleanup(struct screenshot *screenshot);

#endif /* #ifndef SCREENSHOT_H */

