// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_DATA_TYPE_H
#define FOB_DATA_TYPE_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    FOB_PRIM_UNKNOWN,
    FOB_PRIM_VOID,
    FOB_PRIM_BOOL,
    FOB_PRIM_CHAR,
    FOB_PRIM_I8,
    FOB_PRIM_I16,
    FOB_PRIM_I32,
    FOB_PRIM_I64,
    FOB_PRIM_ISIZE,
    FOB_PRIM_U8,
    FOB_PRIM_U16,
    FOB_PRIM_U32,
    FOB_PRIM_U64,
    FOB_PRIM_USIZE,
    FOB_PRIM_F32,
    FOB_PRIM_F64
} FOB_Primitive_Type;

typedef struct {
    FOB_Primitive_Type primitive_type;
    size_t pointers;
} FOB_Data_Type;

static inline bool fob_primitive_is_float(const FOB_Primitive_Type primitive_type) {
    return primitive_type == FOB_PRIM_F32 || primitive_type == FOB_PRIM_F64;
}

static inline bool fob_data_type_is_float(const FOB_Data_Type *data_type) {
    return data_type->pointers == 0 && fob_primitive_is_float(data_type->primitive_type);
}

static inline bool fob_primitive_is_unsigned(const FOB_Primitive_Type primitive_type) {
    return (primitive_type >= FOB_PRIM_U8 && primitive_type <= FOB_PRIM_USIZE) || primitive_type == FOB_PRIM_BOOL;
}

static inline bool fob_data_type_is_unsigned(const FOB_Data_Type *data_type) {
    return data_type->pointers == 0 && fob_primitive_is_unsigned(data_type->primitive_type);
}

static inline FOB_Data_Type fob_create_data_type(FOB_Primitive_Type primitive_type) {
    return (FOB_Data_Type){ .primitive_type = primitive_type, .pointers = 0 };
}

static inline FOB_Primitive_Type fob_data_type_to_primitive_type(const FOB_Data_Type *data_type) {
    return data_type->pointers > 0 ? FOB_PRIM_ISIZE : data_type->primitive_type;
}

char *fob_data_type_to_string(const FOB_Data_Type *data_type);

#endif
