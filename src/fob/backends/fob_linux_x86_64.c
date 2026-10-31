// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

// TOFIX:
// @hardcoded: Hardcoded temporary registers. Unlikely to cause damage now, but will when register variables are added.
// @implement: Unimplimented features.

#include "fob_ir.h"
#include "fob_data_type.h"
#include "fob_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#define STACK_CACHE_CAPACITY 16

#define DEFINED_REGISTERS 16

#define TEMP_INT_REGISTER_NUM (size_t)FOB_HW_RCX
#define TEMP_FLOAT_REGISTER_NUM (size_t)6

#define TEMP_INT_REGISTER_NUM_2 (size_t)FOB_HW_RBX
#define TEMP_FLOAT_REGISTER_NUM_2 (size_t)8

#define FUNCTION_RODATA_CAPACITY 16
#define LOCAL_VARIABLE_CAPACITY 4

#define REGISTER_ARGUMENT_CAPACITY 6

typedef enum {
    FOB_WORD_BYTE,
    FOB_WORD_WORD,
    FOB_WORD_DWORD,
    FOB_WORD_QWORD
} FOB_Word_Size;

typedef enum {
    FOB_HW_RAX,
    FOB_HW_RBX,
    FOB_HW_RCX,
    FOB_HW_RDX,
    FOB_HW_RSI,
    FOB_HW_RDI,
    FOB_HW_R8,
    FOB_HW_R9,
    FOB_HW_R10,
    FOB_HW_R11,
    FOB_HW_R12,
    FOB_HW_R13,
    FOB_HW_R14,
    FOB_HW_R15
} FOB_Hardware_Register;

typedef struct {
    size_t uid;
    FOB_Data_Type data_type;
    bool is_global;

    union FOB_Backend_Variable_Value {
        int stack_offset;
        char *global_name;
    } value;
} FOB_Backend_Variable;

typedef struct {
    size_t stack_bytes_used;
    size_t stack_bytes_allocated;
    char *function_rodata;
    size_t function_rodata_length;
    size_t function_rodata_capacity;
    size_t function_rodata_constant_count;
    FOB_Backend_Variable *local_variables;
    size_t local_variable_count;
    size_t local_variable_capacity;
    size_t *cached_stack_bytes_allocated;
    size_t *cached_stack_bytes_used;
    size_t stack_cache_count;
    size_t stack_cache_capacity;
} FOB_Backend;

static inline char float_word_char(const FOB_Primitive_Type type) {
    return type == FOB_PRIM_F32 ? 's' : 'd';
}

static FOB_Word_Size primitive_type_to_word_size(const FOB_Primitive_Type primitive_type) {
    switch (primitive_type) {
        case FOB_PRIM_BOOL:
        case FOB_PRIM_CHAR:
        case FOB_PRIM_I8:
        case FOB_PRIM_U8: return FOB_WORD_BYTE;
        case FOB_PRIM_I16:
        case FOB_PRIM_U16: return FOB_WORD_WORD;
        case FOB_PRIM_I32:
        case FOB_PRIM_U32:
        case FOB_PRIM_F32: return FOB_WORD_DWORD;
        default: return FOB_WORD_QWORD;
    }
}

static size_t primitive_type_to_allocation_size(const FOB_Primitive_Type primitive_type) {
    switch (primitive_type) {
        case FOB_PRIM_BOOL:
        case FOB_PRIM_CHAR:
        case FOB_PRIM_I8:
        case FOB_PRIM_U8: return 1;
        case FOB_PRIM_I16:
        case FOB_PRIM_U16: return 2;
        case FOB_PRIM_I32:
        case FOB_PRIM_U32:
        case FOB_PRIM_F32: return 4;
        default: return 8;
    }
}

static char *word_size_to_string(const FOB_Word_Size word_size) {
    switch (word_size) {
        case FOB_WORD_BYTE: return "byte";
        case FOB_WORD_WORD: return "word";
        case FOB_WORD_DWORD: return "dword";
        default: return "qword";
    }
}

static FOB_Operand get_temporary_register(const FOB_Primitive_Type type) {
    return (FOB_Operand){ .type = FOB_OPER_REGISTER, .value.register_ = (FOB_Operand_Register){
        .primitive_type = type, .number = fob_primitive_is_float(type) ? TEMP_FLOAT_REGISTER_NUM : TEMP_INT_REGISTER_NUM }};
}

// For special cases where the 2 main temporary registers may be being used (arithmetic, indexing).
static FOB_Operand get_temporary_register_2(const FOB_Primitive_Type type) {
    return (FOB_Operand){ .type = FOB_OPER_REGISTER, .value.register_ = (FOB_Operand_Register){
        .primitive_type = type, .number = fob_primitive_is_float(type) ? TEMP_FLOAT_REGISTER_NUM_2 : TEMP_INT_REGISTER_NUM_2 }};
}

static inline bool is_register_argument(const FOB_Operand *operand) {
    assert(operand->type == FOB_OPER_ARGUMENT);
    return operand->value.argument.index < REGISTER_ARGUMENT_CAPACITY;
}

static FOB_Backend create_backend() {
    FOB_Backend backend = (FOB_Backend){ .stack_bytes_used = 0, .stack_bytes_allocated = 0,
        .function_rodata = malloc(FUNCTION_RODATA_CAPACITY + 1), .function_rodata_length = 0,
        .function_rodata_capacity = FUNCTION_RODATA_CAPACITY, .function_rodata_constant_count = 0,
        .local_variables = malloc(LOCAL_VARIABLE_CAPACITY * sizeof(FOB_Backend_Variable)), 
        .local_variable_count = 0, .local_variable_capacity = LOCAL_VARIABLE_CAPACITY,
        .cached_stack_bytes_allocated = malloc(STACK_CACHE_CAPACITY * sizeof(size_t)),
        .cached_stack_bytes_used = malloc(STACK_CACHE_CAPACITY * sizeof(size_t)),
        .stack_cache_count = 0, .stack_cache_capacity = STACK_CACHE_CAPACITY };

    backend.function_rodata[0] = '\0';
    return backend;
}

static void delete_backend(FOB_Backend *backend) {
    free(backend->function_rodata);
    free(backend->local_variables);
    free(backend->cached_stack_bytes_allocated);
    free(backend->cached_stack_bytes_used);
}

static void reset_backend_to_enter_new_function(FOB_Backend *backend) {
    backend->stack_bytes_used = backend->stack_bytes_allocated = 
        backend->function_rodata_length = backend->function_rodata_constant_count = 0;
    backend->function_rodata_capacity = FUNCTION_RODATA_CAPACITY;
    backend->function_rodata[0] = '\0';

    backend->local_variable_count = 0;
    backend->local_variable_capacity = LOCAL_VARIABLE_CAPACITY;
}

static void append_new_function_rodata(const char *rodata, FOB_Backend *backend) {
    const size_t len = strlen(rodata);

    if (backend->function_rodata_length + len > backend->function_rodata_capacity) {
        backend->function_rodata_capacity *= 2;
        backend->function_rodata = realloc(backend->function_rodata, backend->function_rodata_capacity + 1);
    }

    strcat(backend->function_rodata, rodata);
    backend->function_rodata_length += len;
}

