#include "parser.h"
#include "token.h"
#include "lexer.h"
#include "error.h"
#include "symbol.h"
#include "scope.h"
#include "fob_ast.h"
#include "fob_data_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#define TOKEN_CAPACITY 8

#define MOD_NO_MODS 0
#define MOD_SIGNED 0x01
#define MOD_UNSIGNED 0x02

#define PARSING_CONDITION 0x01
#define PARSING_ARITHMETIC 0x02
#define PARSING_SINGLE_VALUE 0x04
#define PARSING_VALUE 0x08
#define PARSING_LOOP 0x10
#define PARSING_FOR 0x20

typedef struct {
    char *filename;
    Token *tokens;
    size_t token_count;
    size_t token_index;
    Token *token;
    Scope scope;
    size_t function_uids;
    size_t scope_uids;
    size_t flags;
} Parser;

static Parser create_parser(char *filename) {
    Parser prs = (Parser){ .filename = filename, .tokens = malloc(TOKEN_CAPACITY * sizeof(Token)), .token_index = 0,
        .scope = { .function_uid = 0, .scope_uid = 0, .indent_level = 0 }, .function_uids = 0, .scope_uids = 0, .flags = 0 };

    Lexer lex = create_lexer(filename);
    Token tok;
    size_t capacity = TOKEN_CAPACITY;

    while ((tok = get_next_token(&lex)).type != TOKEN_EOF) {
        if (prs.token_count + 2 > capacity) { // +2 for current and EOF token.
            capacity *= 2;
            prs.tokens = realloc(prs.tokens, capacity * sizeof(Token));
        }

        prs.tokens[prs.token_count++] = tok;
    }
    
    delete_lexer(&lex);
    prs.tokens[prs.token_count++] = tok;
    prs.token = &prs.tokens[0];
    return prs;
}

static void delete_parser(Parser *parser) {
    for (size_t i = 0; i < parser->token_count; i++)
        delete_token(&parser->tokens[i]);

    free(parser->tokens);
}

static void step(Parser *parser) {
    if (parser->token_index + 1 < parser->token_count)
        parser->token = &parser->tokens[++parser->token_index];
}

static Token *peek(Parser *parser, const int offset) {
    if (parser->token_index + offset >= parser->token_count)
        return &parser->tokens[parser->token_count - 1];
    
    if ((int)parser->token_index + offset <= 0)
        return &parser->tokens[0];

    return &parser->tokens[parser->token_index + offset];
}

static void eat(Parser *parser, const Token_Type type) {
    if (parser->token->type != type)
        log_error(ERROR_CRITICAL, "Expected '%s' but found '%s'\n", token_type_to_string(type), token_type_to_string(parser->token->type));

    if (parser->token->type != TOKEN_EOF)
        step(parser);
}

static FOB_AST *parse(Parser *parser);
static FOB_AST *parse_value(Parser *parser);

static FOB_AST *parse_value(Parser *parser) {
    const size_t flags = parser->flags;
    parser->flags |= PARSING_VALUE;
    FOB_AST *value = parse(parser);
    // TODO: ...
    parser->flags = flags;
    return value;
}

static FOB_Primitive_Type vague_data_type_to_primitive_type(const char *identifier, const size_t typename_len, const size_t modifiers) {
    const bool is_unsigned = modifiers & MOD_UNSIGNED;

    if (typename_len == 4 && strncmp(identifier, "char", typename_len) == 0)  return is_unsigned ? FOB_PRIM_U8 : FOB_PRIM_I8;
    if (typename_len == 5 && strncmp(identifier, "short", typename_len) == 0)  return is_unsigned ? FOB_PRIM_U16 : FOB_PRIM_I16;
    if (typename_len == 3 && strncmp(identifier, "int", typename_len) == 0)  return is_unsigned ? FOB_PRIM_U32 : FOB_PRIM_I32;
    if (typename_len == 4 && strncmp(identifier, "long", typename_len) == 0)  return is_unsigned ? FOB_PRIM_U64 : FOB_PRIM_I64;

    return FOB_PRIM_UNKNOWN;
}

static bool id_is_data_type(const char *identifier) {
    size_t pointers = 0;
    const size_t len = strlen(identifier);

    for (int i = (int)len - 1; i >= 0; i--) {
        if (identifier[i] != '*')
            break;

        pointers++;
    }

    const size_t typename_len = len - pointers;

    if (typename_len == 4 && strncmp(identifier, "bool", typename_len) == 0)  return true;
    if (typename_len == 6 && strncmp(identifier, "int8_t", typename_len) == 0)  return true;
    if (typename_len == 7 && strncmp(identifier, "int16_t", typename_len) == 0)  return true;
    if (typename_len == 7 && strncmp(identifier, "int32_t", typename_len) == 0)  return true;
    if (typename_len == 7 && strncmp(identifier, "int64_t", typename_len) == 0)  return true;
    if (typename_len == 7 && strncmp(identifier, "ssize_t", typename_len) == 0)  return true;
    if (typename_len == 7 && strncmp(identifier, "uint8_t", typename_len) == 0)  return true;
    if (typename_len == 8 && strncmp(identifier, "uint16_t", typename_len) == 0)  return true;
    if (typename_len == 8 && strncmp(identifier, "uint32_t", typename_len) == 0)  return true;
    if (typename_len == 8 && strncmp(identifier, "uint64_t", typename_len) == 0)  return true;
    if (typename_len == 6 && strncmp(identifier, "size_t", typename_len) == 0)  return true;
    if (typename_len == 5 && strncmp(identifier, "float", typename_len) == 0)  return true;
    if (typename_len == 6 && strncmp(identifier, "double", typename_len) == 0)  return true;
    if (typename_len == 4 && strncmp(identifier, "void", typename_len) == 0)  return true;

    return vague_data_type_to_primitive_type(identifier, typename_len, 0) != FOB_PRIM_UNKNOWN;
}

