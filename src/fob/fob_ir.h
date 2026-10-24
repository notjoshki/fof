// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_H
#define FOB_H

#include "fob_data_type.h"
#include "fob_ast.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define FOB_RETURN_REGISTER_1 0

#define FOB_TEMPORARY_IR_REGISTER_1 0
#define FOB_TEMPORARY_IR_REGISTER_2 1

typedef enum {
    FOB_OP_NOP,
    FOB_OP_EOF,
    FOB_OP_FUNC_START,
    FOB_OP_FUNC_END,
    FOB_OP_RETURN,
    FOB_OP_LOAD_CONST,
    FOB_OP_LOAD_MEMORY,
    FOB_OP_NEW_LOCAL_VARIABLE,
    FOB_OP_STORE_MEMORY,
    FOB_OP_PUSH,
    FOB_OP_POP,
    FOB_OP_AND,
    FOB_OP_OR,
    FOB_OP_XOR,
    FOB_OP_NOT,
    FOB_OP_BOOL_NOT,
    FOB_OP_ADD,
    FOB_OP_SUB,
    FOB_OP_MUL,
    FOB_OP_DIV,
    FOB_OP_MOD,
    FOB_OP_SHL,
    FOB_OP_SHR,
    FOB_OP_NEG,
    FOB_OP_CMP,
    FOB_OP_CMP_EQ,
    FOB_OP_CMP_NEQ,
    FOB_OP_CMP_LT,
    FOB_OP_CMP_LTE,
    FOB_OP_CMP_GT,
    FOB_OP_CMP_GTE,
    FOB_OP_JMP,
    FOB_OP_JMP_TRUE,
    FOB_OP_JMP_FALSE,
    FOB_OP_NEW_LABEL,
    FOB_OP_INLINE_ASM,
    FOB_OP_BLOCK_START,
    FOB_OP_BLOCK_END,
    FOB_OP_REF,
    FOB_OP_CALL,
    FOB_OP_SPILL_ARG,
    FOB_OP_UNSPILL_ARG
} FOB_Opcode;

typedef enum {
    FOB_OPER_NONE,
    FOB_OPER_INT,
    FOB_OPER_FLOAT,
    FOB_OPER_STRING,
    FOB_OPER_REGISTER,
    FOB_OPER_IDENTIFIER,
    FOB_OPER_VARIABLE,
    FOB_OPER_LABEL,
    FOB_OPER_ARGUMENT,
    FOB_OPER_DEREF
} FOB_Operand_Type;

typedef union {
    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
} FOB_Operand_Int_Const;

typedef struct {
    FOB_Primitive_Type primitive_type;
    bool is_unsigned;
    FOB_Operand_Int_Const int_const;
} FOB_Operand_Int;

typedef union {
    float f32;
    double f64;
} FOB_Operand_Float_Const;

typedef struct {
    FOB_Primitive_Type primitive_type;
    FOB_Operand_Float_Const float_const;
} FOB_Operand_Float;

typedef struct {
    char *string;
} FOB_Operand_String;

typedef struct {
    FOB_Primitive_Type primitive_type;
    size_t number;
} FOB_Operand_Register;

typedef struct {
    char *name;
} FOB_Operand_Identifier;

typedef struct {
    char *name;
    size_t uid;
    FOB_Data_Type data_type;
} FOB_Operand_Variable;

typedef struct {
    size_t number;
} FOB_Operand_Label;

typedef struct {
    size_t index;
    size_t preceding_int_argument_count;
    size_t preceding_float_argument_count;
    FOB_Data_Type data_type;
} FOB_Operand_Argument;

typedef struct {
    size_t register_number;
    FOB_Data_Type data_type;
} FOB_Operand_Deref;

typedef struct {
    FOB_Operand_Type type;
    FOB_Data_Type data_type;

    union FOB_Operand_Value {
        FOB_Operand_Int int_;
        FOB_Operand_Float float_;
        FOB_Operand_String string;
        FOB_Operand_Register register_;
        FOB_Operand_Identifier identifier;
        FOB_Operand_Variable variable;
        FOB_Operand_Label label;
        FOB_Operand_Argument argument;
        FOB_Operand_Deref deref;
    } value;
} FOB_Operand;