static FOB_Backend_Variable *find_backend_variable(const size_t uid, const FOB_Backend *backend) {
    for (size_t i = 0; i < backend->local_variable_count; i++) {
        FOB_Backend_Variable *var = &backend->local_variables[i];

        if (var->uid == uid)
            return var;
    }

    return NULL;
}

static void add_local_backend_variable(size_t uid, FOB_Data_Type data_type, int stack_offset, FOB_Backend *backend) {
    if (backend->local_variable_count + 1 > backend->local_variable_capacity) {
        backend->local_variable_capacity *= 2;
        backend->local_variables = realloc(backend->local_variables, backend->local_variable_capacity * sizeof(FOB_Backend_Variable));
    }

    backend->local_variables[backend->local_variable_count++] = (FOB_Backend_Variable){ 
        .uid = uid, .data_type = data_type, .value.stack_offset = stack_offset };
}

static char *fob_operand_int_to_string(const FOB_Operand *operand) {
    const FOB_Operand_Int *int_ = &operand->value.int_;
    char *str = malloc(32);

    switch (int_->primitive_type) {
        case FOB_PRIM_I32:
            sprintf(str, "%" PRId32, int_->int_const.i32);
            break;
        case FOB_PRIM_I64:
            sprintf(str, "%" PRId64, int_->int_const.i64);
            break;
        case FOB_PRIM_U32:
            sprintf(str, "%" PRIu32, int_->int_const.u32);
            break;
        case FOB_PRIM_U64:
            sprintf(str, "%" PRIu64, int_->int_const.u64);
            break;
        default:
            fob_alert("Uncaught int const primitive type %u\n", int_->primitive_type);
            str[0] = '\0';
            break;
    }

    return str;
}

static char *fob_operand_float_to_string(const FOB_Operand *operand, FOB_Backend *backend, const bool dereferenced) {
    const FOB_Operand_Float *float_ = &operand->value.float_;
    char *label = malloc(16);
    sprintf(label, ".c%zu", backend->function_rodata_constant_count++);

    char *data = malloc(strlen(label) + 32);

    if (float_->primitive_type == FOB_PRIM_F32) {
        uint32_t bits;
        memcpy(&bits, &float_->float_const.f32, 4);
        sprintf(data, "%s: dd %" PRIu32 "\n", label, bits);
    } else if (float_->primitive_type == FOB_PRIM_F64) {
        uint64_t bits;
        memcpy(&bits, &float_->float_const.f64, 8);
        sprintf(data, "%s: dq %" PRIu64 "\n", label, bits);
    } else {
        fob_alert("Uncaught float const primitive type %u\n", float_->primitive_type);
        data[0] = '\0';
    }

    append_new_function_rodata(data, backend);
    free(data);

    if (!dereferenced)
        return label;

    char *deref = malloc(strlen(label) + 9);
    sprintf(deref, "%s [%s]", word_size_to_string(primitive_type_to_word_size(float_->primitive_type)), label);
    free(label);
    return deref;
}

static char *fob_operand_string_to_string(const FOB_Operand *operand, FOB_Backend *backend, const bool dereference) {
    char *label = malloc(16);
    sprintf(label, ".c%zu", backend->function_rodata_constant_count++);

    char *data = malloc(strlen(label) + strlen(operand->value.string.string) + 13);
    sprintf(data, "%s: db `%s`\n", label, operand->value.string.string);

    append_new_function_rodata(data, backend);
    free(data);

    if (!dereference)
        return label;

    char *code = malloc(strlen(label) + 11);
    sprintf(code, "qword [%s]", label);
    free(label);
    return code;
}

static size_t ir_register_to_hardware_register_number(const size_t number, const bool is_float) {
    switch (number) {
        case FOB_TEMPORARY_IR_REGISTER_1: return is_float ? 6 : (size_t)FOB_HW_RAX;
        case FOB_TEMPORARY_IR_REGISTER_2: return is_float ? 7 : (size_t)FOB_HW_RCX;
        default: break;
    }

    fob_alert("Uncaught IR register number %zu\n", number);
    return 0;
}

static char *fob_operand_register_to_string(const FOB_Operand *operand, const bool is_hardware_register) {
    const FOB_Operand_Register *reg = &operand->value.register_;
    const bool is_float = fob_data_type_is_float(&operand->data_type);
    size_t num = is_hardware_register ? reg->number : ir_register_to_hardware_register_number(reg->number, is_float);
    char *str = malloc(24);

    if (is_float) {
        sprintf(str, "xmm%zu", num);
        return str;
    }

    FOB_Word_Size word;

    switch (reg->primitive_type) {
        case FOB_PRIM_I8:
        case FOB_PRIM_U8:
        case FOB_PRIM_CHAR:
        case FOB_PRIM_BOOL:
            word = FOB_WORD_BYTE;
            break;
        case FOB_PRIM_I16:
        case FOB_PRIM_U16:
            word = FOB_WORD_WORD;
            break;
        case FOB_PRIM_I32:
        case FOB_PRIM_U32:
            word = FOB_WORD_DWORD;
            break;
        case FOB_PRIM_I64:
        case FOB_PRIM_ISIZE:
        case FOB_PRIM_U64:
        case FOB_PRIM_USIZE:
            word = FOB_WORD_QWORD;
            break;
        default:
            fob_alert("Uncaught register primitive type %u\n", reg->primitive_type);
            word = FOB_WORD_BYTE;
            break;
    }

    if (num > FOB_HW_RDI) {
        sprintf(str, "r%zu", num);

        if (word == FOB_WORD_BYTE)
            strcat(str, "b");
        else if (word == FOB_WORD_WORD)
            strcat(str, "w");
        else if (word == FOB_WORD_DWORD)
            strcat(str, "d");

        return str;
    }

    char *reg_chars;

    if (num == FOB_HW_RAX) reg_chars = "a";
    else if (num == FOB_HW_RBX) reg_chars = "b";
    else if (num == FOB_HW_RCX) reg_chars = "c";
    else if (num == FOB_HW_RDX) reg_chars = "d";
    else if (num == FOB_HW_RSI) reg_chars = "si";
    else reg_chars = "di";

    if (word == FOB_WORD_BYTE)
        sprintf(str, "%sl", reg_chars);
    else if (word == FOB_WORD_WORD) {
        if (num <= FOB_HW_RDX)
            sprintf(str, "%sx", reg_chars);
        else
            strcpy(str, reg_chars);
    } else {
        const char size_char = word == FOB_WORD_DWORD ? 'e' : 'r';

        if (num <= FOB_HW_RDX)
            sprintf(str, "%c%sx", size_char, reg_chars);
        else
            sprintf(str, "%c%s", size_char, reg_chars);
    }

    return str;
}

static char *fob_operand_variable_to_string(const FOB_Operand *operand, FOB_Backend *backend) {
    FOB_Backend_Variable *var = find_backend_variable(operand->value.variable.uid, backend);

    if (var == NULL) {
        fob_alert("Unable to find backend variable %zu ('%s')\n", operand->value.variable.uid, operand->value.variable.name);
        return calloc(1, sizeof(char));
    }

    char *str;
    const char *word = word_size_to_string(primitive_type_to_word_size(fob_data_type_to_primitive_type(&var->data_type)));

    if (var->is_global) {
        str = malloc(strlen(word) + strlen(var->value.global_name) + 5);
        sprintf(str, "%s [%s_]", word, var->value.global_name);
        return str;
    }

    str = malloc(strlen(word) + 32);
    sprintf(str, "%s [rbp%+d]", word, var->value.stack_offset);
    return str;
}

