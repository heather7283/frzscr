#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <wayland-util.h>

#include "screenshot.h"
#include "common.h"
#include "wayland.h"
#include "window.h"

#define EPOLL_MAX_EVENTS 16

unsigned int DEBUG_LEVEL = 1;

struct config {
    char *output;
    char fork_child;
} config = {
    .output = NULL,
    .fork_child = 'N',
};

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "frzscr - freeze screen\n"
        "\n"
        "usage:\n"
        "    frzscr [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -h               print this help message and exit\n"
        "    -o OUTPUT        only freeze this output (eg eDP-1)\n"
        "    -c CMD [ARGS...] fork CMD and wait for it to exit\n";

    fputs(help_string, stderr);
    exit(exit_status);
}

void parse_command_line(int *argc, char ***argv) {
    int opt;

    while ((opt = getopt(*argc, *argv, ":o:ch")) != -1) {
        switch (opt) {
        case 'o':
            debug("output name supplied on command line: %s\n", optarg);
            warn("output option not implemented!\n");
            config.output = strdup(optarg);
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case 'c':
            config.fork_child = 'Y';
            break;
        case '?':
            critical("unknown option: %c\n", optopt);
            print_help_and_exit(1);
            break;
        case ':':
            critical("missing arg for %c\n", optopt);
            print_help_and_exit(1);
            break;
        default:
            die("unspecified error while parsing command line options\n");
        }
    }
}

int main(int argc, char **argv) {
    int exit_status = 0;
    int signal_fd = -1;
    int epoll_fd = -1;
    pid_t child_pid = -1;

    wl_list_init(&wayland.outputs);
    wl_list_init(&windows);
    wl_list_init(&screenshots);

    int child_argc = -1;
    char **child_argv = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            config.fork_child = 'Y';
            child_argc = argc - i - 1;
            child_argv = &argv[i + 1];
            argc = i;
        }
    }

    parse_command_line(&argc, &argv);

    debug("parent args (argc = %d):\n", argc);
    for (int i = 0; i < argc; i++) {
        debug("argv[%d]: %s\n", i, argv[i]);
    }

    if (config.fork_child == 'Y') {
        if (child_argc < 1) {
            die("empty child command\n");
        }
        debug("child args (argc = %d):\n", child_argc);
        for (int i = 0; i < child_argc; i++) {
            debug("argv[%d]: %s\n", i, child_argv[i]);
        }
    }

    wayland_init();

    struct output *output;
    wl_list_for_each(output, &wayland.outputs, link) {
        wl_list_insert(&screenshots, &take_screenshot(output)->link);
    }

    struct screenshot *screenshot, *screenshot_tmp;
    wl_list_for_each(screenshot, &screenshots, link) {
        wl_list_insert(&windows, &create_window_from_screenshot(screenshot)->link);
    }

    wl_display_roundtrip(wayland.display);

    if (config.fork_child == 'Y') {
        child_pid = fork();
        switch (child_pid) {
        case -1:
            critical("fork() failed: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        case 0:
            // child
            execvp(child_argv[0], child_argv);
            die("execvp() failed: %s\n", strerror(errno));
        default:
            // parent, just continue
            break;
        }
    }

    /* block signals so we can catch them later */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        critical("failed to block signals: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up signalfd */
    if ((signal_fd = signalfd(-1, &mask, 0)) == -1) {
        critical("failed to set up signalfd: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up epoll */
    if ((epoll_fd = epoll_create1(0)) == -1) {
        critical("failed to set up epoll: %s\n", strerror(errno));
        goto cleanup;
    }

    struct epoll_event epoll_event;
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland.fd, &epoll_event) == -1) {
        critical("failed to add wayland fd to epoll list: %s\n", strerror(errno));
        goto cleanup;
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        critical("failed to add signal fd to epoll list: %s\n", strerror(errno));
        goto cleanup;
    }

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (1) {
        /* main event loop */
        do {
            number_fds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            critical("epoll_wait error: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == wayland.fd) {
                /* wayland events */
                if (wl_display_dispatch(wayland.display) == -1) {
                    critical("wl_display_dispatch failed\n");
                    exit_status = 1;
                    goto cleanup;
                }
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    critical("failed to read signalfd_siginfo from signal_fd\n");
                    exit_status = 1;
                    goto cleanup;
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                case SIGTERM:
                    debug("received signal %d, exiting\n", signo);
                    goto cleanup;
                case SIGCHLD:
                    debug("received SIGCHLD\n");
                    if (child_pid < 0) {
                        critical("but no child was created??? wtf\n");
                        exit_status = 1;
                        goto cleanup;
                    }
                    int wstatus;
                    waitpid(child_pid, &wstatus, 0);
                    if (WIFEXITED(wstatus)) {
                        debug("child exited with code %d\n", WEXITSTATUS(wstatus));
                    }
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    wl_list_for_each_safe(screenshot, screenshot_tmp, &screenshots, link) {
        screenshot_cleanup(screenshot);
    }

    struct window *window, *window_tmp;
    wl_list_for_each_safe(window, window_tmp, &windows, link) {
        window_cleanup(window);
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

