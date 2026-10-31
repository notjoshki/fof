// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#include "fob_ir.h"
#include "fob_ast.h"
#include "fob_data_type.h"
#include "fob_error.h"
#include "fob_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define IR_CAPACITY 8

#define nop (FOB_Operand){ .type = FOB_OPER_NONE, .data_type = fob_create_data_type(FOB_PRIM_VOID) }

#define t1(data_type) fob_create_fob_operand_register(data_type, FOB_TEMPORARY_IR_REGISTER_1)
#define t2(data_type) fob_create_fob_operand_register(data_type, FOB_TEMPORARY_IR_REGISTER_2)

typedef enum {
    FOB_ARITHMETIC_LOWEST_PRECEDENCE,
    FOB_ARITHMETIC_MIDDLE_PRECEDENCE,
    FOB_ARITHMETIC_HIGHEST_PRECEDENCE
} FOB_Arithmetic_Precedence;

static bool ast_is_complex_operand(const FOB_AST *ast) {
    switch (ast->type) {
        case FOB_AST_ARITHMETIC:
        case FOB_AST_CONDITION:
        case FOB_AST_CALL:
        case FOB_AST_DEREFERENCE:
        case FOB_AST_INDEX:
        case FOB_AST_PRE_OPER:
        case FOB_AST_POST_OPER: return true;
        default: return false;
    }
}

static void push_op(FOB_IR *ir, FOB_Opcode type, FOB_Operand destination, FOB_Operand source) {
    if (ir->count + 1 > ir->capacity) {
        ir->capacity *= 2;
        ir->ops = realloc(ir->ops, ir->capacity * sizeof(FOB_Op));
    }

    ir->ops[ir->count++] = (FOB_Op){ .type = type, .destination = destination, .source = source };
}

static void push_ast(FOB_IR *ir, const FOB_AST *ast);
static void push_load(FOB_IR *ir, FOB_Operand destination, FOB_Operand source);
static FOB_Operand ast_to_fob_operand(FOB_IR *ir, const FOB_AST *ast);

static FOB_Operand ast_int_to_operand(const FOB_AST *ast) {
    FOB_Operand oper;
    FOB_Primitive_Type prim = ast->value.constant.data_type.primitive_type;

    if (prim == FOB_PRIM_I32)
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I32), (FOB_Operand_Int_Const){ .i32 = ast->value.constant.value.i32 });
    else if (prim == FOB_PRIM_I64)
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I64), (FOB_Operand_Int_Const){ .i64 = ast->value.constant.value.i64 });
    else if (prim == FOB_PRIM_U32)
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_U32), (FOB_Operand_Int_Const){ .u32 = ast->value.constant.value.u32 });
    else if (prim == FOB_PRIM_U64)
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_U64), (FOB_Operand_Int_Const){ .u64 = ast->value.constant.value.u64 });
    else {
        fob_alert("Uncaught constant primitive type %u\n", prim);
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I32), (FOB_Operand_Int_Const){ .i32 = 0 });
    }

    return oper;
}

static FOB_Operand ast_float_to_operand(const FOB_AST *ast) {
    FOB_Operand oper;
    const FOB_AST_Constant *constant = &ast->value.constant;

    if (constant->data_type.primitive_type == FOB_PRIM_F32)
        oper = fob_create_fob_operand_float(fob_create_data_type(FOB_PRIM_F32), (FOB_Operand_Float_Const){ .f32 = constant->value.f32 });
    else if (constant->data_type.primitive_type == FOB_PRIM_F64)
        oper = fob_create_fob_operand_float(fob_create_data_type(FOB_PRIM_F64), (FOB_Operand_Float_Const){ .f64 = constant->value.f64 });
    else {
        fob_alert("Uncaught constant primitive type %u\n", constant->data_type.primitive_type);
        oper = fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I32), (FOB_Operand_Int_Const){ .i32 = 0 });
    }

    return oper;
}

static FOB_Operand ast_string_to_operand(const FOB_AST *ast) {
    return fob_create_fob_operand_string(ast->value.string.text);
}