static char *fob_operand_label_to_string(const FOB_Operand *operand) {
    char *str = malloc(24);
    sprintf(str, ".l%zu", operand->value.label.number);
    return str;
}

static char *fob_operand_identifier_to_string(const FOB_Operand *operand) {
    char *str = malloc(strlen(operand->value.identifier.name) + 2);
    sprintf(str, "%s_", operand->value.identifier.name);
    return str;
}

static FOB_Operand get_argument_hardware_register(const FOB_Data_Type data_type, const size_t index) {
    assert(index < REGISTER_ARGUMENT_CAPACITY);

    if (fob_data_type_is_float(&data_type))
        return fob_create_fob_operand_register(data_type, index);

    FOB_Hardware_Register hwreg;

    switch (index) {
        case 0:
            hwreg = FOB_HW_RDI;
            break;
        case 1:
            hwreg = FOB_HW_RSI;
            break;
        case 2:
            hwreg = FOB_HW_RDX;
            break;
        case 3:
            hwreg = FOB_HW_RCX;
            break;
        case 4:
            hwreg = FOB_HW_R8;
            break;
        case 5:
            hwreg = FOB_HW_R9;
            break;
        default:
            fob_alert("Uncaught register argument index %zu\n", index);
            hwreg = 0;
            break;
    }

    return fob_create_fob_operand_register(data_type, hwreg);
}

static char *fob_operand_argument_to_string(const FOB_Operand *operand) {
    if (is_register_argument(operand)) {
        const size_t regnum = fob_data_type_is_float(&operand->data_type) ? operand->value.argument.preceding_float_argument_count :
            operand->value.argument.preceding_int_argument_count;

        const FOB_Operand reg = get_argument_hardware_register(operand->data_type, regnum);
        return fob_operand_register_to_string(&reg, true);
    }

    // TODO @implement:
    fob_alert("Stack parameters aren't yet implemented\n");
    return calloc(1, sizeof(char));
}

static char *fob_operand_deref_to_string(const FOB_Operand *operand) {
    const FOB_Operand reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I64), operand->value.deref.register_number);
    char *reg_str = fob_operand_register_to_string(&reg, false);

    char *str = malloc(strlen(reg_str) + 13);
    sprintf(str, "%s [%s]", word_size_to_string(primitive_type_to_word_size(fob_data_type_to_primitive_type(&operand->value.deref.data_type))), reg_str);

    free(reg_str);
    return str;
}

static char *fob_operand_to_string(const FOB_Operand *operand, FOB_Backend *backend) {
    switch (operand->type) {
        case FOB_OPER_INT: return fob_operand_int_to_string(operand);
        case FOB_OPER_FLOAT: return fob_operand_float_to_string(operand, backend, true);
        case FOB_OPER_STRING: return fob_operand_string_to_string(operand, backend, true);
        case FOB_OPER_REGISTER: return fob_operand_register_to_string(operand, false);
        case FOB_OPER_VARIABLE: return fob_operand_variable_to_string(operand, backend);
        case FOB_OPER_LABEL: return fob_operand_label_to_string(operand);
        case FOB_OPER_IDENTIFIER: return fob_operand_identifier_to_string(operand);
        case FOB_OPER_ARGUMENT: return fob_operand_argument_to_string(operand);
        case FOB_OPER_DEREF: return fob_operand_deref_to_string(operand);
        default: break;
    }

    fob_alert("Uncaught operand type %u\n", operand->type);
    return calloc(1, sizeof(char));
}

static char *emit_block_start(FOB_Backend *backend) {
    if (backend->stack_cache_count + 1 > backend->stack_cache_capacity) {
        backend->stack_cache_capacity *= 2;
        backend->cached_stack_bytes_allocated = realloc(backend->cached_stack_bytes_allocated, backend->stack_cache_capacity * sizeof(size_t));
        backend->cached_stack_bytes_used = realloc(backend->cached_stack_bytes_used , backend->stack_cache_capacity * sizeof(size_t));
    }

    backend->cached_stack_bytes_allocated[backend->stack_cache_count] = backend->stack_bytes_allocated;
    backend->cached_stack_bytes_used[backend->stack_cache_count++] = backend->stack_bytes_used;
    return calloc(1, sizeof(char));
}

static char *emit_block_end(FOB_Backend *backend) {
    if (backend->stack_cache_count == 0) {
        fob_alert("BLOCK_END but block count is 0\n");
        return calloc(1, sizeof(char));
    }

    const size_t was_allocated = backend->stack_bytes_allocated;

    backend->stack_bytes_allocated = backend->cached_stack_bytes_allocated[--backend->stack_cache_count];
    backend->stack_bytes_used = backend->cached_stack_bytes_used[backend->stack_cache_count];

    if (was_allocated == backend->stack_bytes_allocated)
        return calloc(1, sizeof(char));

    char *code = malloc(24);
    const int diff = (int)was_allocated - (int)backend->stack_bytes_allocated;
    sprintf(code, "%s rsp, %d\n", diff > 0 ? "add" : "sub", abs(diff));
    return code;
}

static char *emit_func_start(const FOB_Op *op, FOB_Backend *backend) {
    reset_backend_to_enter_new_function(backend);
    assert(op->source.type == FOB_OPER_IDENTIFIER);
    char *name = fob_operand_to_string(&op->source, backend);

    char *code = malloc(strlen(name) + 27);
    sprintf(code, "%s:\npush rbp\nmov rbp, rsp\n", name);

    free(name);
    return code;
}

static char *emit_func_end(FOB_Backend *backend) {
    char *rodata = malloc(backend->function_rodata_length + 1);
    strcpy(rodata, backend->function_rodata);
    return rodata;
}

static char *emit_load_const(const FOB_Op *op, FOB_Backend *backend);
static char *emit_load_memory(const FOB_Op *op, FOB_Backend *backend);

static char *emit_return(const FOB_Op *op, FOB_Backend *backend) {
    char *code;
    
    if (op->source.type == FOB_OPER_NONE) {
        code = malloc(13);
        strcpy(code, backend->stack_bytes_allocated > 0 ? "leave\nret\n" : "pop rbp\nret\n");
        return code;
    }

    code = fob_operand_is_constant(&op->source) ? emit_load_const(op, backend) : emit_load_memory(op, backend);
    code = realloc(code, strlen(code) + 13);
    strcat(code, backend->stack_bytes_allocated > 0 ? "leave\nret\n" : "pop rbp\nret\n");
    return code;
}

