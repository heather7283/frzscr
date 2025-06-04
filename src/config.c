#include <stddef.h>
#include <signal.h>

#include "config.h"

struct config config = {
    .output = NULL,
    .fork_child = false,
    .timeout = 0,
    .child_kill_signal = SIGTERM,
    .cursor = false,
};