static FOB_Operand ast_variable_to_operand(const FOB_AST *ast) {
    return fob_create_fob_operand_variable(ast->value.variable.name, ast->value.variable.uid, ast->value.variable.data_type);
}

static FOB_Arithmetic_Precedence arithmetic_operator_to_precedence(const FOB_AST_Operator_Type type) {
    switch (type) {
        case FOB_AST_OPERATOR_AND:
        case FOB_AST_OPERATOR_OR:
        case FOB_AST_OPERATOR_XOR:
        case FOB_AST_OPERATOR_NOT:
        case FOB_AST_OPERATOR_SHL:
        case FOB_AST_OPERATOR_SHR: return FOB_ARITHMETIC_LOWEST_PRECEDENCE;
        case FOB_AST_OPERATOR_ADD:
        case FOB_AST_OPERATOR_SUB: return FOB_ARITHMETIC_MIDDLE_PRECEDENCE;
        default: return FOB_ARITHMETIC_HIGHEST_PRECEDENCE;
    }
}

static FOB_Opcode arithmetic_operator_to_opcode(const FOB_AST_Operator_Type type) {
    switch (type) {
        case FOB_AST_OPERATOR_AND: return FOB_OP_AND;
        case FOB_AST_OPERATOR_OR: return FOB_OP_OR;
        case FOB_AST_OPERATOR_XOR: return FOB_OP_XOR;
        case FOB_AST_OPERATOR_NOT: return FOB_OP_NOT;
        case FOB_AST_OPERATOR_ADD: return FOB_OP_ADD;
        case FOB_AST_OPERATOR_SUB: return FOB_OP_SUB;
        case FOB_AST_OPERATOR_MUL: return FOB_OP_MUL;
        case FOB_AST_OPERATOR_DIV: return FOB_OP_DIV;
        case FOB_AST_OPERATOR_MOD: return FOB_OP_MOD;
        case FOB_AST_OPERATOR_SHL: return FOB_OP_SHL;
        case FOB_AST_OPERATOR_SHR: return FOB_OP_SHR;
        default: break;
    }

    fob_alert("Uncaught operator type %u\n", type);
    return 0;
}

static void do_single_arithmetic_operation(FOB_IR *ir, const FOB_AST_Operator_Type type, const FOB_Data_Type data_type) {
    push_op(ir, FOB_OP_POP, t2(data_type), nop);
    push_op(ir, FOB_OP_POP, t1(data_type), nop);
    push_op(ir, arithmetic_operator_to_opcode(type), t1(data_type), t2(data_type));
    push_op(ir, FOB_OP_PUSH, nop, t1(data_type));
}

static inline FOB_AST_Operator_Type get_ast_operator_enum(const FOB_AST *ast) {
    return ast->type == FOB_AST_OPERATOR ? ast->value.operator.type : 0;
}

static FOB_Operand ast_arithmetic_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_AST_List *values = &ast->value.arithmetic.expression;
    const FOB_Data_Type dt = ast->value.arithmetic.result_type;

    FOB_AST_Operator_Type delayed_operators[values->count];
    size_t delayed_count = 0;

    for (size_t i = 0; i < values->count; i++) {
        FOB_AST *item = values->nodes[i];

        if (i % 2 != 0) {
            const FOB_AST_Operator_Type oper = get_ast_operator_enum(item);
            const FOB_Arithmetic_Precedence this_precedence = arithmetic_operator_to_precedence(oper);

            while (delayed_count > 0 && arithmetic_operator_to_precedence(delayed_operators[delayed_count - 1]) >= this_precedence)
                do_single_arithmetic_operation(ir, delayed_operators[--delayed_count], dt);

            delayed_operators[delayed_count++] = oper;
            continue;
        }

        push_load(ir, t1(dt), ast_to_fob_operand(ir, item));
        push_op(ir, FOB_OP_PUSH, nop, t1(dt));
    }

    while (delayed_count > 0)
        do_single_arithmetic_operation(ir, delayed_operators[--delayed_count], dt);

    push_op(ir, FOB_OP_POP, t1(dt), nop);
    return t1(dt);
}

