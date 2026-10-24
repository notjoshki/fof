// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_BACKEND_H
#define FOB_BACKEND_H

typedef enum {
    FOB_LINUX_X86_64
} FOB_Architecture;

char *fob_emit_ir(const FOB_IR *ir, const char *entrypoint_name);

#endif
