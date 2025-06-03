#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdbool.h>

struct config {
    char *output;
    bool fork_child;
    unsigned int timeout;
    int child_kill_signal;
    bool cursor;
};

extern struct config config;

#endif /* #ifndef CONFIG_H */