static FOB_Opcode conditional_operator_to_opcode(const FOB_AST_Operator_Type type) {
    switch (type) {
        case FOB_AST_OPERATOR_EQ: return FOB_OP_CMP_EQ;
        case FOB_AST_OPERATOR_NEQ: return FOB_OP_CMP_NEQ;
        case FOB_AST_OPERATOR_LT: return FOB_OP_CMP_LT;
        case FOB_AST_OPERATOR_LTE: return FOB_OP_CMP_LTE;
        case FOB_AST_OPERATOR_GT: return FOB_OP_CMP_GT;
        case FOB_AST_OPERATOR_GTE: return FOB_OP_CMP_GTE;
        default: break;
    }

    fob_alert("Uncaught operator type %u\n", type);
    return 0;
}

static FOB_Operand ast_condition_to_operand(FOB_IR *ir, const FOB_AST *ast, const bool as_a_value) {
    const FOB_AST_List *values = &ast->value.condition.expression;
    const FOB_Data_Type dt = ast->value.condition.comparison_type;
    const FOB_Data_Type bool_dt = fob_create_data_type(FOB_PRIM_BOOL);
    const FOB_Data_Type sum_dt = fob_create_data_type(FOB_PRIM_I32);
    FOB_Operand true_label;

    // If more than 1 comparisons are made, we need to sum them all up to equal true.
    if (values->count > 3) {
        true_label = fob_create_fob_operand_label(ir->label_count++);
        push_load(ir, t1(sum_dt),
            fob_create_fob_operand_int(sum_dt, (FOB_Operand_Int_Const){ .i32 = 0 }));

        push_op(ir, FOB_OP_PUSH, nop, t1(sum_dt));
    }

    FOB_Operand false_label;

    if (as_a_value)
        false_label = fob_create_fob_operand_label(ir->label_count++);

    for (size_t i = 1; i < values->count; i += 4) {
        const FOB_AST *lhs = values->nodes[i - 1];
        const FOB_AST *rhs = values->nodes[i + 1];
        const FOB_AST_Operator_Type oper = get_ast_operator_enum(values->nodes[i]);

        // The next and last &&/|| if present.
        const FOB_AST_Operator_Type next_chain = i + 2 == values->count ? 0 : get_ast_operator_enum(values->nodes[i + 2]);
        const FOB_AST_Operator_Type last_chain = (int)i - 2 < 0 ? 0 : get_ast_operator_enum(values->nodes[(int)i - 2]);

        push_load(ir, t1(dt), ast_to_fob_operand(ir, lhs));
        push_op(ir, FOB_OP_PUSH, nop, t1(dt));

        push_load(ir, t1(dt), ast_to_fob_operand(ir, rhs));
        push_op(ir, FOB_OP_PUSH, nop, t1(dt));

        push_op(ir, FOB_OP_POP, t2(dt), nop);
        push_op(ir, FOB_OP_POP, t1(dt), nop);

        push_op(ir, FOB_OP_CMP, t1(dt), t2(dt));

        push_op(ir, conditional_operator_to_opcode(oper), t1(bool_dt), 
            // vvv This so we know the data type for the comparison instruction.
            t1(dt));

        if (values->count == 3) // Just a single comparison.
            break;

        // Sum and push to the final equation.
        push_op(ir, FOB_OP_POP, t2(sum_dt), nop);

        push_op(ir, last_chain == FOB_AST_OPERATOR_BOOL_AND ? FOB_OP_AND : FOB_OP_OR, t1(bool_dt), t2(bool_dt));

        if (next_chain == FOB_AST_OPERATOR_BOOL_OR)
            push_op(ir, FOB_OP_JMP_TRUE, true_label, t1(bool_dt));
        else
            push_op(ir, FOB_OP_JMP_FALSE, false_label, t1(bool_dt));

        push_op(ir, FOB_OP_PUSH, nop, t1(sum_dt));
    }

    // If it isn't a raw value, i.e it's part of a if/while statement, we handle those in their respective functions, not here.
    if (values->count == 3 || !as_a_value)
        return t1(bool_dt);

    FOB_Operand done_label = fob_create_fob_operand_label(ir->label_count++);
    push_op(ir, FOB_OP_POP, t1(bool_dt), nop);
    push_op(ir, FOB_OP_NEW_LABEL, nop, true_label);
    push_op(ir, FOB_OP_JMP, done_label, nop);
    push_op(ir, FOB_OP_NEW_LABEL, nop, false_label);

    push_load(ir, t1(sum_dt), fob_create_fob_operand_int(sum_dt, (FOB_Operand_Int_Const){ .i32 = 0 }));
    push_op(ir, FOB_OP_NEW_LABEL, nop, done_label);

    // Just to ensure the boolean is 0 or 1.
    push_load(ir, t1(sum_dt), t1(bool_dt));
    return t1(bool_dt);
}