static char *emit_load_const(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    assert(dst->type == FOB_OPER_REGISTER);

    const FOB_Primitive_Type dst_prim = fob_data_type_to_primitive_type(&dst->data_type);
    const FOB_Operand *src = &op->source;
    const FOB_Primitive_Type src_prim = fob_data_type_to_primitive_type(&src->data_type);

    char *dst_str;
    char *src_str = fob_operand_to_string(src, backend);
    char *code;

    switch (dst_prim) {
        case FOB_PRIM_BOOL:
        case FOB_PRIM_CHAR:
        case FOB_PRIM_I8:
        case FOB_PRIM_U8:
        case FOB_PRIM_I16:
        case FOB_PRIM_U16: {
            FOB_Operand dst_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I32), dst->value.register_.number);
            dst_str = fob_operand_to_string(&dst_reg, backend);

            if (fob_primitive_is_float(src_prim)) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 14);
                sprintf(code, "cvtts%c2si %s, %s\n", float_word_char(src_prim), dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 8);
                sprintf(code, "mov %s, %s\n", dst_str, src_str);
            }

            if (dst_prim != FOB_PRIM_BOOL)
                break;

            FOB_Operand dst_reg_8bit = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I8), dst->value.register_.number);
            char *dst_str_8bit = fob_operand_to_string(&dst_reg_8bit, backend);

            char *trunc = malloc((strlen(dst_str) * 2) + strlen(dst_str_8bit) + 16);
            sprintf(trunc, "test %s, %s\n"
                           "setnz %s\n", dst_str, dst_str, dst_str_8bit);
            free(dst_str_8bit);

            code = realloc(code, strlen(code) + strlen(trunc) + 1);
            strcat(code, trunc);
            free(trunc);
            break;
        }
        case FOB_PRIM_I32:
        case FOB_PRIM_U32:
        case FOB_PRIM_I64:
        case FOB_PRIM_ISIZE:
        case FOB_PRIM_U64:
        case FOB_PRIM_USIZE:
            dst_str = fob_operand_to_string(dst, backend);

            if (fob_primitive_is_float(src_prim)) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 14);
                sprintf(code, "cvtts%c2si %s, %s\n", float_word_char(src_prim), dst_str, src_str);
                break;
            }

            code = malloc(strlen(dst_str) + strlen(src_str) + 8);
            sprintf(code, "mov %s, %s\n", dst_str, src_str);
            break;
        case FOB_PRIM_F32:
        case FOB_PRIM_F64: {
            dst_str = fob_operand_to_string(dst, backend);

            if (dst_prim == FOB_PRIM_F32 && src_prim == FOB_PRIM_F32) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 10);
                sprintf(code, "movss %s, %s\n", dst_str, src_str);
                break;
            } else if (dst_prim == FOB_PRIM_F32 && src_prim == FOB_PRIM_F64) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "cvtsd2ss %s, %s\n", dst_str, src_str);
                break;
            } else if (dst_prim == FOB_PRIM_F64 && src_prim == FOB_PRIM_F64) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 10);
                sprintf(code, "movsd %s, %s\n", dst_str, src_str);
                break;
            } else if (dst_prim == FOB_PRIM_F64 && src_prim == FOB_PRIM_F32) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "cvtss2sd %s, %s\n", dst_str, src_str);
                break;
            }

            char *temp_reg_str;
            FOB_Operand temp_reg;

            if (primitive_type_to_allocation_size(src_prim) < 4) {
                const bool is_unsigned = fob_primitive_is_unsigned(src_prim);
                temp_reg = get_temporary_register(is_unsigned ? FOB_PRIM_U32 : FOB_PRIM_I32);
                temp_reg_str = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(dst_str) + strlen(src_str) + (strlen(temp_reg_str) * 2) + 23);
                sprintf(code, "mov%cx %s, %s\n"
                              "cvtsi2s%c %s, %s\n", is_unsigned ? 'z' : 's', temp_reg_str, src_str, float_word_char(dst_prim), dst_str, temp_reg_str);
            } else {
                temp_reg = get_temporary_register(src_prim);
                temp_reg_str = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(dst_str) + strlen(src_str) + (strlen(temp_reg_str) * 2) + 21);
                sprintf(code, "mov %s, %s\n"
                              "cvtsi2s%c %s, %s\n", temp_reg_str, src_str, float_word_char(dst_prim), dst_str, temp_reg_str);
            }

            free(temp_reg_str);
            break;
        }
        default:
            fob_alert("Uncaught destination primitive type %u\n", dst_prim);
            free(src_str);
            return calloc(1, sizeof(char));
    }

    free(dst_str);
    free(src_str);
    return code;
}