static FOB_Data_Type identifier_to_data_type(const char *identifier, const bool show_error, const size_t modifiers) {
    FOB_Data_Type type = fob_create_data_type(FOB_PRIM_UNKNOWN);
    const size_t len = strlen(identifier);

    for (int i = (int)len - 1; i >= 0; i--) {
        if (identifier[i] != '*')
            break;

        type.pointers++;
    }

    const size_t typename_len = len - type.pointers;
    bool was_vague = false;

    if (typename_len == 4 && strncmp(identifier, "bool", typename_len) == 0)
        type.primitive_type = FOB_PRIM_BOOL;
    else if (typename_len == 6 && strncmp(identifier, "int8_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_I8;
    else if (typename_len == 7 && strncmp(identifier, "int16_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_I16;
    else if ((typename_len == 7 && strncmp(identifier, "int32_t", typename_len) == 0) ||
            (typename_len == 6 && strncmp(identifier, "signed", typename_len) == 0))
        type.primitive_type = FOB_PRIM_I32;
    else if (typename_len == 7 && strncmp(identifier, "int64_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_I64;
    else if (typename_len == 7 && strncmp(identifier, "ssize_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_ISIZE;
    else if (typename_len == 7 && strncmp(identifier, "uint8_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_U8;
    else if (typename_len == 8 && strncmp(identifier, "uint16_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_U16;
    else if ((typename_len == 8 && strncmp(identifier, "uint32_t", typename_len) == 0) ||
            (typename_len == 8 && strncmp(identifier, "unsigned", typename_len) == 0))
        type.primitive_type = FOB_PRIM_U32;
    else if (typename_len == 8 && strncmp(identifier, "uint64_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_U64;
    else if (typename_len == 6 && strncmp(identifier, "size_t", typename_len) == 0)
        type.primitive_type = FOB_PRIM_USIZE;
    else if (typename_len == 5 && strncmp(identifier, "float", typename_len) == 0)
        type.primitive_type = FOB_PRIM_F32;
    else if (typename_len == 6 && strncmp(identifier, "double", typename_len) == 0)
        type.primitive_type = FOB_PRIM_F64;
    else if (typename_len == 4 && strncmp(identifier, "void", typename_len) == 0) {}
    else {
        type.primitive_type = vague_data_type_to_primitive_type(identifier, typename_len, modifiers);

        if (type.primitive_type == FOB_PRIM_UNKNOWN) {
            if (show_error)
                log_error(ERROR_CRITICAL, "Undefined data type '%s'\n", identifier);
        } else
            was_vague = true;
    }

    if ((modifiers & MOD_SIGNED || modifiers & MOD_UNSIGNED) && !was_vague)
        log_error(ERROR_CRITICAL, "Invalid combination of signedness modifier with fixed width type '%.*s'\n", (int)typename_len, identifier);

    return type;
}

static FOB_Data_Type parse_data_type(Parser *parser, const size_t modifiers) {
    FOB_Data_Type dt = identifier_to_data_type(parser->token->value, true, modifiers);
    eat(parser, TOKEN_ID);

    while (parser->token->type == TOKEN_STAR) {
        dt.pointers++;
        eat(parser, TOKEN_STAR);
    }

    return dt;
}

static FOB_AST_List parse_body(Parser *parser, bool require_braces) {
    FOB_AST_List body = fob_create_ast_list();

    if (!require_braces && parser->token->type == TOKEN_LBRACE)
        require_braces = true;

    if (require_braces) {
        eat(parser, TOKEN_LBRACE);

        while (parser->token->type != TOKEN_EOF && parser->token->type != TOKEN_RBRACE)
            fob_push_ast_node(&body, parse(parser));

        eat(parser, TOKEN_RBRACE);
    } else
        fob_push_ast_node(&body, parse(parser));

    return body;
}

static FOB_AST *parse_parameter(Parser *parser) {
    FOB_Data_Type dt = parse_data_type(parser, MOD_NO_MODS);
    char *name = malloc(strlen(parser->token->value) + 1);
    strcpy(name, parser->token->value);
    eat(parser, TOKEN_ID);

    FOB_AST *ast = fob_new_ast_parameter(name, dt, NULL);
    create_symbol(ast->uid, SYMBOL_VARIABLE, name, dt, parser->scope);
    return ast;
}

static FOB_AST *parse_function(Parser *parser, char *type, char *name, const size_t modifiers) {
    Symbol *symbol = find_symbol(SYMBOL_FUNCTION, name, &parser->scope);
    bool redefined = false;

    if (symbol != NULL && !symbol->predefined_only) {
        log_error(ERROR_CRITICAL, "Redefinition of function '%s'\n", name); 
        redefined = true;
    }

    FOB_Data_Type dt = identifier_to_data_type(type, true, modifiers);
    free(type);

    parser->scope.function_uid = ++parser->function_uids;

    if (symbol != NULL && symbol->predefined_only) {
        free(symbol->name);
        symbol->name = name;
    }

    if (symbol == NULL || !symbol->predefined_only)
        symbol = create_symbol(parser->scope.function_uid, SYMBOL_FUNCTION, name, dt, parser->scope);

    FOB_AST_List params = fob_create_ast_list();
    eat(parser, TOKEN_LPAREN);

    while (parser->token->type != TOKEN_EOF && parser->token->type != TOKEN_RPAREN) {
        if (params.count == 0 && strcmp(parser->token->value, "void") == 0 && peek(parser, 1)->type == TOKEN_RPAREN) {
            eat(parser, TOKEN_ID);
            break;
        }

        if (params.count > 0)
            eat(parser, TOKEN_COMMA);

        fob_push_ast_node(&params, parse_parameter(parser));
    }

    eat(parser, TOKEN_RPAREN);

    if (parser->token->type == TOKEN_SEMICOLON) {
        if (symbol->predefined_only)
            log_error(ERROR_CRITICAL, "Predefining function '%s' again\n", name);
        else {
            symbol->function_parameters = malloc(sizeof(FOB_AST_List));
            *symbol->function_parameters = params;
            symbol->predefined_only = true;
        }

        parser->scope.function_uid = 0;
        return fob_new_ast_nop();
    }

    FOB_AST *ast = fob_new_ast_function(name, dt, params, parse_body(parser, true), 0);

    if (!redefined) {
        if (symbol->predefined_only) {
            fob_delete_ast_list(symbol->function_parameters, true);
            free(symbol->function_parameters);
        }

        symbol->function_parameters = &ast->value.function.parameters;
    }

    parser->scope.function_uid = 0;
    symbol->predefined_only = false;
    return ast;
}

static FOB_AST *parse_declaration(Parser *parser, char *type, char *name, const size_t modifiers) {
    FOB_Data_Type dt = identifier_to_data_type(type, true, modifiers);
    free(type);

    FOB_AST *value = NULL;

    if (parser->token->type == TOKEN_EQUAL) {
        eat(parser, TOKEN_EQUAL);
        value = parse_value(parser);
    }

    FOB_AST *ast = fob_new_ast_declaration(name, dt, value);
    create_symbol(ast->uid, SYMBOL_VARIABLE, name, dt, parser->scope);
    return ast;
}

static FOB_AST *parse_variable(char *name, const Symbol *symbol) {
    return fob_new_ast_variable(name, symbol->uid, symbol->data_type);
}

static char *parse_initial_identifier(Parser *parser, size_t *out_modifiers) {
    char *id = malloc(strlen(parser->token->value) + 1);
    strcpy(id, parser->token->value);

    if (parser->flags & PARSING_VALUE) {
        eat(parser, TOKEN_ID);
        return id;
    }

    bool stepped = false;

    if (peek(parser, 1)->type == TOKEN_STAR && peek(parser, 2)->type != TOKEN_EQUAL) {
    eat(parser, TOKEN_ID);

        while (parser->token->type == TOKEN_STAR) {
            id = realloc(id, strlen(id) + 2);
            strcat(id, "*");
            eat(parser, TOKEN_STAR);
        }

        stepped = true;
    } else if (strcmp(id, "signed") == 0)
        *out_modifiers |= MOD_SIGNED;
    else if (strcmp(id, "unsigned") == 0)
        *out_modifiers |= MOD_UNSIGNED;

    if (!stepped)
        eat(parser, TOKEN_ID);

    return id;
}

static FOB_AST *parse_call(Parser *parser) {
    char *name = malloc(strlen(parser->token->value) + 1);
    strcpy(name, parser->token->value);
    eat(parser, TOKEN_ID);

    FOB_Data_Type type;
    Symbol *symbol = find_symbol(SYMBOL_FUNCTION, name, FIND_IN_GLOBAL_SCOPE);

    if (symbol == NULL) {
        log_error(ERROR_CRITICAL, "Call to undefined function '%s'\n", name);
        type = fob_create_data_type(FOB_PRIM_VOID);
    } else
        type = symbol->data_type;

    FOB_AST_List args = fob_create_ast_list();
    eat(parser, TOKEN_LPAREN);

    while (parser->token->type != TOKEN_EOF && parser->token->type != TOKEN_RPAREN) {
        if (args.count > 0)
            eat(parser, TOKEN_COMMA);

        fob_push_ast_node(&args, parse_value(parser));
    }

    eat(parser, TOKEN_RPAREN);

    FOB_Data_Type *expected_types;

    if (symbol == NULL)
        expected_types = calloc(args.count, sizeof(FOB_Data_Type)); // Just so we don't get an invalid free.
    else {
        expected_types = malloc(symbol->function_parameters->count * sizeof(FOB_Data_Type));

        for (size_t i = 0; i < symbol->function_parameters->count; i++)
            expected_types[i] = symbol->function_parameters->nodes[i]->value.parameter.data_type;
    }

    return fob_new_ast_call(name, args, expected_types, type);
}

static FOB_AST *parse_identifier(Parser *parser) {
    if (peek(parser, 1)->type == TOKEN_LPAREN)
        return parse_call(parser);

    size_t modifiers = MOD_NO_MODS;
    char *id = parse_initial_identifier(parser, &modifiers);

    if (!(parser->flags & PARSING_VALUE) && modifiers != MOD_NO_MODS) { // ID was a type modifier, re-parse.
        size_t modifiers_before;

        do {
            modifiers_before = modifiers;
            free(id);
            id = parse_initial_identifier(parser, &modifiers);
        } while (modifiers != modifiers_before);

        // TODO: Improve this shit.
        if ((modifiers & MOD_SIGNED || modifiers & MOD_UNSIGNED) && (strcmp(id, "float") == 0 || strcmp(id, "double") == 0))
            log_error(ERROR_CRITICAL, "Invalid combination of signedness modifier and floating-point type '%s'\n", id);

        if (!id_is_data_type(parser->token->value)) {
            // Only modifiers specified, no type name, default to int.
            parser->token_index--;
            parser->token = &parser->tokens[parser->token_index];
            free(id);
            id = malloc(4);
            strcpy(id, "int");
        }
    }

    if (parser->token->type == TOKEN_ID) {
        char *name = malloc(strlen(parser->token->value) + 1);
        strcpy(name, parser->token->value);
        eat(parser, TOKEN_ID);

        if (parser->token->type == TOKEN_LPAREN)
            return parse_function(parser, id, name, modifiers);

        if (modifiers > 0)
            eat(parser, TOKEN_ID);

        return parse_declaration(parser, id, name, modifiers);
    }

    if (!(parser->flags & PARSING_VALUE) && modifiers != MOD_NO_MODS) {
        log_error(ERROR_CRITICAL, "Expected symbol name following modifier '%s'\n", id);
        free(id);
        return fob_new_ast_nop();
    }

    Symbol *symbol = find_symbol(SYMBOL_VARIABLE, id, &parser->scope);

    if (symbol != NULL)
        return parse_variable(id, symbol);

    log_error(ERROR_CRITICAL, "Undefined identifier '%s'\n", id);
    free(id);
    return fob_new_ast_nop();
}

static FOB_AST *parse_return(Parser *parser) {
    FOB_AST *value = parser->token->type == TOKEN_SEMICOLON ? NULL : parse_value(parser);
    Symbol *symbol = find_function_symbol_by_function_uid(SYMBOL_FUNCTION, parser->scope.function_uid);

    if (symbol == NULL) {
        log_error(ERROR_CRITICAL, "Return statement in global scope\n");
        return fob_new_ast_return(fob_create_data_type(FOB_PRIM_VOID), value);
    }

    if (value != NULL && symbol->data_type.primitive_type == FOB_PRIM_VOID)
        log_error(ERROR_CRITICAL, "Unexpected return value in function '%s'\n", symbol->name);
    else if (value == NULL && symbol->data_type.primitive_type != FOB_PRIM_VOID) {
        char *type = fob_data_type_to_string(&symbol->data_type);
        log_error(ERROR_CRITICAL, "Missing return value in function '%s' of type '%s'\n", symbol->name, type);
        free(type);
    }

    return fob_new_ast_return(symbol->data_type, value);
}

static FOB_AST *parse_operator(Parser *parser) {
    FOB_AST_Operator_Type type = 0;

    switch (parser->token->type) {
        case TOKEN_AND:
            type = FOB_AST_OPERATOR_AND;
            break;
        case TOKEN_OR:
            type = FOB_AST_OPERATOR_OR;
            break;
        case TOKEN_XOR:
            type = FOB_AST_OPERATOR_XOR;
            break;
        case TOKEN_NOT:
            type = FOB_AST_OPERATOR_NOT;
            break;
        case TOKEN_PLUS:
            type = FOB_AST_OPERATOR_ADD;
            break;
        case TOKEN_MINUS:
            type = FOB_AST_OPERATOR_SUB;
            break;
        case TOKEN_STAR:
            type = FOB_AST_OPERATOR_MUL;
            break;
        case TOKEN_SLASH:
            type = FOB_AST_OPERATOR_DIV;
            break;
        case TOKEN_PERCENT:
            type = FOB_AST_OPERATOR_MOD;
            break;
        case TOKEN_SHIFTL:
            type = FOB_AST_OPERATOR_SHL;
            break;
        case TOKEN_SHIFTR:
            type = FOB_AST_OPERATOR_SHR;
            break;
        case TOKEN_EQUAL_EQUAL:
            type = FOB_AST_OPERATOR_EQ;
            break;
        case TOKEN_NOT_EQUAL:
            type = FOB_AST_OPERATOR_NEQ;
            break;
        case TOKEN_LESS:
            type = FOB_AST_OPERATOR_LT;
            break;
        case TOKEN_LESS_EQUAL:
            type = FOB_AST_OPERATOR_LTE;
            break;
        case TOKEN_GREATER:
            type = FOB_AST_OPERATOR_GT;
            break;
        case TOKEN_GREATER_EQUAL:
            type = FOB_AST_OPERATOR_GTE;
            break;
        case TOKEN_BOOL_AND:
            type = FOB_AST_OPERATOR_BOOL_AND;
            break;
        case TOKEN_BOOL_OR:
            type = FOB_AST_OPERATOR_BOOL_OR;
            break;
        default:
            log_compiler_error("Uncaught operator type %u\n", parser->token->type);
            break;
    }

    step(parser);
    return fob_new_ast_operator(type);
}

static bool is_conditional_operator_token(Parser *parser) {
    switch (parser->token->type) {
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_BOOL_AND:
        case TOKEN_BOOL_OR: return true;
        default: return false;
    }
} 

static FOB_AST *parse_condition(Parser *parser, FOB_AST *first) {
    parser->flags |= PARSING_CONDITION;
    FOB_AST_List expr = fob_create_ast_list();

    if (first == NULL)
        first = parse_value(parser);

    const FOB_Data_Type first_dt = fob_get_ast_data_type(first);
    bool is_unsigned = fob_primitive_is_unsigned(first_dt.primitive_type);
    bool is_float = fob_primitive_is_float(first_dt.primitive_type);
    bool is_f64 = first_dt.primitive_type == FOB_PRIM_F64;


    if (!is_conditional_operator_token(parser) || parser->token->type == TOKEN_BOOL_AND || parser->token->type == TOKEN_BOOL_OR)
        fob_push_single_valued_condition(&expr, first, true);
    else
        fob_push_ast_node(&expr, first);

    while (is_conditional_operator_token(parser)) {
        if (parser->token->type == TOKEN_BOOL_AND || parser->token->type == TOKEN_BOOL_OR) {
            fob_push_ast_node(&expr, parse_operator(parser));
            FOB_AST *value = parse_value(parser);

            if (!is_conditional_operator_token(parser) || parser->token->type == TOKEN_BOOL_AND || parser->token->type == TOKEN_BOOL_OR)
                fob_push_single_valued_condition(&expr, value, true);
            else
                fob_push_ast_node(&expr, value);

            if (!is_conditional_operator_token(parser))
                break;
        }

        fob_push_ast_node(&expr, parse_operator(parser));

        FOB_AST *value = parse_value(parser);

        if (is_f64) {
            fob_push_ast_node(&expr, value);
            continue;
        }

        const FOB_Data_Type value_dt = fob_get_ast_data_type(value);

        if (!is_float && fob_primitive_is_float(value_dt.primitive_type))
            is_float = true;

        if (!is_f64 && value_dt.primitive_type == FOB_PRIM_F64)
            is_f64 = true;
        else if (is_unsigned && !fob_primitive_is_unsigned(value_dt.primitive_type))
            is_unsigned = false;

        fob_push_ast_node(&expr, value);
    }

    FOB_Data_Type dt;

    if (is_f64)
        dt = fob_create_data_type(FOB_PRIM_F64);
    else if (is_float)
        dt = fob_create_data_type(FOB_PRIM_F32);
    else if (is_unsigned)
        dt = fob_create_data_type(FOB_PRIM_U64);
    else
        dt = fob_create_data_type(FOB_PRIM_I64);

    parser->flags &= ~PARSING_CONDITION;
    return fob_new_ast_condition(expr, dt);
}

static FOB_AST *parse_if(Parser *parser) {
    FOB_AST *condition = parse_condition(parser, NULL);

    const size_t scope_uid = parser->scope.scope_uid;
    parser->scope.scope_uid = parser->scope_uids++;
    parser->scope.indent_level++;

    FOB_AST_List body = parse_body(parser, false);
    FOB_AST_List else_body;
    
    if (strcmp(parser->token->value, "else") == 0) {
        eat(parser, TOKEN_KEYWORD);
        parser->scope.scope_uid = parser->scope_uids++;
        else_body = parse_body(parser, false);
    } else
        else_body = fob_create_ast_list();

    parser->scope.scope_uid = scope_uid;
    parser->scope.indent_level--;
    return fob_new_ast_if(condition, body, else_body);
}

static FOB_AST *parse_inline_asm(Parser *parser) {
    eat(parser, TOKEN_LPAREN);

    char *code = malloc(strlen(parser->token->value) + 1);
    strcpy(code, parser->token->value);
    eat(parser, TOKEN_STRING);

    eat(parser, TOKEN_RPAREN);
    return fob_new_ast_inline_assembly(code);
}

static FOB_AST *parse_for(Parser *parser) {
    const size_t flags = parser->flags;
    parser->flags |= PARSING_FOR;
    eat(parser, TOKEN_LPAREN);

    const size_t scope_uid = parser->scope.scope_uid;
    parser->scope.scope_uid = parser->scope_uids++;
    parser->scope.indent_level++;
    parser->flags |= PARSING_LOOP;

    FOB_AST *stmt = parser->token->type == TOKEN_SEMICOLON ? NULL : parse(parser);
    eat(parser, TOKEN_SEMICOLON);

    FOB_AST *condition = parser->token->type == TOKEN_SEMICOLON ? NULL : parse_condition(parser, NULL);
    eat(parser, TOKEN_SEMICOLON);

    FOB_AST *repeater = parser->token->type == TOKEN_SEMICOLON ? NULL : parse(parser);
    eat(parser, TOKEN_RPAREN);

    parser->flags &= ~PARSING_FOR;
    FOB_AST_List body = parse_body(parser, false);
    parser->flags = flags;

    parser->scope.scope_uid = scope_uid;
    parser->scope.indent_level--;
    return fob_new_ast_for(stmt, condition, repeater, body);
}

static FOB_AST *parse_while(Parser *parser, const bool do_first) {
    eat(parser, TOKEN_KEYWORD);

    const size_t scope_uid = parser->scope.scope_uid;
    parser->scope.scope_uid = parser->scope_uids++;
    parser->scope.indent_level++;

    const size_t flags = parser->flags;
    parser->flags |= PARSING_LOOP;

    FOB_AST *condition;
    FOB_AST_List body;
    
    if (do_first) {
        body = parse_body(parser, false);

        if (strcmp(parser->token->value, "while") != 0)
            log_error(ERROR_CRITICAL, "Expected 'while' keyword to match 'do' but found '%s'\n", token_type_to_string(parser->token->type));

        step(parser);
        condition = parse_condition(parser, NULL);
        eat(parser, TOKEN_SEMICOLON);
    } else {
        condition = parse_condition(parser, NULL);
        body = parse_body(parser, false);
    }

    parser->flags = flags;
    parser->scope.scope_uid = scope_uid;
    parser->scope.indent_level--;
    return fob_new_ast_while(condition, body, do_first);
}

static FOB_AST *parse_keyword(Parser *parser) {
    if (strcmp(parser->token->value, "return") == 0) {
        eat(parser, TOKEN_KEYWORD);
        return parse_return(parser);
    } else if (strcmp(parser->token->value, "if") == 0) {
        eat(parser, TOKEN_KEYWORD);
        return parse_if(parser);
    } else if (strcmp(parser->token->value, "else") == 0) {
        eat(parser, TOKEN_KEYWORD);
        log_error(ERROR_CRITICAL, "else statement without matching if statement\n");
        return fob_new_ast_nop();
    } else if (strcmp(parser->token->value, "asm") == 0) {
        eat(parser, TOKEN_KEYWORD);
        return parse_inline_asm(parser);
    } else if (strcmp(parser->token->value, "for") == 0) {
        eat(parser, TOKEN_KEYWORD);
        return parse_for(parser);
    } else if (strcmp(parser->token->value, "break") == 0) {
        eat(parser, TOKEN_KEYWORD);

        if (!(parser->flags & PARSING_LOOP))
            log_error(ERROR_CRITICAL, "Invalid break statement outside of a loop\n");

        return fob_new_ast_break();
    } else if (strcmp(parser->token->value, "continue") == 0) {
        eat(parser, TOKEN_KEYWORD);

        if (!(parser->flags & PARSING_LOOP))
            log_error(ERROR_CRITICAL, "Invalid continue statement outside of a loop\n");

        return fob_new_ast_continue();
    } else if (strcmp(parser->token->value, "while") == 0)
        return parse_while(parser, false);
    else if (strcmp(parser->token->value, "do") == 0)
        return parse_while(parser, true);

    log_compiler_error("Uncaught keyword '%s'\n", parser->token->value);
    step(parser);
    return fob_new_ast_nop();
}

static FOB_AST *parse_digit(Parser *parser) {
    char *endptr;
    uint64_t u64 = 0;
    double f64 = 0;

    if (parser->token->type == TOKEN_INT) {
        errno = 0;
        u64 = strtoull(parser->token->value, &endptr, 10);
    } else {
        errno = 0;
        f64 = strtod(parser->token->value, &endptr);
    }

    if (endptr == parser->token->value || errno == ERANGE)
        log_error(ERROR_CRITICAL, "Digit conversion failed: %s\n", strerror(errno));
    else if (*endptr != '\0')
        log_error(ERROR_CRITICAL, "Digit conversion failed; characters left in buffer\n");

    if (parser->token->type == TOKEN_FLOAT) {
        step(parser);
        return fob_new_ast_constant(fob_create_data_type(FOB_PRIM_F64), (FOB_AST_Constant_Value){ .f64 = f64 });
    }

    step(parser);
    return fob_new_ast_constant(fob_create_data_type(FOB_PRIM_U64), (FOB_AST_Constant_Value){ .u64 = u64 });
}

static FOB_AST *parse_string(Parser *parser) {
    char *text = malloc(strlen(parser->token->value) + 1);
    strcpy(text, parser->token->value);
    eat(parser, TOKEN_STRING);
    return fob_new_ast_string(text);
}

static FOB_AST *parse_assignment(Parser *parser, FOB_AST *lhs) {
    eat(parser, TOKEN_EQUAL);
    return fob_new_ast_assignment(lhs, parse_value(parser));
}

static bool is_arithmetic_operator_token(Parser *parser) {
    switch (parser->token->type) {
        case TOKEN_AND:
        case TOKEN_OR:
        case TOKEN_XOR:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
        case TOKEN_SHIFTL:
        case TOKEN_SHIFTR: return true;
        default: return false;
    }
} 

static FOB_AST *parse_arithmetic(Parser *parser, FOB_AST *first) {
    parser->flags |= PARSING_ARITHMETIC;
    FOB_AST_List expr = fob_create_ast_list();

    if (first == NULL)
        first = parse_value(parser);

    const FOB_Data_Type first_dt = fob_get_ast_data_type(first);
    bool is_unsigned = fob_primitive_is_unsigned(first_dt.primitive_type);
    bool is_float = fob_primitive_is_float(first_dt.primitive_type);
    bool is_f64 = first_dt.primitive_type == FOB_PRIM_F64;

    fob_push_ast_node(&expr, first);

    while (is_arithmetic_operator_token(parser)) {
        fob_push_ast_node(&expr, parse_operator(parser));

        FOB_AST *value = parse_value(parser);

        if (is_f64) {
            fob_push_ast_node(&expr, value);
            continue;
        }

        const FOB_Data_Type value_dt = fob_get_ast_data_type(value);

        if (!is_float && fob_primitive_is_float(value_dt.primitive_type))
            is_float = true;

        if (!is_f64 && value_dt.primitive_type == FOB_PRIM_F64)
            is_f64 = true;
        else if (is_unsigned && !fob_primitive_is_unsigned(value_dt.primitive_type))
            is_unsigned = false;

        fob_push_ast_node(&expr, value);
    }

    FOB_Data_Type dt;

    if (is_f64)
        dt = fob_create_data_type(FOB_PRIM_F64);
    else if (is_float)
        dt = fob_create_data_type(FOB_PRIM_F32);
    else if (is_unsigned)
        dt = fob_create_data_type(FOB_PRIM_U64);
    else
        dt = fob_create_data_type(FOB_PRIM_I64);

    parser->flags &= ~PARSING_ARITHMETIC;
    return fob_new_ast_arithmetic(expr, dt);
}

static FOB_AST *parse_parentheses_expression(Parser *parser) {
    const size_t flags = parser->flags;
    parser->flags = 0;

    eat(parser, TOKEN_LPAREN);
    FOB_AST *value = parse_value(parser);
    eat(parser, TOKEN_RPAREN);

    parser->flags = flags;
    return fob_new_ast_parentheses_expression(value);
}

static FOB_AST *parse_unary_not(Parser *parser) {
    const bool as_bool = parser->token->type == TOKEN_BOOL_NOT;
    step(parser);
    parser->flags |= PARSING_SINGLE_VALUE;
    FOB_AST *value = parse_value(parser);
    parser->flags &= ~PARSING_SINGLE_VALUE;
    return fob_new_ast_unary_not(value, as_bool);
}

static FOB_AST *parse_unary_negate(Parser *parser) {
    eat(parser, TOKEN_MINUS);
    parser->flags |= PARSING_SINGLE_VALUE;
    FOB_AST *value = parse_value(parser);
    parser->flags &= ~PARSING_SINGLE_VALUE;
    return fob_new_ast_unary_negate(value);
}

static FOB_AST *parse_pointer(Parser *parser) {
    eat(parser, TOKEN_AND);
    parser->flags |= PARSING_SINGLE_VALUE;
    FOB_AST *value = parse_value(parser);
    parser->flags &= ~PARSING_SINGLE_VALUE;
    return fob_new_ast_pointer(value);
}

static FOB_AST *parse_dereference(Parser *parser) {
    eat(parser, TOKEN_STAR);
    parser->flags |= PARSING_SINGLE_VALUE;
    FOB_AST *value = parse_value(parser);
    parser->flags &= ~PARSING_SINGLE_VALUE;
    return fob_new_ast_dereference(value);
}

static void skip_semicolons(Parser *parser) {
    if (parser->flags & PARSING_FOR)
        return;

    while (parser->token->type == TOKEN_SEMICOLON)
        step(parser);
}

static FOB_AST *parse_index(Parser *parser, FOB_AST *pointer) {
    eat(parser, TOKEN_LSQUARE);
    FOB_AST *offset = parse_value(parser);
    eat(parser, TOKEN_RSQUARE);
    return fob_new_ast_index(pointer, offset);
}

static FOB_AST *parse_pre_operation(Parser *parser) {
    const Token_Type token = parser->token->type;
    assert(token == TOKEN_PLUS || token == TOKEN_MINUS);
    const FOB_AST_Operator_Type type = token == TOKEN_PLUS ? FOB_AST_OPERATOR_ADD : FOB_AST_OPERATOR_SUB;
    eat(parser, token);
    eat(parser, token);

    parser->flags |= PARSING_SINGLE_VALUE;
    FOB_AST *value = parse_value(parser);
    parser->flags &= ~PARSING_SINGLE_VALUE;
    return fob_new_ast_pre_operation(type, value);
}

static FOB_AST *parse_post_operation(Parser *parser, FOB_AST *value) {
    const Token_Type token = parser->token->type;
    assert(token == TOKEN_PLUS || token == TOKEN_MINUS);
    const FOB_AST_Operator_Type type = token == TOKEN_PLUS ? FOB_AST_OPERATOR_ADD : FOB_AST_OPERATOR_SUB;
    eat(parser, token);
    eat(parser, token);
    return fob_new_ast_post_operation(type, value);
}

static FOB_AST *parse(Parser *parser) {
    if (!(parser->flags & PARSING_VALUE))
        skip_semicolons(parser);

    FOB_AST *stmt = NULL;

    if ((parser->token->type == TOKEN_PLUS || parser->token->type == TOKEN_MINUS) && peek(parser, 1)->type == parser->token->type) {
        stmt = parse_pre_operation(parser);
        goto done_value; // BOOO!!! So scary!!
    }

    switch (parser->token->type) {
        case TOKEN_EOF:
            stmt = fob_new_ast_nop();
            break;
        case TOKEN_ID:
            stmt = parse_identifier(parser);
            break;
        case TOKEN_KEYWORD:
            stmt = parse_keyword(parser);
            break;
        case TOKEN_INT:
        case TOKEN_FLOAT:
            stmt = parse_digit(parser);
            break;
        case TOKEN_STRING:
            stmt = parse_string(parser);
            break;
        case TOKEN_LPAREN:
            stmt = parse_parentheses_expression(parser);
            break;
        case TOKEN_NOT:
        case TOKEN_BOOL_NOT:
            stmt = parse_unary_not(parser);
            break;
        case TOKEN_MINUS:
            if (peek(parser, 1)->type == TOKEN_MINUS)
                stmt = parse_pre_operation(parser);
            else
                stmt = parse_unary_negate(parser);
            break;
        case TOKEN_AND:
            stmt = parse_pointer(parser);
            break;
        case TOKEN_STAR:
            stmt = parse_dereference(parser);
            break;
        case TOKEN_PLUS:
            if (peek(parser, 1)->type == parser->token->type)
                stmt = parse_pre_operation(parser);
            break;
        default: break;
    }

done_value:

    if (stmt == NULL) {
        log_error(ERROR_CRITICAL, "Invalid statement '%s'\n", token_type_to_string(parser->token->type));
        step(parser);
        skip_semicolons(parser);
        return parse(parser);
    }

    if (!(parser->flags & PARSING_VALUE)) {
        if (parser->token->type == TOKEN_LSQUARE)
            stmt = parse_index(parser, stmt);

        if ((parser->token->type == TOKEN_PLUS || parser->token->type == TOKEN_MINUS) && peek(parser, 1)->type == parser->token->type)
            stmt = parse_post_operation(parser, stmt);

        if (parser->token->type == TOKEN_EQUAL)
            stmt = parse_assignment(parser, stmt);

        skip_semicolons(parser);
        return stmt;
    }

    if (parser->flags & PARSING_SINGLE_VALUE) {
        skip_semicolons(parser);
        return stmt;
    }

    while (true) {
        if ((parser->token->type == TOKEN_PLUS || parser->token->type == TOKEN_MINUS) && peek(parser, 1)->type == parser->token->type) {
            stmt = parse_post_operation(parser, stmt);
            continue;
        }

        if (!(parser->flags & PARSING_ARITHMETIC) && is_arithmetic_operator_token(parser)) {
            stmt = parse_arithmetic(parser, stmt);
            continue;
        }

        if (!(parser->flags & PARSING_CONDITION) && is_conditional_operator_token(parser)) {
            stmt = parse_condition(parser, stmt);
            continue;
        }

        if (parser->token->type == TOKEN_LSQUARE) {
            stmt = parse_index(parser, stmt);
            continue;
        }

        break;
    }

    if (!(parser->flags & PARSING_VALUE))
        skip_semicolons(parser);

    return stmt;
}

FOB_AST_List parse_file(char *filename) {
    Parser prs = create_parser(filename);
    FOB_AST_List root = fob_create_ast_list();

    while (prs.token->type != TOKEN_EOF)
        fob_push_ast_node(&root, parse(&prs));

    delete_parser(&prs);
    return root;
}
