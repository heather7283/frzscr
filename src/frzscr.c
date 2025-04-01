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
#include "window.h"
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
        "    -v               increase verbosity\n"
        "    -h               print this help message and exit\n";

    fputs(help_string, stderr);
    exit(exit_status);
}

void parse_command_line(int *argc, char ***argv) {
    int opt;

    while ((opt = getopt(*argc, *argv, ":o:t:hv")) != -1) {
        switch (opt) {
        case 'o':
            debug("output name supplied on command line: %s\n", optarg);
            config.output = xstrdup(optarg);
            break;
        case 't':
            debug("timeout supplied on command line: %s\n", optarg);
            int t = atoi(optarg);
            if (t < 1) {
                die("invalid timeout, expected unsigned int > 0, got %d\n", t);
            }
            config.timeout = t;
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case 'v':
            DEBUG_LEVEL += 1;
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
            config.fork_child = true;
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

    if (config.fork_child) {
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
    if (config.output == NULL) {
        wl_list_for_each(output, &wayland.outputs, link) {
            wl_list_insert(&screenshots, &take_screenshot(output)->link);
        }
    } else {
        bool output_found = false;
        wl_list_for_each(output, &wayland.outputs, link) {
            if (strcmp(output->name, config.output) == 0) {
                wl_list_insert(&screenshots, &take_screenshot(output)->link);
                output_found = true;
                break;
            }
        }
	if (!output_found) {
            critical("output %s not found\n", config.output);
            exit_status = 1;
            goto cleanup;
        }
    }

    struct screenshot *screenshot, *screenshot_tmp;
    wl_list_for_each(screenshot, &screenshots, link) {
        wl_list_insert(&windows, &create_window_from_screenshot(screenshot)->link);
    }

    wl_display_roundtrip(wayland.display);

    if (config.fork_child) {
        child_pid = fork();
        switch (child_pid) {
        case -1:
            critical("fork() failed: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        case 0:
            // child
            if (setpgid(0, 0) < 0) {
                warn("setpgid failed: %s, might fail to kill everyone", strerror(errno));
            }
            execvp(child_argv[0], child_argv);
            die("execvp() failed: %s\n", strerror(errno));
        default:
            // parent, just continue
            break;
        }
    }

    if (config.timeout > 0) {
        debug("setting alarm for %d seconds\n", config.timeout);
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
                    child_pid = -1;
                    goto cleanup;
                case SIGALRM:
                    debug("received SIGALRM\n");
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (child_pid > 0) {
        debug("sending signal %d to pgid %d\n", config.child_kill_signal, child_pid);
        if (kill(-child_pid, config.child_kill_signal) < 0) {
            warn("failed to kill process group %d: %s\n", child_pid, strerror(errno));
            warn("will try killing child only now\n");
            if (kill(child_pid, config.child_kill_signal) < 0) {
                warn("failed to kill child with pid %d: %s\n", child_pid, strerror(errno));
            }
        };
    }

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

