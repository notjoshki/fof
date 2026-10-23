#ifndef SYMBOL_H
#define SYMBOL_H

#include "scope.h"
#include "fob_data_type.h"
#include "fob_ast.h"
#include <stdio.h>

#define FIND_IN_GLOBAL_SCOPE NULL

typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_VARIABLE
} Symbol_Type;

typedef struct {
    size_t uid;
    Symbol_Type type;
    char *name;
    FOB_Data_Type data_type;
    Scope scope;
    FOB_AST_List *function_parameters; // Only used for SYMBOL_FUNCTIONs.
    bool predefined_only;
} Symbol;

void delete_symbols();
Symbol *create_symbol(size_t uid, Symbol_Type type, char *name, FOB_Data_Type data_type, Scope scope);
Symbol *find_symbol(Symbol_Type type, char *name, const Scope *scope);
Symbol *find_symbol_by_uid(Symbol_Type type, const size_t uid);
Symbol *find_function_symbol_by_function_uid(Symbol_Type type, const size_t uid);

#endif
