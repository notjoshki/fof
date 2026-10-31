// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#include "fob_ast.h"
#include "fob_data_type.h"
#include "fob_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define FOB_AST_LIST_CAPACITY 4

static size_t uids = 0;

static FOB_AST *new_ast() {
    FOB_AST *ast = malloc(sizeof(FOB_AST));
    ast->uid = ++uids;
    return ast;
}

FOB_AST *fob_new_ast_nop() {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_NOP;
    return ast;
}

FOB_AST *fob_new_ast_constant(FOB_Data_Type data_type, FOB_AST_Constant_Value value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_CONSTANT;
    ast->value.constant.data_type = data_type;
    ast->value.constant.value = value;
    return ast;
}

FOB_AST *fob_new_ast_string(char *text) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_STRING;
    ast->value.string.text = text;
    return ast;
}

FOB_AST *fob_new_ast_function(char *name, FOB_Data_Type data_type, FOB_AST_List parameters, FOB_AST_List body, size_t flags) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_FUNCTION;
    ast->value.function.name = name;
    ast->value.function.data_type = data_type;
    ast->value.function.parameters = parameters;
    ast->value.function.body = body;
    ast->value.function.flags = flags;
    return ast;
}

FOB_AST *fob_new_ast_parameter(char *name, FOB_Data_Type data_type, FOB_AST *default_value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_PARAMETER;
    ast->value.parameter.name = name;
    ast->value.parameter.data_type = data_type;
    ast->value.parameter.default_value = default_value;
    return ast;
}

FOB_AST *fob_new_ast_return(FOB_Data_Type return_type, FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_RETURN;
    ast->value.return_.return_type = return_type;
    ast->value.return_.value = value;
    return ast;
}

FOB_AST *fob_new_ast_declaration(char *name, FOB_Data_Type data_type, FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_DECLARATION;
    ast->value.declaration.name = name;
    ast->value.declaration.data_type = data_type;
    ast->value.declaration.value = value;
    return ast;
}

FOB_AST *fob_new_ast_variable(char *name, size_t uid, FOB_Data_Type data_type) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_VARIABLE;
    ast->value.variable.name = name;
    ast->value.variable.uid = uid;
    ast->value.variable.data_type = data_type;
    return ast;
}

FOB_AST *fob_new_ast_assignment(FOB_AST *lhs, FOB_AST *rhs) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_ASSIGNMENT;
    ast->value.assignment.lhs = lhs;
    ast->value.assignment.rhs = rhs;
    return ast;
}

FOB_AST *fob_new_ast_arithmetic(FOB_AST_List expression, FOB_Data_Type result_type) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_ARITHMETIC;
    ast->value.arithmetic.expression = expression;
    ast->value.arithmetic.result_type = result_type;
    return ast;
}

FOB_AST *fob_new_ast_operator(FOB_AST_Operator_Type type) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_OPERATOR;
    ast->value.operator.type = type;
    return ast;
}

FOB_AST *fob_new_ast_condition(FOB_AST_List expression, FOB_Data_Type comparison_type) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_CONDITION;
    ast->value.condition.expression = expression;
    ast->value.condition.comparison_type = comparison_type;
    return ast;
}

void fob_push_condition_part(FOB_AST_List *expression, FOB_AST *lhs, FOB_AST_Operator_Type operator, FOB_AST *rhs) {
    fob_push_ast_node(expression, lhs);
    fob_push_ast_node(expression, fob_new_ast_operator(operator));
    fob_push_ast_node(expression, rhs);
}

void fob_push_single_valued_condition(FOB_AST_List *expression, FOB_AST *value, const bool checking_if_true) {
    fob_push_ast_node(expression, value);
    fob_push_ast_node(expression, fob_new_ast_operator(checking_if_true ? FOB_AST_OPERATOR_NEQ : FOB_AST_OPERATOR_EQ));
    fob_push_ast_node(expression, fob_new_ast_constant(fob_create_data_type(FOB_PRIM_I32), (FOB_AST_Constant_Value){ .i32 = 0 }));
}

