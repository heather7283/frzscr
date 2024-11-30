#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "wlr-screencopy-unstable-v1-client.h"

#include "common.h"
#include "wayland.h"
#include "screenshot.h"
#include "shm.h"

struct wl_list screenshots;

static void frame_buffer_handler(void *data, struct zwlr_screencopy_frame_v1 *frame,
                                 uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
    struct screenshot *sshot = data;
    sshot->format = format;
    sshot->width  = width;
    sshot->height = height;
    sshot->stride = stride;

    sshot->wl_buffer = create_buffer(&sshot->data, format, width, height, stride);

    zwlr_screencopy_frame_v1_copy(frame, sshot->wl_buffer);
}

static void frame_flags_handler(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
    struct screenshot *sshot = data;
    sshot->flags = flags;
}

static void frame_ready_handler(void *data, struct zwlr_screencopy_frame_v1 *frame,
                                uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
    struct screenshot *sshot = data;
    sshot->ready = true;
    zwlr_screencopy_frame_v1_destroy(frame);
}

static void frame_failed_handler(void *data, struct zwlr_screencopy_frame_v1 *frame) {
    struct screenshot *sshot = data;
    die("failed to capture screenshot\n");
}

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = frame_buffer_handler,
    .flags = frame_flags_handler,
    .ready = frame_ready_handler,
    .failed = frame_failed_handler,
};

struct screenshot *take_screenshot(struct output *output) {
    struct screenshot *screenshot = calloc(1, sizeof(struct screenshot));
    if (screenshot == NULL) {
        die("failed to calloc() screenshot struct: %s\n", strerror(errno));
    }
    screenshot->output = output;

    struct zwlr_screencopy_frame_v1 *frame =
        zwlr_screencopy_manager_v1_capture_output_region(wayland.screencopy_manager, 0,
                                                         screenshot->output->wl_output, 0, 0,
                                                         screenshot->output->logical_geometry.w,
                                                         screenshot->output->logical_geometry.h);
    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, screenshot);

    wl_display_roundtrip(wayland.display);
    wl_display_dispatch(wayland.display);
    if (!screenshot->ready) {
        die("screenshot not ready after roundrip and dispatch (wtf)\n");
    }

    debug("captured sshot of %s (%dx%d) size %dx%d stride %d\n",
          screenshot->output->name,
          screenshot->output->logical_geometry.w, screenshot->output->logical_geometry.h,
          screenshot->width, screenshot->height, screenshot->stride);

    return screenshot;
}

void screenshot_cleanup(struct screenshot *screenshot) {
    wl_buffer_destroy(screenshot->wl_buffer);
    if (munmap(screenshot->data, screenshot->stride * screenshot->height) < 0) {
        warn("munmap() failed during cleanup: %s\n", strerror(errno));
    }
    wl_list_remove(&screenshot->link);
    free(screenshot);
}