static FOB_Operand ast_unary_not_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type dt = fob_get_ast_data_type(ast->value.unary_not.value);
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.unary_not.value));
    push_op(ir, ast->value.unary_not.as_boolean ? FOB_OP_BOOL_NOT : FOB_OP_NOT, t1(dt), nop);
    return t1(dt);
}

static FOB_Operand ast_unary_negate_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type dt = fob_get_ast_data_type(ast->value.unary_not.value);
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.unary_not.value));
    push_op(ir, FOB_OP_NEG, t1(dt), nop);
    return t1(dt);
}

static FOB_Operand ast_pointer_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type dt = fob_get_ast_data_type(ast);
    push_op(ir, FOB_OP_REF, t1(dt), ast_to_fob_operand(ir, ast->value.pointer.value));
    return t1(dt);
}

static void push_call(FOB_IR *ir, const FOB_AST *ast) {
    size_t int_args = 0;
    size_t float_args = 0;

    for (size_t i = 0; i < ast->value.call.arguments.count; i++) {
        const FOB_AST *arg = ast->value.call.arguments.nodes[i];
        const FOB_Data_Type arg_type = fob_get_ast_data_type(arg);
        const FOB_Data_Type param_type = ast->value.call.expected_arguments_data_types[i];
        bool spilled = false;

        if (i > 0 && ast_is_complex_operand(arg)) {
            spilled = true;
            size_t spilled_ints = 0;
            size_t spilled_floats = 0;

            for (size_t j = 0; j < i; j++) {
                const FOB_Data_Type spill_param_type = ast->value.call.expected_arguments_data_types[j];
                push_op(ir, FOB_OP_SPILL_ARG, nop, fob_create_fob_operand_argument(j, spilled_ints, spilled_floats, spill_param_type));

                if (fob_data_type_is_float(&spill_param_type))
                    spilled_floats++;
                else
                    spilled_ints++;
            }
        }

        push_load(ir, t1(arg_type), ast_to_fob_operand(ir, arg));
        push_op(ir, FOB_OP_STORE_MEMORY, 
            fob_create_fob_operand_argument(i, int_args, float_args, param_type), t1(arg_type));

        if (spilled) {
            size_t spilled_ints = int_args;
            size_t spilled_floats = float_args;

            for (int j = (int)i - 1; j >= 0; j--) {
                const FOB_Data_Type spill_param_type = ast->value.call.expected_arguments_data_types[j];

                if (fob_data_type_is_float(&spill_param_type))
                    spilled_floats--;
                else
                    spilled_ints--;

                push_op(ir, FOB_OP_UNSPILL_ARG, fob_create_fob_operand_argument(j, spilled_ints, spilled_floats, spill_param_type), nop);
            }
        }

        if (fob_data_type_is_float(&param_type))
            float_args++;
        else
            int_args++;
    }

    push_op(ir, FOB_OP_CALL, nop, fob_create_fob_identifier(ast->value.call.name));
}

