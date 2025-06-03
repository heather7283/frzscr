#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <wayland-util.h>

#include "screenshot.h"
#include "common.h"
#include "wayland.h"
#include "overlay.h"
#include "config.h"
#include "xmalloc.h"

#define EPOLL_MAX_EVENTS 16

int log_enable_debug = 0;

void print_help_and_exit(FILE *stream, int exit_status) {
    const char *help_string =
        "frzscr - freeze screen\n"
        "\n"
        "usage:\n"
        "    frzscr [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -o OUTPUT        only freeze this output (eg eDP-1)\n"
        "    -t TIMEOUT       kill child (with -c) and exit after TIMEOUT seconds\n"
        "    -c CMD [ARGS...] fork CMD and wait for it to exit\n"
        "                     all arguments after -c are treated as ARGS to CMD\n"
        "    -C               include cursor in overlay\n"
        "    -v               increase verbosity\n"
        "    -h               print this help message and exit\n";

    fputs(help_string, stream);
    exit(exit_status);
}

void parse_command_line(int *argc, char ***argv) {
    int opt;

    while ((opt = getopt(*argc, *argv, "o:t:Chv")) != -1) {
        switch (opt) {
        case 'o':
            DEBUG("output name supplied on command line: %s", optarg);
            config.output = xstrdup(optarg);
            break;
        case 't':
            DEBUG("timeout supplied on command line: %s", optarg);
            int t = atoi(optarg);
            if (t < 1) {
                DIE("invalid timeout, expected unsigned int > 0, got %d", t);
            }
            config.timeout = t;
            break;
        case 'C':
            config.cursor = true;
            break;
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case 'v':
            log_enable_debug = 1;
            break;
        case '?':
        default:
            print_help_and_exit(stderr, 1);
            break;
        }
    }
}

int main(int argc, char **argv) {
    int exit_status = 0;
    int signal_fd = -1;
    int epoll_fd = -1;
    pid_t child_pid = -1;

    wl_list_init(&wayland.outputs);
    wl_list_init(&wayland.overlays);
    wl_list_init(&wayland.screenshots);

    int child_argc = -1;
    char **child_argv = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            config.fork_child = true;
            child_argc = argc - i - 1;
            child_argv = &argv[i + 1];
            argc = i;
        }
    }

    parse_command_line(&argc, &argv);

    DEBUG("parent args (argc = %d):", argc);
    for (int i = 0; i < argc; i++) {
        DEBUG("argv[%d]: %s", i, argv[i]);
    }

    if (config.fork_child) {
        if (child_argc < 1) {
            DIE("empty child command");
        }
        DEBUG("child args (argc = %d):", child_argc);
        for (int i = 0; i < child_argc; i++) {
            DEBUG("argv[%d]: %s", i, child_argv[i]);
        }
    }

    wayland_init();

    struct output *output;
    if (config.output == NULL) {
        wl_list_for_each(output, &wayland.outputs, link) {
            wl_list_insert(&wayland.screenshots, &take_screenshot(output)->link);
        }
    } else {
        bool output_found = false;
        wl_list_for_each(output, &wayland.outputs, link) {
            if (strcmp(output->name, config.output) == 0) {
                wl_list_insert(&wayland.screenshots, &take_screenshot(output)->link);
                output_found = true;
                break;
            }
        }
        if (!output_found) {
            DIE("output %s not found", config.output);
        }
    }

    struct screenshot *screenshot, *screenshot_tmp;
    wl_list_for_each(screenshot, &wayland.screenshots, link) {
        wl_list_insert(&wayland.overlays, &create_overlay_from_screenshot(screenshot)->link);
    }

    wl_display_roundtrip(wayland.display);

    if (config.fork_child) {
        child_pid = fork();
        switch (child_pid) {
        case -1:
            EDIE("fork() failed");
        case 0:
            // child
            if (setpgid(0, 0) < 0) {
                EWARN("setpgid failed, might fail to kill all children");
            }
            execvp(child_argv[0], child_argv);
            EDIE("execvp() failed");
        default:
            // parent, just continue
            break;
        }
    }

    if (config.timeout > 0) {
        DEBUG("setting alarm for %d seconds", config.timeout);
        alarm(config.timeout);
    }

    /* block signals so we can catch them later */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        EDIE("failed to block signals");
    }

    /* set up signalfd */
    if ((signal_fd = signalfd(-1, &mask, 0)) == -1) {
        EDIE("failed to set up signalfd");
    }

    /* set up epoll */
    if ((epoll_fd = epoll_create1(0)) == -1) {
        EDIE("failed to set up epoll");
    }

    struct epoll_event epoll_event = {0};
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland.fd, &epoll_event) == -1) {
        EDIE("failed to add wayland fd to epoll list");
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        EDIE("failed to add signal fd to epoll list");
    }

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (1) {
        /* main event loop */
        do {
            number_fds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait can fail with EINTR */

        if (number_fds == -1) {
            EDIE("epoll_wait error");
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == wayland.fd) {
                /* wayland events */
                if (wl_display_dispatch(wayland.display) == -1) {
                    EDIE("wl_display_dispatch failed");
                }
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    EDIE("failed to read signalfd_siginfo from signal_fd");
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                case SIGTERM:
                    DEBUG("received signal %d, exiting", signo);
                    goto cleanup;
                case SIGCHLD:
                    DEBUG("received SIGCHLD");
                    if (child_pid < 0) {
                        WARN("received SIGCHLD but no child was created, ignoring");
                        break;
                    }

                    int wstatus;
                    waitpid(child_pid, &wstatus, 0);
                    if (WIFEXITED(wstatus)) {
                        DEBUG("child exited with code %d", WEXITSTATUS(wstatus));
                    }
                    child_pid = -1;
                    goto cleanup;
                case SIGALRM:
                    DEBUG("received SIGALRM");
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (child_pid > 0) {
        DEBUG("sending signal %d to pgid %d", config.child_kill_signal, child_pid);
        if (kill(-child_pid, config.child_kill_signal) < 0) {
            EWARN("failed to kill process group %d", child_pid);
            WARN("will try killing child only now");
            if (kill(child_pid, config.child_kill_signal) < 0) {
                EWARN("failed to kill child with pid %d", child_pid);
            }
        };
    }

    wl_list_for_each_safe(screenshot, screenshot_tmp, &wayland.screenshots, link) {
        screenshot_cleanup(screenshot);
    }

    struct overlay *overlay, *overlay_tmp;
    wl_list_for_each_safe(overlay, overlay_tmp, &wayland.overlays, link) {
        overlay_cleanup(overlay);
    }

    wayland_cleanup();

    if (epoll_fd > 0) {
        close(epoll_fd);
    }
    if (signal_fd > 0) {
        close(signal_fd);
    }

    exit(exit_status);
}

