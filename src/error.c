#include "error.h"
#include <stdio.h>
#include <stdbool.h>

static size_t critical_errors = 0;
static size_t total_errors = 0;

void log_error_location(const Error_Type type) {
    if (type == ERROR_CRITICAL)
        critical_errors++;

    total_errors++;
    /*
    fprintf(stderr, "%s:", !show_file ? "fof" : fob_get_source_file(source));

    if (show_ln_col)
        fprintf(stderr, "%zu,%zu:", ln, col);

    fputc(' ', stderr);
    */
}

size_t get_total_error_count() {
    return total_errors;
}