static FOB_Operand push_dereference(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type deref_type = fob_get_ast_data_type(ast);
    const FOB_Data_Type ptr_type = fob_get_ast_data_type(ast->value.dereference.value);

    push_load(ir, t2(ptr_type), ast_to_fob_operand(ir, ast->value.dereference.value));
    return fob_create_fob_operand_deref(FOB_TEMPORARY_IR_REGISTER_2, deref_type);
}

static FOB_Operand ast_index_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type index_type = fob_get_ast_data_type(ast);
    const FOB_Data_Type ptr_type = fob_get_ast_data_type(ast->value.index.pointer);

    // TOFIX: I know we only support 64 bit right now and possibly the forseeable future but consider adding USIZE support for constants.
    const FOB_Data_Type offset_type = fob_create_data_type(FOB_PRIM_U64);

    push_load(ir, t1(offset_type), ast_to_fob_operand(ir, ast->value.index.offset));
    push_load(ir, t2(offset_type), 
        fob_create_fob_operand_int(offset_type, (FOB_Operand_Int_Const){ .u64 = (uint64_t)data_type_to_hardware_byte_size(&index_type) }));

    push_op(ir, FOB_OP_MUL, t1(offset_type), t2(offset_type));
    push_load(ir, t2(offset_type), t1(offset_type));
    push_load(ir, t1(ptr_type), ast_to_fob_operand(ir, ast->value.index.pointer));
    push_op(ir, FOB_OP_ADD, t1(offset_type), t2(offset_type));
    return fob_create_fob_operand_deref(FOB_TEMPORARY_IR_REGISTER_1, index_type);
}

static FOB_Operand ast_pre_oper_to_operand(FOB_IR *ir, const FOB_AST *ast) {
    if (ast->value.pre_operation.type != FOB_AST_OPERATOR_ADD && ast->value.pre_operation.type != FOB_AST_OPERATOR_SUB) {
        fob_alert("Uncaught pre-operation type %u\n", ast->value.pre_operation.type);
        return nop;
    }

    const FOB_Data_Type dt = fob_get_ast_data_type(ast->value.pre_operation.value);
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.pre_operation.value));
    push_load(ir, t2(dt), fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I32), (FOB_Operand_Int_Const){ .i32 = 1 }));
    push_op(ir, arithmetic_operator_to_opcode(ast->value.pre_operation.type), t1(dt), t2(dt));

    if (!ast_is_complex_operand(ast->value.pre_operation.value)) {
        push_op(ir, FOB_OP_STORE_MEMORY, ast_to_fob_operand(ir, ast->value.pre_operation.value), t1(dt));
        return t1(dt);
    }

    push_op(ir, FOB_OP_PUSH, nop, t1(dt));
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.pre_operation.value));
    push_op(ir, FOB_OP_POP, t2(dt), nop);
    push_op(ir, FOB_OP_STORE_MEMORY, fob_create_fob_operand_deref(FOB_TEMPORARY_IR_REGISTER_1, dt), t2(dt));
    return t2(dt);
}

static FOB_Operand ast_post_oper_to_operand(FOB_IR *ir, const FOB_AST *ast, const bool as_value) {
    if (ast->value.post_operation.type != FOB_AST_OPERATOR_ADD && ast->value.post_operation.type != FOB_AST_OPERATOR_SUB) {
        fob_alert("Uncaught post-operation type %u\n", ast->value.post_operation.type);
        return nop;
    }

    const FOB_Data_Type dt = fob_get_ast_data_type(ast->value.post_operation.value);
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.post_operation.value));

    if (as_value)
        push_op(ir, FOB_OP_PUSH, nop, t1(dt));

    push_load(ir, t2(dt), fob_create_fob_operand_int(fob_create_data_type(FOB_PRIM_I32), (FOB_Operand_Int_Const){ .i32 = 1 }));
    push_op(ir, arithmetic_operator_to_opcode(ast->value.post_operation.type), t1(dt), t2(dt));
    push_op(ir, FOB_OP_STORE_MEMORY, ast_to_fob_operand(ir, ast->value.post_operation.value), t1(dt));

    if (as_value) {
        push_op(ir, FOB_OP_POP, t1(dt), nop);
        return t1(dt);
    }

    return nop;
}

