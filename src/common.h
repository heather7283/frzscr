#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h> /* exit */
#include <stdio.h> /* fprintf */

extern unsigned int DEBUG_LEVEL;

#define DEBUG(__fmt, ...) \
    do { \
        if (DEBUG_LEVEL >= 1) { \
            fprintf(stderr, "DEBUG %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define WARN(__fmt, ...) \
    do { \
        fprintf(stderr, "WARN %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define CRIT(__fmt, ...) \
    do { \
        fprintf(stderr, "CRITICAL %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define DIE(__fmt, ...) \
    do { \
        fprintf(stderr, "CRITICAL %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(1); \
    } while(0)

#endif /* #ifndef COMMON_H */

