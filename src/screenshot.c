#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "ext-image-copy-capture-v1.h"
#include "ext-image-capture-source-v1.h"
#include "wlr-screencopy-unstable-v1.h"

#include "common.h"
#include "wayland.h"
#include "screenshot.h"
#include "shm.h"
#include "config.h"
#include "xmalloc.h"
#include "utils.h"

static void frame_buffer_handler(void *data, struct zwlr_screencopy_frame_v1 *frame,
                                 uint32_t format,
                                 uint32_t width, uint32_t height, uint32_t stride) {
    struct screenshot *sshot = data;

    sshot->format = format;
    create_buffer(&sshot->buffer, format, width, height, stride);

    zwlr_screencopy_frame_v1_copy(frame, sshot->buffer.wl_buffer);
}

static void frame_flags_handler(void *data,
                                struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
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

    DIE("failed to capture screenshot");
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer = frame_buffer_handler,
    .flags = frame_flags_handler,
    .ready = frame_ready_handler,
    .failed = frame_failed_handler,
};

static void copy_capture_transform_handler(void *data,
                                           struct ext_image_copy_capture_frame_v1 *_,
                                           uint32_t transform) {
    // noop
}

static void copy_capture_damage_handler(void *data,
                                        struct ext_image_copy_capture_frame_v1 *_,
                                        int32_t x, int32_t y,
                                        int32_t width, int32_t height) {
    // noop
}

static void copy_capture_presentation_time_handler(void *data,
                                                   struct ext_image_copy_capture_frame_v1 *_,
                                                   uint32_t tv_sec_hi,
                                                   uint32_t tv_sec_lo,
                                                   uint32_t tv_nsec) {
    // noop
}

static void copy_capture_frame_ready_handler(void *data,
                                             struct ext_image_copy_capture_frame_v1 *frame) {
    struct screenshot *sshot = data;

    sshot->ready = true;
    ext_image_copy_capture_frame_v1_destroy(frame);
}

static void copy_capture_failed_handler(void *data,
                                        struct ext_image_copy_capture_frame_v1 *_,
                                        uint32_t reason) {
    struct screenshot *sshot = data;

    char *reason_str;
    switch (reason) {
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
            reason_str = "invalid buffer constraints";
            break;
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
            reason_str = "stopped";
            break;
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN:
        default:
            reason_str = "unknown reason";
    }

    DIE("failed to capture screenshot: %s", reason_str);
}

static const struct ext_image_copy_capture_frame_v1_listener image_copy_frame_listener = {
    .transform = copy_capture_transform_handler,
    .damage = copy_capture_damage_handler,
    .presentation_time = copy_capture_presentation_time_handler,
    .ready = copy_capture_frame_ready_handler,
    .failed = copy_capture_failed_handler,
};

static void session_buffer_size_handler(void *data,
                                        struct ext_image_copy_capture_session_v1 *_,
                                        uint32_t width,
                                        uint32_t height) {
    struct screenshot *sshot = data;
    DEBUG("session_buffer_size: %ux%u", width, height);
    sshot->session_width = width;
    sshot->session_height = height;
}

static void session_shm_format_handler(void *data,
                                       struct ext_image_copy_capture_session_v1 *_,
                                       uint32_t format) {
    struct screenshot *sshot = data;
    DEBUG("session_shm_format: 0x%" PRIx32, format);
    sshot->format = format;
}

static void session_dmabuf_device_handler(void *data,
                                          struct ext_image_copy_capture_session_v1 *_,
                                          struct wl_array *device) {
    // noop
}

static void session_dmabuf_format_handler(void *data,
                                          struct ext_image_copy_capture_session_v1 *_,
                                          uint32_t format,
                                          struct wl_array *modifiers) {
    // noop
}

static void session_done_handler(void *data, struct ext_image_copy_capture_session_v1 *session) {
    struct screenshot *sshot = data;
    struct ext_image_copy_capture_frame_v1 *frame =
        ext_image_copy_capture_session_v1_create_frame(session);
    ext_image_copy_capture_frame_v1_add_listener(frame, &image_copy_frame_listener, sshot);

    uint32_t format = sshot->format;
    uint32_t width = sshot->session_width;
    uint32_t height = sshot->session_height;
    uint32_t stride = width * get_bytes_per_pixel(format);

    create_buffer(&sshot->buffer, format, width, height, stride);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, sshot->buffer.wl_buffer);
    ext_image_copy_capture_frame_v1_capture(frame);
}

static void session_stopped_handler(void *data, struct ext_image_copy_capture_session_v1 *session) {
    DIE("capture session stopped");
}

static const struct ext_image_copy_capture_session_v1_listener session_listener = {
    .buffer_size = session_buffer_size_handler,
    .shm_format = session_shm_format_handler,
    .dmabuf_device = session_dmabuf_device_handler,
    .dmabuf_format = session_dmabuf_format_handler,
    .done = session_done_handler,
    .stopped = session_stopped_handler,
};

struct screenshot *take_screenshot(struct output *output) {
    struct screenshot *screenshot = xcalloc(1, sizeof(*screenshot));
    screenshot->output = output;

    if (wayland.screencopy_manager) {
        struct zwlr_screencopy_frame_v1 *frame =
            zwlr_screencopy_manager_v1_capture_output(wayland.screencopy_manager,
                                                      config.cursor,
                                                      screenshot->output->wl_output);
        zwlr_screencopy_frame_v1_add_listener(frame, &screencopy_frame_listener, screenshot);
    } else {
        struct ext_image_capture_source_v1 *source =
            ext_output_image_capture_source_manager_v1_create_source(
                wayland.output_image_capture_source_manager,
                screenshot->output->wl_output
            );

        uint32_t options = 0;
        if (config.cursor) {
            options |= EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS;
        };

        screenshot->session =
            ext_image_copy_capture_manager_v1_create_session(wayland.image_copy_capture_manager,
                                                             source, options);
        ext_image_copy_capture_session_v1_add_listener(screenshot->session,
                                                       &session_listener,
                                                       screenshot);

        ext_image_capture_source_v1_destroy(source);
        DEBUG("destroyed source");
    }

    wl_display_roundtrip(wayland.display);
    wl_display_dispatch(wayland.display);
    if (!screenshot->ready) {
        DIE("screenshot not ready after roundrip and dispatch (wtf)");
    }

    DEBUG("captured sshot of %s (logical %ix%i) size %ix%i stride %i",
          screenshot->output->name,
          screenshot->output->logical_geometry.w, screenshot->output->logical_geometry.h,
          screenshot->buffer.width, screenshot->buffer.height, screenshot->buffer.stride);

    return screenshot;
}

void screenshot_cleanup(struct screenshot *screenshot) {
    destroy_buffer(&screenshot->buffer);
    wl_list_remove(&screenshot->link);
    free(screenshot);
}