static FOB_Operand ast_to_fob_operand(FOB_IR *ir, const FOB_AST *ast) {
    if (ast == NULL)
        return nop;

    switch (ast->type) {
        case FOB_AST_CONSTANT:
            if (fob_primitive_is_float(ast->value.constant.data_type.primitive_type))
                return ast_float_to_operand(ast);
            return ast_int_to_operand(ast);
        case FOB_AST_STRING: return ast_string_to_operand(ast);
        case FOB_AST_VARIABLE: return ast_variable_to_operand(ast);
        case FOB_AST_ARITHMETIC: return ast_arithmetic_to_operand(ir, ast);
        case FOB_AST_CONDITION: return ast_condition_to_operand(ir, ast, true);
        case FOB_AST_PARENTHESES_EXPRESSION: return ast_to_fob_operand(ir, ast->value.parentheses_expression.inner_value);
        case FOB_AST_UNARY_NOT: return ast_unary_not_to_operand(ir, ast);
        case FOB_AST_UNARY_NEGATE: return ast_unary_negate_to_operand(ir, ast);
        case FOB_AST_POINTER: return ast_pointer_to_operand(ir, ast);
        case FOB_AST_CALL:
            push_call(ir, ast);
            return fob_create_fob_operand_register(ast->value.call.return_type, FOB_RETURN_REGISTER_1);
        case FOB_AST_DEREFERENCE: return push_dereference(ir, ast);
        case FOB_AST_INDEX: return ast_index_to_operand(ir, ast);
        case FOB_AST_PRE_OPER: return ast_pre_oper_to_operand(ir, ast);
        case FOB_AST_POST_OPER: return ast_post_oper_to_operand(ir, ast, true);
        default: break;
    }

    fob_alert("Uncaught AST type %u (%p)\n", ast->type, (void *)ast);
    return nop;
}

static void push_list(FOB_IR *ir, const FOB_AST_List *list, const bool push_block_ops) {
    if (push_block_ops)
        push_op(ir, FOB_OP_BLOCK_START, nop, nop);

    for (size_t i = 0; i < list->count; i++)
        push_ast(ir, list->nodes[i]);

    if (push_block_ops)
        push_op(ir, FOB_OP_BLOCK_END, nop, nop);
}

static void push_load(FOB_IR *ir, FOB_Operand destination, FOB_Operand source) {
    push_op(ir, fob_operand_is_constant(&source) ? FOB_OP_LOAD_CONST : FOB_OP_LOAD_MEMORY, destination, source);
}

static void push_function(FOB_IR *ir, const FOB_AST *ast) {
    FOB_Operand ident = fob_create_fob_identifier(ast->value.function.name);
    ident.data_type = ast->value.function.data_type;

    push_op(ir, FOB_OP_FUNC_START, nop, ident); 

    size_t int_args = 0;
    size_t float_args = 0;

    for (size_t i = 0; i < ast->value.function.parameters.count; i++) {
        const FOB_AST *param = ast->value.function.parameters.nodes[i];
        FOB_Operand var = fob_create_fob_operand_variable(param->value.parameter.name, param->uid, param->value.parameter.data_type);
        push_op(ir, FOB_OP_NEW_LOCAL_VARIABLE, var, nop);
        push_op(ir, FOB_OP_STORE_MEMORY, var, fob_create_fob_operand_argument(i, int_args, float_args, var.data_type));

        if (fob_data_type_is_float(&var.data_type))
            float_args++;
        else
            int_args++;
    }

    push_list(ir, &ast->value.function.body, false);

    if (ast->value.function.body.count == 0 || ast->value.function.body.nodes[ast->value.function.body.count - 1]->type != FOB_AST_RETURN)
        push_op(ir, FOB_OP_RETURN, nop, nop);

    push_op(ir, FOB_OP_FUNC_END, nop, ident);
}

static void push_return(FOB_IR *ir, const FOB_AST *ast) {
    push_op(ir, FOB_OP_RETURN, fob_create_fob_operand_register(ast->value.return_.return_type, FOB_RETURN_REGISTER_1),
        ast_to_fob_operand(ir, ast->value.return_.value));
}

