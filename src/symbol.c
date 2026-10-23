#include "symbol.h"
#include "scope.h"
#include "fob_data_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define SYMBOL_CAPACITY 8

static Symbol *symbols = NULL;
static size_t symbol_count = 0;
static size_t symbol_capacity = SYMBOL_CAPACITY;

void delete_symbols() {
    if (symbols != NULL) {
        for (size_t i = 0; i < symbol_count; i++) {
            Symbol *symbol = &symbols[i];

            if (!symbol->predefined_only)
                continue;

            assert(symbol->type == SYMBOL_FUNCTION);
            fob_delete_ast_list(symbol->function_parameters, true);
            free(symbol->function_parameters);
        }

        free(symbols);
        symbols = NULL;
    }

    symbol_count = 0;
    symbol_capacity = SYMBOL_CAPACITY;
}

Symbol *create_symbol(size_t uid, Symbol_Type type, char *name, FOB_Data_Type data_type, Scope scope) {
    if (symbols == NULL)
        symbols = malloc(SYMBOL_CAPACITY * sizeof(Symbol));

    if (symbol_count + 1 > symbol_capacity) {
        symbol_capacity *= 2;
        symbols = realloc(symbols, symbol_capacity * sizeof(Symbol));
    }

    symbols[symbol_count] = (Symbol){ .uid = uid, .type = type, .name = name, .data_type = data_type, .scope = scope, .predefined_only = false };
    return &symbols[symbol_count++];
}

static inline bool is_in_scope(const Scope *symbol_scope, const Scope *scope) {
    return ((symbol_scope->function_uid == scope->function_uid) &&
        (symbol_scope->scope_uid <= scope->scope_uid && symbol_scope->indent_level <= scope->indent_level)) || 
        scope->function_uid == 0 || symbol_scope->function_uid == 0;
}

Symbol *find_symbol(Symbol_Type type, char *name, const Scope *scope) {
    if (symbols == NULL || symbol_count == 0)
        return NULL;

    for (size_t i = 0; i < symbol_count; i++) {
        Symbol *symbol = &symbols[i];

        if (symbol->type == type && strcmp(symbols[i].name, name) == 0 &&
                (scope == FIND_IN_GLOBAL_SCOPE || is_in_scope(&symbol->scope, scope)))
            return symbol;
    }

    return NULL;
}

Symbol *find_symbol_by_uid(Symbol_Type type, const size_t uid) {
    if (symbols == NULL || symbol_count == 0)
        return NULL;

    for (size_t i = 0; i < symbol_count; i++) {
        Symbol *symbol = &symbols[i];

        if (symbol->type == type && symbol->uid == uid)
            return symbol;
    }

    return NULL;
}

Symbol *find_function_symbol_by_function_uid(Symbol_Type type, const size_t uid) {
    if (symbols == NULL || symbol_count == 0)
        return NULL;

    for (size_t i = 0; i < symbol_count; i++) {
        Symbol *symbol = &symbols[i];

        if (symbol->type == type && symbol->scope.function_uid == uid)
            return symbol;
    }

    return NULL;
}