static char *emit_load_memory(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    assert(dst->type == FOB_OPER_REGISTER);

    const FOB_Primitive_Type dst_prim = fob_data_type_to_primitive_type(&dst->data_type);
    const FOB_Operand *src = &op->source;
    const FOB_Primitive_Type src_prim = fob_data_type_to_primitive_type(&src->data_type);
    const FOB_Word_Size src_word = primitive_type_to_word_size(src_prim);

    char *dst_str;
    char *src_str = fob_operand_to_string(src, backend);
    char *code;

    if (src->type == FOB_OPER_STRING) {
        dst_str = fob_operand_to_string(dst, backend);
        code = malloc(strlen(dst_str) + strlen(src_str) + 12);
        sprintf(code, "lea %s, %s\n", dst_str, src_str);
        free(dst_str);
        free(src_str);
        return code;
    }

    switch (dst_prim) {
        case FOB_PRIM_BOOL:
        case FOB_PRIM_CHAR:
        case FOB_PRIM_I8:
        case FOB_PRIM_U8:
        case FOB_PRIM_I16:
        case FOB_PRIM_U16:
        case FOB_PRIM_I32:
        case FOB_PRIM_U32:
        case FOB_PRIM_I64:
        case FOB_PRIM_U64:
        case FOB_PRIM_ISIZE:
        case FOB_PRIM_USIZE: {
            const FOB_Word_Size dst_word = primitive_type_to_word_size(dst_prim);

            if (!fob_primitive_is_float(src_prim) && dst_word == src_word && dst_word >= FOB_WORD_DWORD) {
                dst_str = fob_operand_to_string(dst, backend);
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "mov %s, %s\n", dst_str, src_str);
                break;
            }

            FOB_Operand dst_reg;

            if (fob_primitive_is_float(src_prim)) {
                dst_reg = fob_create_fob_operand_register(fob_create_data_type(dst_word <= FOB_WORD_DWORD ?  FOB_PRIM_I32 : dst_prim), dst->value.register_.number);
                dst_str = fob_operand_register_to_string(&dst_reg, false);

                code = malloc(strlen(dst_str) + strlen(src_str) + 19);
                sprintf(code, "cvtts%c2si %s, %s\n", float_word_char(src_prim), dst_str, src_str);
                break;
            }
            
            if (src_word > dst_word) {
                dst_reg = fob_create_fob_operand_register(fob_create_data_type(src_prim), dst->value.register_.number);
                dst_str = fob_operand_register_to_string(&dst_reg, false);

                if (strcmp(dst_str, src_str) == 0)
                    code = calloc(1, sizeof(char));
                else {
                    code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                    sprintf(code, "mov %s, %s\n", dst_str, src_str);
                }

                break;
            }
            
            //if (src_word < dst_word) {
            if (fob_primitive_is_unsigned(src_prim)) {
                if (src_prim == FOB_PRIM_U32) { // mov 32bit in x86_64 is auto zero-extended.
                    dst_reg = fob_create_fob_operand_register(fob_create_data_type(src_prim), dst->value.register_.number);
                    dst_str = fob_operand_register_to_string(&dst_reg, false);

                    code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                    sprintf(code, "mov %s, %s\n", dst_str, src_str);
                } else {
                    dst_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_U32), dst->value.register_.number);
                    dst_str = fob_operand_register_to_string(&dst_reg, false);

                    code = malloc(strlen(dst_str) + strlen(src_str) + 14);
                    sprintf(code, "movzx %s, %s\n", dst_str, src_str);
                }
                break;
            }


            if (dst_word == FOB_WORD_QWORD && src_word == FOB_WORD_DWORD) {
                dst_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I64), dst->value.register_.number);
                dst_str = fob_operand_register_to_string(&dst_reg, false);

                code = malloc(strlen(dst_str) + strlen(src_str) + 15);
                sprintf(code, "movsxd %s, %s\n", dst_str, src_str);
                break;
            }

            dst_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I32), dst->value.register_.number);
            dst_str = fob_operand_register_to_string(&dst_reg, false);

            code = malloc(strlen(dst_str) + strlen(src_str) + 15);
            sprintf(code, "movsx %s, %s\n", dst_str, src_str);
            break;
        }
        case FOB_PRIM_F32:
        case FOB_PRIM_F64: {
            dst_str = fob_operand_register_to_string(dst, false);

            if (dst_prim == FOB_PRIM_F32 && src_prim == FOB_PRIM_F32) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 14);
                sprintf(code, "movss %s, %s\n", dst_str, src_str);
                break;
            }

            if (dst_prim == FOB_PRIM_F32 && src_prim == FOB_PRIM_F64) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 17);
                sprintf(code, "cvtsd2ss %s, %s\n", dst_str, src_str);
                break;
            }

            if (dst_prim == FOB_PRIM_F64 && src_prim == FOB_PRIM_F32) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 17);
                sprintf(code, "cvtss2sd %s, %s\n", dst_str, src_str);
                break;
            }

            if (dst_prim == FOB_PRIM_F64 && src_prim == FOB_PRIM_F64) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 14);
                sprintf(code, "movsd %s, %s\n", dst_str, src_str);
                break;
            }

            FOB_Operand temp_reg;
            char *temp_reg_str = NULL;

            if (src_word < FOB_WORD_DWORD) {
                temp_reg = get_temporary_register(FOB_PRIM_I32);
                temp_reg_str = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(dst_str) + strlen(src_str) + (strlen(temp_reg_str) * 2) + 32);
                sprintf(code, "mov%cx %s, %s\n"
                              "cvtsi2s%c %s, %s\n", fob_primitive_is_unsigned(src_prim) ? 'z' : 's', temp_reg_str, src_str, 
                              float_word_char(dst_prim), dst_str, temp_reg_str);
            } else if (src->type == FOB_OPER_REGISTER) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 19);
                sprintf(code, "cvtsi2s%c %s, %s\n", float_word_char(dst_prim), dst_str, src_str);
            } else {
                temp_reg = get_temporary_register(src_prim);
                temp_reg_str = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(dst_str) + strlen(src_str) + (strlen(temp_reg_str) * 2) + 29);
                sprintf(code, "mov %s, %s\n"
                              "cvtsi2s%c %s, %s\n", temp_reg_str, src_str, float_word_char(dst_prim), dst_str, temp_reg_str);
            }

            if (temp_reg_str != NULL)
                free(temp_reg_str);
            break;
        }
        default:
            fob_alert("Uncaught destination primitive type %u\n", dst_prim);
            free(src_str);
            return calloc(1, sizeof(char));
    }

    if (dst_prim != FOB_PRIM_BOOL) {
        free(dst_str);
        free(src_str);
        return code;
    }

    FOB_Operand dst_reg_8bit = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I8), dst->value.register_.number);
    char *dst_reg_8bit_str = fob_operand_to_string(&dst_reg_8bit, backend);

    char *trunc = malloc(strlen(code) + (strlen(dst_str) * 2) + strlen(dst_reg_8bit_str) + 24);
    sprintf(trunc, "%stest %s, %s\n"
                   "setnz %s\n", code, dst_str, dst_str, dst_reg_8bit_str);

    free(code);
    free(dst_reg_8bit_str);
    free(dst_str);
    free(src_str);
    return trunc;
}

static char *emit_new_local_variable(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand_Variable *var = &op->destination.value.variable;
    const size_t size = primitive_type_to_allocation_size(fob_data_type_to_primitive_type(&var->data_type));
    backend->stack_bytes_used += size;
    add_local_backend_variable(var->uid, var->data_type, -backend->stack_bytes_used, backend);

    if (backend->stack_bytes_used <= backend->stack_bytes_allocated)
        return calloc(1, sizeof(char));

    const size_t allocated_before = backend->stack_bytes_allocated;

    while (backend->stack_bytes_allocated < backend->stack_bytes_used || backend->stack_bytes_allocated % 16 != 0)
        backend->stack_bytes_allocated++;

    char *code = malloc(32);
    sprintf(code, "sub rsp, %zu\n", backend->stack_bytes_allocated - allocated_before);
    return code;
}

static char *emit_store_memory(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    const FOB_Primitive_Type dst_prim = fob_data_type_to_primitive_type(&dst->data_type);
    char *dst_str = fob_operand_to_string(dst, backend);

    const FOB_Operand *src = &op->source;
    char *src_str;
    FOB_Primitive_Type src_prim;
    char *preload = NULL;

    // Arguments can either be passed on the stack or in registers, both are memory, but they are OPER_ARGUMENT types.
    // We need to make sure the argument is passed in a register, or gets loaded from the stack into a register.
    if (src->type == FOB_OPER_ARGUMENT) {
        if (is_register_argument(src)) {
            FOB_Operand src_reg = *src;
            src_reg.data_type = dst->data_type;
            src_prim = dst_prim;
            src_str = fob_operand_to_string(&src_reg, backend);
        } else {
            // get_temporary_register_2 because register_1 is RCX for integers which is the 4th register argument.
            FOB_Op preload_op = (FOB_Op){ .type = FOB_OP_LOAD_MEMORY, .destination = get_temporary_register_2(src->data_type.primitive_type), .source = op->source };
            preload = emit_load_memory(&preload_op, backend);
            src_str = fob_operand_to_string(&preload_op.destination, backend);
            src_prim = fob_data_type_to_primitive_type(&src->data_type);
        }
    } else {
        if (fob_data_type_is_float(&src->data_type) != fob_primitive_is_float(dst_prim) ||
                word_size_to_string(primitive_type_to_word_size(dst_prim)) == word_size_to_string(primitive_type_to_word_size(fob_data_type_to_primitive_type(&src->data_type)))) {
            src_str = fob_operand_to_string(src, backend);
            src_prim = fob_data_type_to_primitive_type(&src->data_type);
        } else {
            assert(src->type == FOB_OPER_REGISTER);
            FOB_Operand src_reg = *src;
            src_reg.value.register_.primitive_type = fob_data_type_to_primitive_type(&dst->data_type);
            src_prim = dst_prim;
            src_str = fob_operand_to_string(&src_reg, backend);
        }
    }

    char *code;

    if (fob_primitive_is_float(dst_prim)) {
        if (dst_prim == FOB_PRIM_F32 && src_prim == FOB_PRIM_F64) {
            code = malloc(strlen(dst_str) + strlen(src_str) + 17);
            sprintf(code, "cvtsd2ss %s, %s\n", dst_str, src_str);
        } else if (dst_prim == FOB_PRIM_F64 && src_prim == FOB_PRIM_F32) {
            code = malloc(strlen(dst_str) + strlen(src_str) + 17);
            sprintf(code, "cvtss2sd %s, %s\n", dst_str, src_str);
        } else if (!fob_primitive_is_float(src_prim)) {
            code = malloc(strlen(dst_str) + strlen(src_str) + 18);
            sprintf(code, "cvtsi2s%c %s, %s\n", float_word_char(dst_prim), dst_str, src_str);
        } else {
            code = malloc(strlen(dst_str) + strlen(src_str) + 15);
            sprintf(code, "movs%c %s, %s\n", float_word_char(dst_prim), dst_str, src_str);
        }
    } else {
        if (fob_primitive_is_float(src_prim)) {
            code = malloc(strlen(dst_str) + strlen(src_str) + 14);
            sprintf(code, "cvtts%c2si %s, %s\n", float_word_char(src_prim), dst_str, src_str);
        } else {
            code = malloc(strlen(dst_str) + strlen(src_str) + 12);
            sprintf(code, "mov %s, %s\n", dst_str, src_str);
        }
    }

    free(dst_str);
    free(src_str);

    if (preload == NULL)
        return code;

    preload = realloc(preload, strlen(preload) + strlen(code) + 1);
    strcat(preload, code);
    free(code);
    return preload;
}