FOB_AST *fob_new_ast_if(FOB_AST *condition, FOB_AST_List body, FOB_AST_List else_body) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_IF;
    ast->value.if_.condition = condition;
    ast->value.if_.body = body;
    ast->value.if_.else_body = else_body;
    return ast;
}

FOB_AST *fob_new_ast_parentheses_expression(FOB_AST *inner_value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_PARENTHESES_EXPRESSION;
    ast->value.parentheses_expression.inner_value = inner_value;
    return ast;
}

FOB_AST *fob_new_ast_unary_not(FOB_AST *value, const bool as_boolean) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_UNARY_NOT;
    ast->value.unary_not.value = value;
    ast->value.unary_not.as_boolean = as_boolean;
    return ast;
}

FOB_AST *fob_new_ast_unary_negate(FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_UNARY_NEGATE;
    ast->value.unary_negate.value = value;
    return ast;
}

FOB_AST *fob_new_ast_inline_assembly(char *code) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_INLINE_ASM;
    ast->value.inline_assembly.code = code;
    return ast;
}

FOB_AST *fob_new_ast_pointer(FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_POINTER;
    ast->value.pointer.value = value;
    return ast;
}

FOB_AST *fob_new_ast_call(char *name, FOB_AST_List arguments, FOB_Data_Type *expected_arguments_data_types, FOB_Data_Type return_type) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_CALL;
    ast->value.call.name = name;
    ast->value.call.arguments = arguments;
    ast->value.call.expected_arguments_data_types = expected_arguments_data_types;
    ast->value.call.return_type = return_type;
    return ast;
}

FOB_AST *fob_new_ast_dereference(FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_DEREFERENCE;
    ast->value.dereference.value = value;
    return ast;
}

FOB_AST *fob_new_ast_index(FOB_AST *pointer, FOB_AST *offset) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_INDEX;
    ast->value.index.pointer = pointer;
    ast->value.index.offset = offset;
    return ast;
}

FOB_AST *fob_new_ast_pre_operation(FOB_AST_Operator_Type type, FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_PRE_OPER;
    ast->value.pre_operation.type = type;
    ast->value.pre_operation.value = value;
    return ast;
}

FOB_AST *fob_new_ast_post_operation(FOB_AST_Operator_Type type, FOB_AST *value) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_POST_OPER;
    ast->value.post_operation.type = type;
    ast->value.post_operation.value = value;
    return ast;
}

FOB_AST *fob_new_ast_for(FOB_AST *statement, FOB_AST *condition, FOB_AST *repeater, FOB_AST_List body) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_FOR;
    ast->value.for_.statement = statement;
    ast->value.for_.condition = condition;
    ast->value.for_.repeater = repeater;
    ast->value.for_.body = body;
    return ast;
}

FOB_AST *fob_new_ast_break() {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_BREAK;
    return ast;
}

FOB_AST *fob_new_ast_continue() {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_CONTINUE;
    return ast;
}

FOB_AST *fob_new_ast_while(FOB_AST *condition, FOB_AST_List body, bool do_body_first) {
    FOB_AST *ast = new_ast();
    ast->type = FOB_AST_WHILE;
    ast->value.while_.condition = condition;
    ast->value.while_.body = body;
    ast->value.while_.do_body_first = do_body_first;
    return ast;
}

