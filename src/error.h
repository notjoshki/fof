#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#define ERROR_NO_FILE NULL
#define ERROR_NO_LN 0
#define ERROR_NO_COL 0

#define log_error(type, ...) \
    do { \
        log_error_location(ERROR_CRITICAL); \
        fprintf(stderr, "fof: Error: "); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define log_compiler_error(...) \
    do { \
        fprintf(stderr, "COMPILER ERROR: "); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

typedef enum {
    ERROR_CRITICAL,
    ERROR_WARNING
} Error_Type;

void log_error_location(const Error_Type type);
size_t get_total_error_count();

#endif
