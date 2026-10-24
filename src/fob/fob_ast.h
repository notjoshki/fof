// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_AST_H
#define FOB_AST_H

#include "fob_data_type.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define FOB_FUNCTION_EXTERN 0x01

typedef enum {
    FOB_AST_NOP,
    FOB_AST_CONSTANT,
    FOB_AST_FUNCTION,
    FOB_AST_PARAMETER,
    FOB_AST_RETURN,
    FOB_AST_DECLARATION,
    FOB_AST_VARIABLE,
    FOB_AST_ASSIGNMENT,
    FOB_AST_ARITHMETIC,
    FOB_AST_OPERATOR,
    FOB_AST_IF,
    FOB_AST_CONDITION,
    FOB_AST_PARENTHESES_EXPRESSION,
    FOB_AST_UNARY_NOT,
    FOB_AST_UNARY_NEGATE,
    FOB_AST_INLINE_ASM,
    FOB_AST_POINTER,
    FOB_AST_CALL,
    FOB_AST_DEREFERENCE
} FOB_AST_Type;

typedef struct FOB_AST FOB_AST;

typedef struct {
    FOB_AST **nodes;
    size_t count;
    size_t capacity;
} FOB_AST_List;

typedef union {
    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
} FOB_AST_Constant_Value;

typedef struct {
    FOB_Data_Type data_type;
    FOB_AST_Constant_Value value;
} FOB_AST_Constant;

typedef struct {
    char *name;
    FOB_Data_Type data_type;
    FOB_AST_List parameters;
    FOB_AST_List body;
    size_t flags;
} FOB_AST_Function;

typedef struct {
    char *name;
    FOB_Data_Type data_type;
    FOB_AST *default_value;
} FOB_AST_Parameter;

typedef struct {
    FOB_Data_Type return_type;
    FOB_AST *value;
} FOB_AST_Return;

typedef struct {
    char *name;
    FOB_Data_Type data_type;
    FOB_AST *value;
} FOB_AST_Declaration;

typedef struct {
    char *name;
    size_t uid;
    FOB_Data_Type data_type;
} FOB_AST_Variable;

typedef struct {
    FOB_AST *lhs;
    FOB_AST *rhs;
} FOB_AST_Assignment;

typedef struct {
    FOB_AST_List expression;
    FOB_Data_Type result_type;
} FOB_AST_Arithmetic;

typedef struct {
    FOB_AST_List expression;
    FOB_Data_Type comparison_type;
} FOB_AST_Condition;

typedef enum {
    FOB_AST_OPERATOR_AND,
    FOB_AST_OPERATOR_OR,
    FOB_AST_OPERATOR_XOR,
    FOB_AST_OPERATOR_NOT,
    FOB_AST_OPERATOR_ADD,
    FOB_AST_OPERATOR_SUB,
    FOB_AST_OPERATOR_MUL,
    FOB_AST_OPERATOR_DIV,
    FOB_AST_OPERATOR_MOD,
    FOB_AST_OPERATOR_SHL,
    FOB_AST_OPERATOR_SHR,
    FOB_AST_OPERATOR_EQ,
    FOB_AST_OPERATOR_NEQ,
    FOB_AST_OPERATOR_LT,
    FOB_AST_OPERATOR_LTE,
    FOB_AST_OPERATOR_GT,
    FOB_AST_OPERATOR_GTE,
    FOB_AST_OPERATOR_BOOL_AND,
    FOB_AST_OPERATOR_BOOL_OR
} FOB_AST_Operator_Type;

typedef struct {
    FOB_AST_Operator_Type type;
} FOB_AST_Operator;

typedef struct {
    FOB_AST *condition;
    FOB_AST_List body;
    FOB_AST_List else_body;
} FOB_AST_If;

typedef struct {
    FOB_AST *inner_value;
} FOB_AST_Parentheses_Expression;

typedef struct {
    FOB_AST *value;
    bool as_boolean;
} FOB_AST_Unary_Not;

typedef struct {
    FOB_AST *value;
} FOB_AST_Unary_Negate;