void fob_delete_ast(FOB_AST *ast) {
    switch (ast->type) {
        case FOB_AST_OPERATOR:
        case FOB_AST_CONSTANT:
        case FOB_AST_BREAK:
        case FOB_AST_CONTINUE:
        case FOB_AST_NOP: break;
        case FOB_AST_STRING:
            free(ast->value.string.text);
            break;
        case FOB_AST_FUNCTION:
            free(ast->value.function.name);
            fob_delete_ast_list(&ast->value.function.parameters, true);
            fob_delete_ast_list(&ast->value.function.body, true);
            break;
        case FOB_AST_PARAMETER:
            free(ast->value.parameter.name);

            if (ast->value.parameter.default_value != NULL)
                fob_delete_ast(ast->value.parameter.default_value);
            break;
        case FOB_AST_RETURN:
            if (ast->value.return_.value != NULL)
                fob_delete_ast(ast->value.return_.value);
            break;
        case FOB_AST_DECLARATION:
            free(ast->value.declaration.name);

            if (ast->value.declaration.value != NULL)
                fob_delete_ast(ast->value.declaration.value);
            break;
        case FOB_AST_VARIABLE:
            free(ast->value.variable.name);
            break;
        case FOB_AST_ASSIGNMENT:
            fob_delete_ast(ast->value.assignment.lhs);
            fob_delete_ast(ast->value.assignment.rhs);
            break;
        case FOB_AST_ARITHMETIC:
            fob_delete_ast_list(&ast->value.arithmetic.expression, true);
            break;
        case FOB_AST_IF:
            fob_delete_ast(ast->value.if_.condition);
            fob_delete_ast_list(&ast->value.if_.body, true);
            fob_delete_ast_list(&ast->value.if_.else_body, true);
            break;
        case FOB_AST_CONDITION:
            fob_delete_ast_list(&ast->value.condition.expression, true);
            break;
        case FOB_AST_PARENTHESES_EXPRESSION:
            fob_delete_ast(ast->value.parentheses_expression.inner_value);
            break;
        case FOB_AST_UNARY_NOT:
            fob_delete_ast(ast->value.unary_not.value);
            break;
        case FOB_AST_UNARY_NEGATE:
            fob_delete_ast(ast->value.unary_negate.value);
            break;
        case FOB_AST_INLINE_ASM:
            free(ast->value.inline_assembly.code);
            break;
        case FOB_AST_POINTER:
            fob_delete_ast(ast->value.pointer.value);
            break;
        case FOB_AST_CALL:
            free(ast->value.call.name);
            free(ast->value.call.expected_arguments_data_types);
            fob_delete_ast_list(&ast->value.call.arguments, true);
            break;
        case FOB_AST_DEREFERENCE:
            fob_delete_ast(ast->value.dereference.value);
            break;
        case FOB_AST_INDEX:
            fob_delete_ast(ast->value.index.pointer);
            fob_delete_ast(ast->value.index.offset);
            break;
        case FOB_AST_PRE_OPER:
            fob_delete_ast(ast->value.pre_operation.value);
            break;
        case FOB_AST_POST_OPER:
            fob_delete_ast(ast->value.post_operation.value);
            break;
        case FOB_AST_FOR:
            if (ast->value.for_.statement != NULL)
                fob_delete_ast(ast->value.for_.statement);

            if (ast->value.for_.condition != NULL)
                fob_delete_ast(ast->value.for_.condition);

            if (ast->value.for_.repeater != NULL)
                fob_delete_ast(ast->value.for_.repeater);

            fob_delete_ast_list(&ast->value.for_.body, true);
            break;
        case FOB_AST_WHILE:
            fob_delete_ast(ast->value.while_.condition);
            fob_delete_ast_list(&ast->value.while_.body, true);
            break;
        default:
            fob_alert("Uncaught AST type %u (%p)\n", ast->type, (void *)ast);
            break;
    }

    free(ast);
}

char *fob_ast_type_to_string(const FOB_AST_Type type) {
    switch (type) {
        case FOB_AST_NOP: return "nop";
        case FOB_AST_CONSTANT: return "constant";
        case FOB_AST_STRING: return "string";
        case FOB_AST_FUNCTION: return "function";
        case FOB_AST_PARAMETER: return "parameter";
        case FOB_AST_RETURN: return "return";
        case FOB_AST_DECLARATION: return "declaration";
        case FOB_AST_VARIABLE: return "variable";
        case FOB_AST_ASSIGNMENT: return "assignment";
        case FOB_AST_ARITHMETIC: return "arithmetic";
        case FOB_AST_OPERATOR: return "operator";
        case FOB_AST_IF: return "if";
        case FOB_AST_CONDITION: return "condition";
        case FOB_AST_PARENTHESES_EXPRESSION: return "parentheses expression";
        case FOB_AST_UNARY_NOT: return "unary not";
        case FOB_AST_UNARY_NEGATE: return "unary negate";
        case FOB_AST_INLINE_ASM: return "inline assembly";
        case FOB_AST_POINTER: return "pointer";
        case FOB_AST_CALL: return "call";
        case FOB_AST_DEREFERENCE: return "dereference";
        case FOB_AST_INDEX: return "index";
        case FOB_AST_PRE_OPER: return "pre-operation";
        case FOB_AST_POST_OPER: return "post-operation";
        case FOB_AST_FOR: return "for";
        case FOB_AST_BREAK: return "break";
        case FOB_AST_CONTINUE: return "continue";
        case FOB_AST_WHILE: return "while";
        default: break;
    }

    fob_alert("Uncaught AST type %u\n", type);
    return "\0";
}

