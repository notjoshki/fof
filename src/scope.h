#ifndef SCOPE_H
#define SCOPE_H

#include <stdio.h>

typedef struct {
    size_t function_uid;
    size_t scope_uid;
    size_t indent_level;
} Scope;

static inline Scope create_scope(size_t function_uid, size_t scope_uid, size_t indent_level) {
    return (Scope){ .function_uid = function_uid, .scope_uid = scope_uid, .indent_level = indent_level };
}

#endif