static void push_declaration(FOB_IR *ir, const FOB_AST *ast) {
    FOB_Operand var = fob_create_fob_operand_variable(ast->value.declaration.name, ast->uid, ast->value.declaration.data_type);
    push_op(ir, FOB_OP_NEW_LOCAL_VARIABLE, var, nop);

    if (ast->value.declaration.value == NULL)
        return;

    FOB_Operand reg = t1(ast->value.declaration.data_type);
    push_load(ir, reg, ast_to_fob_operand(ir, ast->value.declaration.value));
    push_op(ir, FOB_OP_STORE_MEMORY, var, reg);
}

static void push_assignment(FOB_IR *ir, const FOB_AST *ast) {
    const FOB_Data_Type dt = fob_get_ast_data_type(ast->value.assignment.lhs);
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.assignment.rhs));

    if (!ast_is_complex_operand(ast->value.assignment.lhs)) {
        push_op(ir, FOB_OP_STORE_MEMORY, ast_to_fob_operand(ir, ast->value.assignment.lhs), t1(dt));
        return;
    }

    push_op(ir, FOB_OP_PUSH, nop, t1(dt));
    push_load(ir, t1(dt), ast_to_fob_operand(ir, ast->value.assignment.lhs));
    push_op(ir, FOB_OP_POP, t2(dt), nop);
    push_op(ir, FOB_OP_STORE_MEMORY, fob_create_fob_operand_deref(FOB_TEMPORARY_IR_REGISTER_1, dt), t2(dt));
}

static void push_if(FOB_IR *ir, const FOB_AST *ast) {
    const bool has_else = ast->value.if_.else_body.count > 0;
    FOB_Operand else_label;
    FOB_Operand done_label;

    if (has_else)
        else_label = fob_create_fob_operand_label(ir->label_count++);

    done_label = fob_create_fob_operand_label(ir->label_count++);

    push_load(ir, t1(fob_create_data_type(FOB_PRIM_BOOL)),
        ast_to_fob_operand(ir, ast->value.if_.condition));

    push_op(ir, FOB_OP_JMP_FALSE, has_else ? else_label : done_label, t1(fob_create_data_type(FOB_PRIM_BOOL)));
    push_list(ir, &ast->value.if_.body, true);

    if (has_else) {
        push_op(ir, FOB_OP_JMP, done_label, nop);
        push_op(ir, FOB_OP_NEW_LABEL, nop, else_label);
        push_list(ir, &ast->value.if_.else_body, true);
    }

    push_op(ir, FOB_OP_NEW_LABEL, nop, done_label);
}

static void push_inline_assembly(FOB_IR *ir, const FOB_AST *ast) {
    push_op(ir, FOB_OP_INLINE_ASM, nop, fob_create_fob_operand_string(ast->value.inline_assembly.code));
}

static void push_for(FOB_IR *ir, const FOB_AST *ast) {
    push_op(ir, FOB_OP_BLOCK_START, nop, nop);
    push_ast(ir, ast->value.for_.statement);

    FOB_Operand condition_label = fob_create_fob_operand_label(ir->label_count++);
    FOB_Operand repeat_label = fob_create_fob_operand_label(ir->label_count++);
    FOB_Operand body_label = fob_create_fob_operand_label(ir->label_count++);
    FOB_Operand end_label = fob_create_fob_operand_label(ir->label_count++);

    ir->current_break_label_number = end_label.value.label.number;
    ir->current_continue_label_number = repeat_label.value.label.number;

    push_op(ir, FOB_OP_NEW_LABEL, nop, condition_label);
    FOB_Operand condition = ast_condition_to_operand(ir, ast->value.for_.condition, true);
    push_op(ir, FOB_OP_JMP_FALSE, end_label, condition);

    push_op(ir, FOB_OP_NEW_LABEL, nop, body_label);
    push_list(ir, &ast->value.for_.body, true);

    push_op(ir, FOB_OP_NEW_LABEL, nop, repeat_label);
    push_ast(ir, ast->value.for_.repeater);

    push_op(ir, FOB_OP_JMP, condition_label, nop);

    push_op(ir, FOB_OP_NEW_LABEL, nop, end_label);
    push_op(ir, FOB_OP_BLOCK_END, nop, nop);
}