static char *emit_push(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *src = &op->source;
    assert(src->type == FOB_OPER_REGISTER || (src->type == FOB_OPER_ARGUMENT && is_register_argument(src)));

    char *src_str;
    char *code;

    if (fob_data_type_is_float(&src->data_type)) {
        src_str = fob_operand_to_string(src, backend);
        code = malloc(strlen(src_str) + 37);
        sprintf(code, "sub rsp, 8\n"
                      "movs%c %s [rsp], %s\n", float_word_char(src->data_type.primitive_type), 
                      word_size_to_string(primitive_type_to_word_size(src->data_type.primitive_type)), src_str);
    } else {
        if (src->type == FOB_OPER_REGISTER) {
            FOB_Operand src_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I64), src->value.register_.number); // Ensure always 64 bit register.
            src_str = fob_operand_register_to_string(&src_reg, false);
        } else {
            FOB_Operand src_reg = *src;
            src_reg.data_type.primitive_type = FOB_PRIM_I64;
            src_str = fob_operand_to_string(&src_reg, backend);
        }

        code = malloc(strlen(src_str) + 9);
        sprintf(code, "push %s\n", src_str);
    }

    free(src_str);
    backend->stack_bytes_used += 8;
    backend->stack_bytes_allocated += 8;
    return code;
}

static char *emit_pop(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    assert(dst->type == FOB_OPER_REGISTER || (dst->type == FOB_OPER_ARGUMENT && is_register_argument(dst)));

    char *dst_str;
    char *code;

    if (fob_data_type_is_float(&dst->data_type)) {
        dst_str = fob_operand_to_string(dst, backend);
        code = malloc(strlen(dst_str) + 37);
        sprintf(code, "movs%c %s, %s [rsp]\n"
                      "add rsp, 8\n", float_word_char(dst->data_type.primitive_type), 
                      dst_str, word_size_to_string(primitive_type_to_word_size(dst->data_type.primitive_type)));
    } else {
        // Ensure 64 bit register.

        if (dst->type == FOB_OPER_REGISTER) {
            FOB_Operand dst_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I64), dst->value.register_.number); // Ensure always 64 bit register.
            dst_str = fob_operand_register_to_string(&dst_reg, false);
        } else {
            FOB_Operand dst_reg = *dst;
            dst_reg.data_type.primitive_type = FOB_PRIM_I64;
            dst_str = fob_operand_to_string(&dst_reg, backend);
        }

        code = malloc(strlen(dst_str) + 8);
        sprintf(code, "pop %s\n", dst_str);
    }

    free(dst_str);
    assert(backend->stack_bytes_allocated >= 8);
    backend->stack_bytes_used -= 8;
    backend->stack_bytes_allocated -= 8;
    return code;
}

