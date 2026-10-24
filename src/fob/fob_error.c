// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#include "fob_error.h"
#include <stdio.h>
#include <stdbool.h>

static size_t errors_thrown = 0;

void _fob_alert_increment_errors_thrown() {
    errors_thrown++;
}

bool fob_error_thrown() {
    return errors_thrown > 0;
}