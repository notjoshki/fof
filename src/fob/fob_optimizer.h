// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_OPTIMIZER_H
#define FOB_OPTIMIZER_H

#include "fob_ir.h"

typedef enum {
    FOB_OPTIMIZATION_NONE,
    FOB_OPTIMIZATION_GENERIC,
    FOB_OPTIMIZATION_ALL
} FOB_Optimization_Level;

void fob_optimize_ir(FOB_IR *ir, const FOB_Optimization_Level level);

#endif