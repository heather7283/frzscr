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

unsigned int DEBUG_LEVEL = 0;

void print_help_and_exit(int exit_status) {
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

    fputs(help_string, stderr);
    exit(exit_status);
}

void parse_command_line(int *argc, char ***argv) {
    int opt;

    while ((opt = getopt(*argc, *argv, ":o:t:Chv")) != -1) {
        switch (opt) {
        case 'o':
            DEBUG("output name supplied on command line: %s\n", optarg);
            config.output = xstrdup(optarg);
            break;
        case 't':
            DEBUG("timeout supplied on command line: %s\n", optarg);
            int t = atoi(optarg);
            if (t < 1) {
                DIE("invalid timeout, expected unsigned int > 0, got %d\n", t);
            }
            config.timeout = t;
            break;
        case 'C':
            config.cursor = true;
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case 'v':
            DEBUG_LEVEL += 1;
            break;
        case '?':
            CRIT("unknown option: %c\n", optopt);
            print_help_and_exit(1);
            break;
        case ':':
            CRIT("missing arg for %c\n", optopt);
            print_help_and_exit(1);
            break;
        default:
            DIE("unspecified error while parsing command line options\n");
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

    DEBUG("parent args (argc = %d):\n", argc);
    for (int i = 0; i < argc; i++) {
        DEBUG("argv[%d]: %s\n", i, argv[i]);
    }

    if (config.fork_child) {
        if (child_argc < 1) {
            DIE("empty child command\n");
        }
        DEBUG("child args (argc = %d):\n", child_argc);
        for (int i = 0; i < child_argc; i++) {
            DEBUG("argv[%d]: %s\n", i, child_argv[i]);
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
            CRIT("output %s not found\n", config.output);
            exit_status = 1;
            goto cleanup;
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
            CRIT("fork() failed: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        case 0:
            // child
            if (setpgid(0, 0) < 0) {
                WARN("setpgid failed: %s, might fail to kill everyone", strerror(errno));
            }
            execvp(child_argv[0], child_argv);
            DIE("execvp() failed: %s\n", strerror(errno));
        default:
            // parent, just continue
            break;
        }
    }

    if (config.timeout > 0) {
        DEBUG("setting alarm for %d seconds\n", config.timeout);
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
        CRIT("failed to block signals: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up signalfd */
    if ((signal_fd = signalfd(-1, &mask, 0)) == -1) {
        CRIT("failed to set up signalfd: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up epoll */
    if ((epoll_fd = epoll_create1(0)) == -1) {
        CRIT("failed to set up epoll: %s\n", strerror(errno));
        goto cleanup;
    }

    struct epoll_event epoll_event;
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland.fd, &epoll_event) == -1) {
        CRIT("failed to add wayland fd to epoll list: %s\n", strerror(errno));
        goto cleanup;
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        CRIT("failed to add signal fd to epoll list: %s\n", strerror(errno));
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
            CRIT("epoll_wait error: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == wayland.fd) {
                /* wayland events */
                if (wl_display_dispatch(wayland.display) == -1) {
                    CRIT("wl_display_dispatch failed\n");
                    exit_status = 1;
                    goto cleanup;
                }
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    CRIT("failed to read signalfd_siginfo from signal_fd\n");
                    exit_status = 1;
                    goto cleanup;
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                case SIGTERM:
                    DEBUG("received signal %d, exiting\n", signo);
                    goto cleanup;
                case SIGCHLD:
                    DEBUG("received SIGCHLD\n");
                    if (child_pid < 0) {
                        CRIT("but no child was created??? wtf\n");
                        exit_status = 1;
                        goto cleanup;
                    }
                    int wstatus;
                    waitpid(child_pid, &wstatus, 0);
                    if (WIFEXITED(wstatus)) {
                        DEBUG("child exited with code %d\n", WEXITSTATUS(wstatus));
                    }
                    child_pid = -1;
                    goto cleanup;
                case SIGALRM:
                    DEBUG("received SIGALRM\n");
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (child_pid > 0) {
        DEBUG("sending signal %d to pgid %d\n", config.child_kill_signal, child_pid);
        if (kill(-child_pid, config.child_kill_signal) < 0) {
            WARN("failed to kill process group %d: %s\n", child_pid, strerror(errno));
            WARN("will try killing child only now\n");
            if (kill(child_pid, config.child_kill_signal) < 0) {
                WARN("failed to kill child with pid %d: %s\n", child_pid, strerror(errno));
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