typedef struct {
    char *code;
} FOB_AST_Inline_Assembly;

typedef struct {
    FOB_AST *value;
} FOB_AST_Pointer;

typedef struct {
    char *name;
    FOB_AST_List arguments;
    FOB_Data_Type *expected_arguments_data_types;
    FOB_Data_Type return_type;
} FOB_AST_Call;

typedef struct {
    FOB_AST *value;
} FOB_AST_Dereference;

struct FOB_AST {
    size_t uid;
    FOB_AST_Type type;

    union FOB_AST_Value {
        FOB_AST_Constant constant;
        FOB_AST_Function function;
        FOB_AST_Parameter parameter;
        FOB_AST_Return return_;
        FOB_AST_Declaration declaration;
        FOB_AST_Variable variable;
        FOB_AST_Assignment assignment;
        FOB_AST_Arithmetic arithmetic;
        FOB_AST_Operator operator;
        FOB_AST_If if_;
        FOB_AST_Condition condition;
        FOB_AST_Parentheses_Expression parentheses_expression;
        FOB_AST_Unary_Not unary_not;
        FOB_AST_Unary_Negate unary_negate;
        FOB_AST_Inline_Assembly inline_assembly;
        FOB_AST_Pointer pointer;
        FOB_AST_Call call;
        FOB_AST_Dereference dereference;
    } value;
};

FOB_AST *fob_new_ast_nop();
FOB_AST *fob_new_ast_constant(FOB_Data_Type data_type, FOB_AST_Constant_Value value);
FOB_AST *fob_new_ast_function(char *name, FOB_Data_Type data_type, FOB_AST_List parameters, FOB_AST_List body, size_t flags);
FOB_AST *fob_new_ast_parameter(char *name, FOB_Data_Type data_type, FOB_AST *default_value);
FOB_AST *fob_new_ast_return(FOB_Data_Type return_type, FOB_AST *value);
FOB_AST *fob_new_ast_declaration(char *name, FOB_Data_Type data_type, FOB_AST *value);
FOB_AST *fob_new_ast_variable(char *name, size_t uid, FOB_Data_Type data_type);
FOB_AST *fob_new_ast_assignment(FOB_AST *lhs, FOB_AST *rhs);
FOB_AST *fob_new_ast_arithmetic(FOB_AST_List expression, FOB_Data_Type result_type);
FOB_AST *fob_new_ast_operator(FOB_AST_Operator_Type type);
FOB_AST *fob_new_ast_condition(FOB_AST_List expression, FOB_Data_Type comparison_type);
void fob_push_condition_part(FOB_AST_List *expression, FOB_AST *lhs, FOB_AST_Operator_Type operator, FOB_AST *rhs);
void fob_push_single_valued_condition(FOB_AST_List *expression, FOB_AST *value, const bool checking_if_true);
FOB_AST *fob_new_ast_if(FOB_AST *condition, FOB_AST_List body, FOB_AST_List else_body);
FOB_AST *fob_new_ast_parentheses_expression(FOB_AST *inner_value);
FOB_AST *fob_new_ast_unary_not(FOB_AST *value, const bool as_boolean);
FOB_AST *fob_new_ast_unary_negate(FOB_AST *value);
FOB_AST *fob_new_ast_inline_assembly(char *code);
FOB_AST *fob_new_ast_pointer(FOB_AST *value);
FOB_AST *fob_new_ast_call(char *name, FOB_AST_List arguments, FOB_Data_Type *expected_arguments_data_types, FOB_Data_Type return_type);
FOB_AST *fob_new_ast_dereference(FOB_AST *value);

void fob_delete_ast(FOB_AST *ast);
char *fob_ast_type_to_string(const FOB_AST_Type type);

FOB_AST_List fob_create_ast_list();
void fob_delete_ast_list(FOB_AST_List *list, const bool delete_nodes);
void fob_push_ast_node(FOB_AST_List *list, FOB_AST *ast);

FOB_Data_Type fob_get_ast_data_type(const FOB_AST *ast);

#endif
