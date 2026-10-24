// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#include "fob_ir.h"
#include "fob_error.h"
#include "fob_data_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

static char *ir_operand_int_to_string(const FOB_Operand *operand) {
    const FOB_Operand_Int *int_ = &operand->value.int_;
    char *str = malloc(24);

    if (int_->primitive_type == FOB_PRIM_I32)
        sprintf(str, "%" PRId32, int_->int_const.i32);
    else if (int_->primitive_type == FOB_PRIM_I64)
        sprintf(str, "%" PRId64, int_->int_const.i64);
    else if (int_->primitive_type == FOB_PRIM_U32)
        sprintf(str, "%" PRIu32, int_->int_const.u32);
    else if (int_->primitive_type == FOB_PRIM_U64)
        sprintf(str, "%" PRIu64, int_->int_const.u64);
    else {
        fob_alert("Uncaught int const primitive type %u\n", int_->primitive_type);
        str[0] = '\0';
    }

    return str;
}

static char *ir_operand_float_to_string(const FOB_Operand *operand) {
    char *str = malloc(24);

    if (operand->value.float_.primitive_type == FOB_PRIM_F32)
        sprintf(str, "%f", operand->value.float_.float_const.f32);
    else
        sprintf(str, "%lf", operand->value.float_.float_const.f64);

    return str;
}

static char *ir_operand_register_to_string(const FOB_Operand *operand) {
    char *str = malloc(16);
    sprintf(str, "%%%zu", operand->value.register_.number);
    return str;
}

static char *ir_operand_identifier_to_string(const char *identifier) {
    char *str = malloc(strlen(identifier) + 2);
    sprintf(str, "@%s", identifier);
    return str;
}

static char *ir_operand_variable_to_string(const FOB_Operand *operand) {
    return ir_operand_identifier_to_string(operand->value.variable.name);
}

static char *ir_operand_label_to_string(const FOB_Operand *operand) {
    char *str = malloc(16);
    sprintf(str, "%zu", operand->value.label.number);
    return str;
}

static char *ir_operand_argument_to_string(const FOB_Operand *operand) {
    char *str = malloc(17);
    sprintf(str, "#%zu", operand->value.argument.index);
    return str;
}

static char *ir_operand_deref_to_string(const FOB_Operand *operand) {
    char *str = malloc(32);
    sprintf(str, "deref %%%zu", operand->value.deref.register_number);
    return str;
}

static char *ir_operand_to_string(const FOB_Operand *operand) {
    char *str;

    switch (operand->type) {
        case FOB_OPER_NONE:
            str = calloc(1, sizeof(char));
            break;
        case FOB_OPER_INT:
            str = ir_operand_int_to_string(operand);
            break;
        case FOB_OPER_FLOAT:
            str = ir_operand_float_to_string(operand);
            break;
        case FOB_OPER_REGISTER:
            str = ir_operand_register_to_string(operand);
            break;
        case FOB_OPER_IDENTIFIER:
            str = ir_operand_identifier_to_string(operand->value.identifier.name);
            break;
        case FOB_OPER_VARIABLE:
            str = ir_operand_variable_to_string(operand);
            break;
        case FOB_OPER_LABEL:
            str = ir_operand_label_to_string(operand);
            break;
        case FOB_OPER_ARGUMENT:
            str = ir_operand_argument_to_string(operand);
            break;
        case FOB_OPER_DEREF:
            str = ir_operand_deref_to_string(operand);
            break;
        default:
            fob_alert("Uncaught operand type %u\n", operand->type);
            str = calloc(1, sizeof(char));
            break;
    }

    char *dt = fob_data_type_to_string(&operand->data_type);
    dt = realloc(dt, strlen(dt) + strlen(str) + 2);
    strcat(dt, " ");
    strcat(dt, str);
    free(str);
    return dt;
}

static char *ir_func_start_to_string(const FOB_Op *op) {
    char *type = fob_data_type_to_string(&op->source.data_type);
    char *ident = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(ident) + strlen(type) + 14);
    sprintf(str, "%s func %s\n{\n", type, ident);
    free(type);
    free(ident);
    return str;
}

static char *ir_func_end_to_string() {
    char *str = malloc(3);
    strcpy(str, "}\n");
    return str;
}

static char *ir_return_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 13);
    sprintf(str, "ret %s = %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_load_or_store_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s %c= %s\n", dst, op->type == FOB_OP_STORE_MEMORY ? 's' : 'l', src);
    free(dst);
    free(src);
    return str;
}

static char *ir_new_local_variable_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 8);
    sprintf(str, "new %s\n", dst);
    free(dst);
    return str;
}

static char *ir_push_to_string(const FOB_Op *op) {
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(src) + 9);
    sprintf(str, "push %s\n", src);
    free(src);
    return str;
}

static char *ir_pop_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 8);
    sprintf(str, "pop %s\n", dst);
    free(dst);
    return str;
}

static char *ir_and_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s &= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_or_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s |= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_xor_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s ^= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_not_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc((strlen(dst) * 2) + 10);
    sprintf(str, "%s = ~%s\n", dst, dst);
    free(dst);
    return str;
}

static char *ir_add_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s += %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_sub_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s -= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_mul_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s *= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_div_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s /= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_mod_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 10);
    sprintf(str, "%s %%= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_shl_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 11);
    sprintf(str, "%s <<= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_shr_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 11);
    sprintf(str, "%s >>= %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_cmp_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 9);
    sprintf(str, "%s ? %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_cmp_eq_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 7);
    sprintf(str, "== %s\n", dst);
    free(dst);
    return str;
}

static char *ir_cmp_neq_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 7);
    sprintf(str, "!= %s\n", dst);
    free(dst);
    return str;
}

static char *ir_cmp_lt_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 6);
    sprintf(str, "< %s\n", dst);
    free(dst);
    return str;
}