static char *emit_arithmetic(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    const FOB_Operand *src = &op->source;
    assert(dst->type == FOB_OPER_REGISTER);

    char *dst_str = fob_operand_to_string(dst, backend);
    char *src_str = NULL;

    if (op->type != FOB_OP_NOT && op->type != FOB_OP_NEG && op->type != FOB_OP_BOOL_NOT) {
        assert(dst->data_type.primitive_type == src->data_type.primitive_type);
        src_str = fob_operand_to_string(src, backend);
    } else {
        assert(src->type == FOB_OPER_NONE);
    }

    const FOB_Primitive_Type prim_type = fob_data_type_to_primitive_type(&dst->data_type);
    const bool is_float = fob_primitive_is_float(prim_type);
    char *code;

    switch (op->type) {
        case FOB_OP_AND:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "pand %s, %s\n", dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "and %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_OR:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "por %s, %s\n", dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "or %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_XOR:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "pxor %s, %s\n", dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "xor %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_NOT:
            if (is_float) {
                fob_alert("NOT instruction with floating-point result type\n");
                code = calloc(1, sizeof(char));
            } else {
                /*
                if (word_size_to_string(primitive_type_to_word_size(prim_type)) != FOB_WORD_BYTE) {
                    // NEED 8 bit register.
                    free(dst_str);
                    FOB_Operand dst_8bit = fob_create_fob_operand_register(FOB_PRIM_I8, dst->value.register_.number);
                    dst_str = fob_operand_to_string(&dst_8bit, backend);
                }
                    */

                code = malloc(strlen(dst_str) + 8);
                sprintf(code, "not %s\n", dst_str);
            }
            break;
        case FOB_OP_BOOL_NOT: {
            if (is_float) {
                code = calloc(1, sizeof(char));
                assert(false);
            } else {
                // NEED 8 bit register.
                FOB_Operand dst_8bit = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_BOOL), dst->value.register_.number);
                char *dst_8bit_str = fob_operand_to_string(&dst_8bit, backend);

                // We want the whole register to be this boolean value.
                FOB_Op load = (FOB_Op){ .type = FOB_OP_LOAD_MEMORY, .destination = *dst, .source = dst_8bit };
                char *load_str = emit_load_memory(&load, backend);

                code = malloc(strlen(dst_str) + strlen(load_str) + 24);
                sprintf(code, "test %s, %s\n"
                              "setnz %s\n%s", dst_str, dst_str, dst_8bit_str, load_str);

                free(load_str);
                free(dst_8bit_str);
            }
            break;
        }
        case FOB_OP_NEG: {
            if (is_float) {
                FOB_Operand temp_reg = get_temporary_register_2(prim_type);
                src_str = fob_operand_register_to_string(&temp_reg, true);

                // TOFIX: @hardcoded
                code = malloc(strlen(dst_str) + (strlen(src_str) * 2) + 45);
                sprintf(code, "mov rcx, -1\n"
                              "cvtsi2s%c %s, rcx\n"
                              "muls%c %s, %s\n", float_word_char(prim_type), src_str, 
                              float_word_char(prim_type), dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + 8);
                sprintf(code, "neg %s\n", dst_str);
            }
            break;
        }
        case FOB_OP_ADD:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 15);
                sprintf(code, "adds%c %s, %s\n", float_word_char(prim_type), dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "add %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_SUB:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 15);
                sprintf(code, "subs%c %s, %s\n", float_word_char(prim_type), dst_str, src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 12);
                sprintf(code, "sub %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_MUL:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 15);
                sprintf(code, "muls%c %s, %s\n", float_word_char(prim_type), dst_str, src_str);
            } else if (fob_primitive_is_unsigned(prim_type)) {
                assert(dst->value.register_.number == FOB_HW_RAX);
                code = malloc(strlen(src_str) + 8);
                sprintf(code, "mul %s\n", src_str);
            } else {
                code = malloc(strlen(dst_str) + strlen(src_str) + 13);
                sprintf(code, "imul %s, %s\n", dst_str, src_str);
            }
            break;
        case FOB_OP_DIV:
            if (is_float) {
                code = malloc(strlen(dst_str) + strlen(src_str) + 15);
                sprintf(code, "divs%c %s, %s\n", float_word_char(prim_type), dst_str, src_str);
            } else if (fob_primitive_is_unsigned(prim_type)) {
                assert(dst->value.register_.number == FOB_HW_RAX);
                code = malloc(strlen(src_str) + 21);
                sprintf(code, "xor rdx, rdx\n"
                              "div %s\n", src_str);
            } else {
                assert(dst->value.register_.number == FOB_HW_RAX);
                code = malloc(strlen(src_str) + 24);
                sprintf(code, "%s\n"
                              "idiv %s\n", primitive_type_to_word_size(prim_type) == FOB_WORD_DWORD ? "cdq" : "cqo", src_str);
            }
            break;
        case FOB_OP_MOD:
            if (is_float) {
                fob_alert("MOD instruction with floating-point result type\n");
                code = calloc(1, sizeof(char));
                break;
            }

            if (fob_primitive_is_unsigned(prim_type)) {
                assert(dst->value.register_.number == FOB_HW_RAX);
                FOB_Operand temp_reg = fob_create_fob_operand_register(fob_create_data_type(prim_type), FOB_HW_RDX);
                char *temp_rdx = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(dst_str) + strlen(src_str) + strlen(temp_rdx) + 32);
                sprintf(code, "xor rdx, rdx\n"
                              "div %s\n"
                              "mov %s, %s\n", src_str, dst_str, temp_rdx);
                free(temp_rdx);
            } else {
                assert(dst->value.register_.number == FOB_HW_RAX);

                FOB_Operand temp_reg = fob_create_fob_operand_register(fob_create_data_type(prim_type), FOB_HW_RDX);
                char *temp_rdx = fob_operand_register_to_string(&temp_reg, true);

                code = malloc(strlen(src_str) + strlen(temp_rdx) + strlen(dst_str) + 33);
                sprintf(code, "xor rdx, rdx\n"
                              "idiv %s\n"
                              "mov %s, %s\n", src_str, dst_str, temp_rdx);
                free(temp_rdx);
            }
            break;
        case FOB_OP_SHL:
        case FOB_OP_SHR: {
            if (is_float) {
                fob_alert("SHL instruction with floating-point result type\n");
                code = calloc(1, sizeof(char));
                break;
            }

            assert(src->type == FOB_OPER_REGISTER);
            assert(ir_register_to_hardware_register_number(src->value.register_.number, false) == FOB_HW_RCX);

            FOB_Operand src_8bit = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I8), FOB_HW_RCX);
            char *src_8bit_str = fob_operand_register_to_string(&src_8bit, true);
            code = malloc(strlen(dst_str) + strlen(src_8bit_str) + 14);
            sprintf(code, "sa%c %s, %s\n", op->type == FOB_OP_SHL ? 'l' : 'r', dst_str, src_8bit_str);
            free(src_8bit_str);
            break;
        }
        default:
            fob_alert("Uncaught arithmetic type %u\n", op->type);
            code = calloc(1, sizeof(char));
            break;
    }

    free(dst_str);

    if (src_str != NULL)
        free(src_str);

    return code;
}

static char *emit_cmp(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    const FOB_Operand *src = &op->source;
    assert(dst->type == FOB_OPER_REGISTER);
    assert(dst->data_type.primitive_type == src->data_type.primitive_type);
    const FOB_Primitive_Type prim_type = dst->data_type.primitive_type;

    char *dst_str = fob_operand_to_string(dst, backend);
    char *src_str = fob_operand_to_string(src, backend);
    char *code;

    if (fob_primitive_is_float(prim_type)) {
        code = malloc(strlen(dst_str) + strlen(src_str) + 11);
        sprintf(code, "comis%c %s, %s\n", float_word_char(prim_type), dst_str, src_str);
    } else {
        code = malloc(strlen(dst_str) + strlen(src_str) + 8);
        sprintf(code, "cmp %s, %s\n", dst_str, src_str);
    }

    free(dst_str);
    free(src_str);
    return code;
}

static char *emit_cmp_operator(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    const FOB_Operand *src = &op->source;
    assert(dst->type == FOB_OPER_REGISTER);
    const bool is_float = fob_data_type_is_float(&src->data_type);

    char *dst_str = fob_operand_to_string(dst, backend);
    char *code = malloc(strlen(dst_str) + 11);

    switch (op->type) {
        case FOB_OP_CMP_EQ:
            sprintf(code, "sete %s\n", dst_str);
            break;
        case FOB_OP_CMP_NEQ:
            sprintf(code, "setne %s\n", dst_str);
            break;
        case FOB_OP_CMP_LT:
            sprintf(code, "set%c %s\n", is_float ? 'b' : 'l', dst_str);
            break;
        case FOB_OP_CMP_LTE:
            sprintf(code, "set%ce %s\n", is_float ? 'b' : 'l', dst_str);
            break;
        case FOB_OP_CMP_GT:
            sprintf(code, "set%c %s\n", is_float ? 'a' : 'g', dst_str);
            break;
        case FOB_OP_CMP_GTE:
            sprintf(code, "set%ce %s\n", is_float ? 'a' : 'g', dst_str);
            break;
        default:
            fob_alert("Uncaught comparison operator type %u\n", op->type);
            code[0] = '\0';
            break;
    }

    free(dst_str);
    return code;
}

static char *emit_jmp(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    assert(dst->type == FOB_OPER_LABEL);

    char *dst_str = fob_operand_to_string(dst, backend);
    char *code = malloc(strlen(dst_str) + 8);
    sprintf(code, "jmp %s\n", dst_str);
    free(dst_str);
    return code;
}

static char *emit_jmp_bool(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *dst = &op->destination;
    const FOB_Operand *src = &op->source;
    assert(dst->type == FOB_OPER_LABEL);
    assert(src->type == FOB_OPER_REGISTER);

    FOB_Operand src_reg = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I8), src->value.register_.number);
    char *src_str = fob_operand_register_to_string(&src_reg, false);
    char *dst_str = fob_operand_to_string(dst, backend);

    char *code = malloc(strlen(dst_str) + (strlen(src_str) * 2) + 22);
    sprintf(code, "test %s, %s\n"
                  "j%s %s\n", src_str, src_str, op->type == FOB_OP_JMP_TRUE ? "nz" : "z", dst_str);

    free(dst_str);
    free(src_str);
    return code;
}