FOB_AST_List fob_create_ast_list() {
    return (FOB_AST_List){ .nodes = malloc(FOB_AST_LIST_CAPACITY * sizeof(FOB_AST *)), 
        .count = 0, .capacity = FOB_AST_LIST_CAPACITY };
}

void fob_delete_ast_list(FOB_AST_List *list, const bool delete_nodes) {
    if (delete_nodes) {
        for (size_t i = 0; i < list->count; i++)
            fob_delete_ast(list->nodes[i]);
    }

    free(list->nodes);
}

void fob_push_ast_node(FOB_AST_List *list, FOB_AST *ast) {
    if (list->count + 1 > list->capacity) {
        list->capacity *= 2;
        list->nodes = realloc(list->nodes, list->capacity * sizeof(FOB_AST *));
    }

    list->nodes[list->count++] = ast;
}

FOB_Data_Type fob_get_ast_data_type(const FOB_AST *ast) {
    switch (ast->type) {
        case FOB_AST_INLINE_ASM:
        case FOB_AST_NOP: return fob_create_data_type(FOB_PRIM_VOID);
        case FOB_AST_CONSTANT: return ast->value.constant.data_type;
        case FOB_AST_STRING: {
            FOB_Data_Type dt = fob_create_data_type(FOB_PRIM_CHAR);
            dt.pointers++;
            return dt;
        }
        case FOB_AST_FUNCTION: return ast->value.function.data_type;
        case FOB_AST_PARAMETER: return ast->value.parameter.data_type;
        case FOB_AST_RETURN: return ast->value.return_.return_type;
        case FOB_AST_DECLARATION: return ast->value.declaration.data_type;
        case FOB_AST_VARIABLE: return ast->value.variable.data_type;
        case FOB_AST_ASSIGNMENT: return fob_get_ast_data_type(ast->value.assignment.lhs);
        case FOB_AST_ARITHMETIC: return ast->value.arithmetic.result_type;
        case FOB_AST_IF:
        case FOB_AST_CONDITION: return fob_create_data_type(FOB_PRIM_BOOL);
        case FOB_AST_PARENTHESES_EXPRESSION: return fob_get_ast_data_type(ast->value.parentheses_expression.inner_value);
        case FOB_AST_UNARY_NOT: return fob_get_ast_data_type(ast->value.unary_not.value);
        case FOB_AST_UNARY_NEGATE: return fob_get_ast_data_type(ast->value.unary_negate.value);
        case FOB_AST_POINTER: {
            FOB_Data_Type dt = fob_get_ast_data_type(ast->value.pointer.value);
            dt.pointers++;
            return dt;
        }
        case FOB_AST_CALL: return ast->value.call.return_type;
        case FOB_AST_DEREFERENCE: {
            FOB_Data_Type dt = fob_get_ast_data_type(ast->value.dereference.value);
            assert(dt.pointers > 0);

            if (dt.pointers > 0)
                dt.pointers--;

            return dt;
        }
        case FOB_AST_INDEX: {
            FOB_Data_Type dt = fob_get_ast_data_type(ast->value.index.pointer);
            assert(dt.pointers > 0);

            if (dt.pointers > 0)
                dt.pointers--;

            return dt;
        }
        case FOB_AST_PRE_OPER: return fob_get_ast_data_type(ast->value.pre_operation.value);
        case FOB_AST_POST_OPER: return fob_get_ast_data_type(ast->value.post_operation.value);
        default: break;
    }

    fob_alert("Uncaught AST type %u (%p)\n", ast->type, (void *)ast);
    return fob_create_data_type(FOB_PRIM_VOID);
}