static char *ir_cmp_lte_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 7);
    sprintf(str, "<= %s\n", dst);
    free(dst);
    return str;
}

static char *ir_cmp_gt_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 6);
    sprintf(str, "> %s\n", dst);
    free(dst);
    return str;
}

static char *ir_cmp_gte_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 7);
    sprintf(str, ">= %s\n", dst);
    free(dst);
    return str;
}

static char *ir_jmp_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 9);
    sprintf(str, "goto %s\n", dst);
    free(dst);
    return str;
}

static char *ir_jmp_true_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 15);
    sprintf(str, "goto %s if %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_jmp_false_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 16);
    sprintf(str, "goto %s if !%s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_new_label_to_string(const FOB_Op *op) {
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(src) + 5);
    sprintf(str, "%s:\n", src);
    free(src);
    return str;
}

static char *ir_inline_asm_to_string(const FOB_Op *op) {
    char *str = malloc(strlen(op->source.value.string.string) + 9);
    sprintf(str, "asm(\"%s\")\n", op->source.value.string.string);
    return str;
}

static char *ir_ref_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(dst) + strlen(src) + 13);
    sprintf(str, "%s = ptr %s\n", dst, src);
    free(dst);
    free(src);
    return str;
}

static char *ir_call_to_string(const FOB_Op *op) {
    char *ident = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(ident) + 4);
    sprintf(str, "%s()\n", ident);
    free(ident);
    return str;
}

static char *ir_spill_arg_to_string(const FOB_Op *op) {
    char *src = ir_operand_to_string(&op->source);
    char *str = malloc(strlen(src) + 10);
    sprintf(str, "spill %s\n", src);
    free(src);
    return str;
}

static char *ir_unspill_arg_to_string(const FOB_Op *op) {
    char *dst = ir_operand_to_string(&op->destination);
    char *str = malloc(strlen(dst) + 12);
    sprintf(str, "unspill %s\n", dst);
    free(dst);
    return str;
}

static char *ir_op_to_string(const FOB_Op *op) {
    switch (op->type) {
        case FOB_OP_EOF:
        case FOB_OP_NOP: return calloc(1, sizeof(char));
        case FOB_OP_FUNC_START: return ir_func_start_to_string(op);
        case FOB_OP_FUNC_END: return ir_func_end_to_string();
        case FOB_OP_RETURN: return ir_return_to_string(op);
        case FOB_OP_LOAD_CONST:
        case FOB_OP_LOAD_MEMORY:
        case FOB_OP_STORE_MEMORY: return ir_load_or_store_to_string(op);
        case FOB_OP_NEW_LOCAL_VARIABLE: return ir_new_local_variable_to_string(op);
        case FOB_OP_PUSH: return ir_push_to_string(op);
        case FOB_OP_POP: return ir_pop_to_string(op);
        case FOB_OP_AND: return ir_and_to_string(op);
        case FOB_OP_OR: return ir_or_to_string(op);
        case FOB_OP_XOR: return ir_xor_to_string(op);
        case FOB_OP_NOT: return ir_not_to_string(op);
        case FOB_OP_ADD: return ir_add_to_string(op);
        case FOB_OP_SUB: return ir_sub_to_string(op);
        case FOB_OP_MUL: return ir_mul_to_string(op);
        case FOB_OP_DIV: return ir_div_to_string(op);
        case FOB_OP_MOD: return ir_mod_to_string(op);
        case FOB_OP_SHL: return ir_shl_to_string(op);
        case FOB_OP_SHR: return ir_shr_to_string(op);
        case FOB_OP_CMP: return ir_cmp_to_string(op);
        case FOB_OP_CMP_EQ: return ir_cmp_eq_to_string(op);
        case FOB_OP_CMP_NEQ: return ir_cmp_neq_to_string(op);
        case FOB_OP_CMP_LT: return ir_cmp_lt_to_string(op);
        case FOB_OP_CMP_LTE: return ir_cmp_lte_to_string(op);
        case FOB_OP_CMP_GT: return ir_cmp_gt_to_string(op);
        case FOB_OP_CMP_GTE: return ir_cmp_gte_to_string(op);
        case FOB_OP_JMP: return ir_jmp_to_string(op);
        case FOB_OP_JMP_TRUE: return ir_jmp_true_to_string(op);
        case FOB_OP_JMP_FALSE: return ir_jmp_false_to_string(op);
        case FOB_OP_NEW_LABEL: return ir_new_label_to_string(op);
        case FOB_OP_INLINE_ASM: return ir_inline_asm_to_string(op);
        case FOB_OP_BLOCK_START:
        case FOB_OP_BLOCK_END: {
            char *str = malloc(3);
            strcpy(str, op->type == FOB_OP_BLOCK_START ? "{\n" : "}\n");
            return str;
        }
        case FOB_OP_REF: return ir_ref_to_string(op);
        case FOB_OP_CALL: return ir_call_to_string(op);
        case FOB_OP_SPILL_ARG: return ir_spill_arg_to_string(op);
        case FOB_OP_UNSPILL_ARG: return ir_unspill_arg_to_string(op);
        default: break;
    }

    fob_alert("Uncaught op type %u\n", op->type);
    return calloc(1, sizeof(char));
}

char *fob_ir_to_string(const FOB_IR *ir) {
    char *str = calloc(1, sizeof(char));

    for (size_t i = 0; i < ir->count; i++) {
        char *op = ir_op_to_string(&ir->ops[i]);
        str = realloc(str, strlen(str) + strlen(op) + 1);
        strcat(str, op);
        free(op);
    }

    return str;
}