static void push_while(FOB_IR *ir, const FOB_AST *ast) {
    FOB_Operand condition_label = fob_create_fob_operand_label(ir->label_count++);
    FOB_Operand body_label = fob_create_fob_operand_label(ir->label_count++);
    FOB_Operand end_label = fob_create_fob_operand_label(ir->label_count++);

    if (ast->value.while_.do_body_first) {
        push_op(ir, FOB_OP_NEW_LABEL, nop, body_label);
        push_list(ir, &ast->value.while_.body, true);

        push_op(ir, FOB_OP_NEW_LABEL, nop, condition_label);
        FOB_Operand condition = ast_condition_to_operand(ir, ast->value.while_.condition, true);
        push_op(ir, FOB_OP_JMP_TRUE, body_label, condition);
        push_op(ir, FOB_OP_NEW_LABEL, nop, end_label);
        return;
    }


    push_op(ir, FOB_OP_NEW_LABEL, nop, condition_label);
    FOB_Operand condition = ast_condition_to_operand(ir, ast->value.while_.condition, true);
    push_op(ir, FOB_OP_JMP_FALSE, end_label, condition);
    push_op(ir, FOB_OP_NEW_LABEL, nop, body_label);
    push_list(ir, &ast->value.while_.body, true);
    push_op(ir, FOB_OP_JMP, condition_label, nop);
    push_op(ir, FOB_OP_NEW_LABEL, nop, end_label);
}

static void push_ast(FOB_IR *ir, const FOB_AST *ast) {
    switch (ast->type) {
        case FOB_OP_NOP: break;
        case FOB_AST_FUNCTION:
            push_function(ir, ast);
            break;
        case FOB_AST_RETURN:
            push_return(ir, ast);
            break;
        case FOB_AST_DECLARATION:
            push_declaration(ir, ast);
            break;
        case FOB_AST_ASSIGNMENT:
            push_assignment(ir, ast);
            break;
        case FOB_AST_IF:
            push_if(ir, ast);
            break;
        case FOB_AST_INLINE_ASM:
            push_inline_assembly(ir, ast);
            break;
        case FOB_AST_CALL:
            push_call(ir, ast);
            break;
        case FOB_AST_PRE_OPER:
            ast_pre_oper_to_operand(ir, ast);
            break;
        case FOB_AST_POST_OPER:
            ast_post_oper_to_operand(ir, ast, false);
            break;
        case FOB_AST_FOR:
            push_for(ir, ast);
            break;
        case FOB_AST_PARENTHESES_EXPRESSION:
            push_ast(ir, ast->value.parentheses_expression.inner_value);
            break;
        case FOB_AST_BREAK:
            push_op(ir, FOB_OP_JMP, fob_create_fob_operand_label(ir->current_break_label_number), nop);
            break;
        case FOB_AST_CONTINUE:
            push_op(ir, FOB_OP_JMP, fob_create_fob_operand_label(ir->current_continue_label_number), nop);
            break;
        case FOB_AST_WHILE:
            push_while(ir, ast);
            break;
        default:
            fob_alert("Uncaught AST type %u (%p)\n", ast->type, (void *)ast);
            break;
    }
}

FOB_IR fob_ast_root_to_ir(FOB_AST_List *list) {
    FOB_IR ir = (FOB_IR){ .ops = malloc(IR_CAPACITY * sizeof(FOB_Op)), .count = 0, .capacity = IR_CAPACITY, .label_count = 0,
        .current_break_label_number = 0, .current_continue_label_number = 0 };

    push_list(&ir, list, false);
    push_op(&ir, FOB_OP_EOF, nop, nop);
    return ir;
}

void fob_delete_ir(FOB_IR *ir) {
    free(ir->ops);
}
