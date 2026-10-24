// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_MACROS_H
#define FOB_MACROS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

void _fob_alert_increment_errors_thrown();

#define fob_alert(...) \
    do { \
        fprintf(stderr, "fob: %s: %s: ", __FILE__, __func__); \
        fprintf(stderr, __VA_ARGS__); \
        _fob_alert_increment_errors_thrown(); \
    } while (0)

bool fob_error_thrown();

#endif