typedef struct {
    FOB_Opcode type;
    FOB_Operand destination;
    FOB_Operand source;
} FOB_Op;

typedef struct {
    FOB_Op *ops;
    size_t count;
    size_t capacity;
    size_t label_count;
} FOB_IR;

static inline FOB_Op fob_create_fob_op(FOB_Opcode type, FOB_Operand destination, FOB_Operand source) {
    return (FOB_Op){ .type = type, .destination = destination, .source = source };
}

static inline FOB_Operand fob_create_fob_operand_int(FOB_Data_Type data_type, FOB_Operand_Int_Const int_const) {
    return (FOB_Operand){ .type = FOB_OPER_INT, .data_type = data_type, 
        .value.int_ = (FOB_Operand_Int){ .primitive_type = data_type.primitive_type, 
            .is_unsigned = fob_primitive_is_unsigned(data_type.primitive_type), .int_const = int_const }};
}

static inline FOB_Operand fob_create_fob_operand_float(FOB_Data_Type data_type, FOB_Operand_Float_Const float_const) {
    return (FOB_Operand){ .type = FOB_OPER_FLOAT, .data_type = data_type,
        .value.float_ = (FOB_Operand_Float){ .primitive_type = data_type.primitive_type, .float_const = float_const }};
}

static inline FOB_Operand fob_create_fob_operand_string(char *string) {
    return (FOB_Operand){ .type = FOB_OPER_STRING, .data_type = fob_create_data_type(FOB_PRIM_VOID), .value.string.string = string };
}

static inline FOB_Operand fob_create_fob_operand_register(FOB_Data_Type data_type, size_t number) {
    return (FOB_Operand){ .type = FOB_OPER_REGISTER, .data_type = data_type,
        .value.register_ = (FOB_Operand_Register){ .primitive_type = fob_data_type_to_primitive_type(&data_type), .number = number }};
}

static inline FOB_Operand fob_create_fob_identifier(char *name) {
    return (FOB_Operand){ .type = FOB_OPER_IDENTIFIER, .data_type = fob_create_data_type(FOB_PRIM_VOID),
        .value.identifier.name = name };
}

static inline FOB_Operand fob_create_fob_operand_variable(char *name, size_t uid, FOB_Data_Type data_type) {
    return (FOB_Operand){ .type = FOB_OPER_VARIABLE, .data_type = data_type, .value.variable = (FOB_Operand_Variable){
        .name = name, .uid = uid, .data_type = data_type }};
}

static inline FOB_Operand fob_create_fob_operand_label(size_t number) {
    return (FOB_Operand){ .type = FOB_OPER_LABEL, .data_type = fob_create_data_type(FOB_PRIM_VOID), .value.label.number = number };
}

static inline FOB_Operand fob_create_fob_operand_argument(size_t index, size_t preceding_int_argument_count, size_t preceding_float_argument_count, FOB_Data_Type data_type) {
    return (FOB_Operand){ .type = FOB_OPER_ARGUMENT, .data_type = data_type, .value.argument = (FOB_Operand_Argument){
        .index = index, .preceding_int_argument_count = preceding_int_argument_count, .preceding_float_argument_count = preceding_float_argument_count, .data_type = data_type
    }};
}

static inline FOB_Operand fob_create_fob_operand_deref(size_t register_number, FOB_Data_Type data_type) {
    return (FOB_Operand){ .type = FOB_OPER_DEREF, .data_type = data_type, .value.deref = (FOB_Operand_Deref){
        .register_number = register_number, .data_type = data_type
    }};
}

static inline bool fob_operand_is_constant(const FOB_Operand *operand) {
    return operand->type == FOB_OPER_INT || operand->type == FOB_OPER_FLOAT;
}

FOB_IR fob_ast_root_to_ir(FOB_AST_List *list);
void fob_delete_ir(FOB_IR *ir);

#endif