static char *emit_new_label(const FOB_Op *op) {
    const FOB_Operand *src = &op->source;
    assert(src->type == FOB_OPER_LABEL);

    char *code = malloc(24);
    sprintf(code, ".l%zu:\n", src->value.label.number);
    return code;
}

static char *emit_inline_asm(const FOB_Op *op) {
    const char *str = op->source.value.string.string;
    const size_t str_len = strlen(op->source.value.string.string);

    char *code = malloc(str_len + 2);
    size_t len = 0;
    code[0] = '\0';

    for (size_t i = 0; i < str_len; i++) {
        if (str[i] == '\\' && i + 1 < str_len && str[i + 1] != '\\') {
            char value = 0;

            switch (str[i + 1]) {
                case 'n':
                    value = 10;
                    break;
                case 't':
                    value = 9;
                    break;
                case 'r':
                    value = 13;
                    break;
                case '0':
                    value = 0;
                    break;
                case '\'':
                    value = '\'';
                    break;
                case '"':
                    value = '"';
                    break;
                default:
                    fob_alert("Uncaught escape sequence '\\%c'\n", str[i + 1]);
                    break;
            }

            code[len++] = value;
            i++;
        } else
            code[len++] = str[i];
    }

    code[len] = '\n';
    code[len + 1] = '\0';
    return code;
}

static char *emit_ref(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *src = &op->source;
    const FOB_Operand *dst = &op->destination;
    assert(dst->type == FOB_OPER_REGISTER);

    char *src_str = fob_operand_to_string(src, backend);
    char *dst_str = fob_operand_to_string(dst, backend);

    char *code = malloc(strlen(src_str) + strlen(dst_str) + 12);
    sprintf(code, "lea %s, %s\n", dst_str, src_str);

    free(dst_str);
    free(src_str);
    return code;
}

static char *emit_call(const FOB_Op *op, FOB_Backend *backend) {
    const FOB_Operand *src = &op->source;
    assert(src->type == FOB_OPER_IDENTIFIER || src->type == FOB_OPER_REGISTER);
    char *src_str;
    
    if (src->type == FOB_OPER_REGISTER) {
        // Ensure full 64-bit register.
        FOB_Operand src_reg_64 = fob_create_fob_operand_register(fob_create_data_type(FOB_PRIM_I64), src->value.register_.number);
        src_str = fob_operand_register_to_string(&src_reg_64, false);
    } else
        src_str = fob_operand_to_string(src, backend);

    char *code = malloc(strlen(src_str) + 32);
    sprintf(code, "call %s\n", src_str);

    free(src_str);
    return code;
}

static char *emit_spill_arg(const FOB_Op *op, FOB_Backend *backend) {
    assert(op->source.type == FOB_OPER_ARGUMENT);

    if (!is_register_argument(&op->source)) {
        // TODO @implement:
        fob_alert("Stack parameters aren't yet implemented\n");
        return calloc(1, sizeof(char));
    }

    return emit_push(op, backend);
}

static char *emit_unspill_arg(const FOB_Op *op, FOB_Backend *backend) {
    assert(op->destination.type == FOB_OPER_ARGUMENT);

    if (!is_register_argument(&op->destination)) {
        // TODO @implement:
        fob_alert("Stack parameters aren't yet implemented\n");
        return calloc(1, sizeof(char));
    }

    return emit_pop(op, backend);
}

static char *fob_emit_op(const FOB_Op *op, FOB_Backend *backend) {
    switch (op->type) {
        case FOB_OP_NOP:
        case FOB_OP_EOF: return calloc(1, sizeof(char));
        case FOB_OP_FUNC_START: return emit_func_start(op, backend);
        case FOB_OP_FUNC_END: return emit_func_end(backend);
        case FOB_OP_RETURN: return emit_return(op, backend);
        case FOB_OP_LOAD_CONST: return emit_load_const(op, backend);
        case FOB_OP_LOAD_MEMORY: return emit_load_memory(op, backend);
        case FOB_OP_NEW_LOCAL_VARIABLE: return emit_new_local_variable(op, backend);
        case FOB_OP_STORE_MEMORY: return emit_store_memory(op, backend);
        case FOB_OP_PUSH: return emit_push(op, backend);
        case FOB_OP_POP: return emit_pop(op, backend);
        case FOB_OP_AND:
        case FOB_OP_OR:
        case FOB_OP_XOR:
        case FOB_OP_NOT:
        case FOB_OP_BOOL_NOT:
        case FOB_OP_ADD:
        case FOB_OP_SUB:
        case FOB_OP_MUL:
        case FOB_OP_DIV:
        case FOB_OP_MOD:
        case FOB_OP_SHL:
        case FOB_OP_SHR:
        case FOB_OP_NEG: return emit_arithmetic(op, backend);
        case FOB_OP_CMP: return emit_cmp(op, backend);
        case FOB_OP_CMP_EQ:
        case FOB_OP_CMP_NEQ:
        case FOB_OP_CMP_LT:
        case FOB_OP_CMP_LTE:
        case FOB_OP_CMP_GT:
        case FOB_OP_CMP_GTE: return emit_cmp_operator(op, backend);
        case FOB_OP_JMP: return emit_jmp(op, backend);
        case FOB_OP_JMP_TRUE:
        case FOB_OP_JMP_FALSE: return emit_jmp_bool(op, backend);
        case FOB_OP_NEW_LABEL: return emit_new_label(op);
        case FOB_OP_INLINE_ASM: return emit_inline_asm(op);
        case FOB_OP_BLOCK_START: return emit_block_start(backend);
        case FOB_OP_BLOCK_END: return emit_block_end(backend);
        case FOB_OP_REF: return emit_ref(op, backend);
        case FOB_OP_CALL: return emit_call(op, backend);
        case FOB_OP_SPILL_ARG: return emit_spill_arg(op, backend);
        case FOB_OP_UNSPILL_ARG: return emit_unspill_arg(op, backend);
        default: break;
    }

    fob_alert("Uncaught op %u\n", op->type);
    return calloc(1, sizeof(char));
}

char *fob_emit_ir(const FOB_IR *ir, const char *entrypoint_name) {
    FOB_Backend backend = create_backend();
    char *code;
    
    if (entrypoint_name == NULL)
        code = calloc(1, sizeof(char));
    else {
        const char *template = "default rel\nsection .text\nglobal _start\n_start:\nmov rdi, qword [rsp]\nlea rsi, [rsp+8]\ncall %s_\nmov rdi, rax\nmov rax, 60\nsyscall\n";
        code = malloc(strlen(entrypoint_name) + strlen(template) + 1);
        sprintf(code, template, entrypoint_name);
    }

    for (size_t i = 0; i < ir->count; i++) {
        char *str = fob_emit_op(&ir->ops[i], &backend);
        code = realloc(code, strlen(code) + strlen(str) + 1);
        strcat(code, str);
        free(str);
    }

    delete_backend(&backend);
    return code;
}

size_t data_type_to_hardware_byte_size(const FOB_Data_Type *data_type) {
    return primitive_type_to_allocation_size(fob_data_type_to_primitive_type(data_type));
}
