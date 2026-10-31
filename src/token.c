#include "token.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void delete_token(Token *token) {
    if (token->type == TOKEN_ID || token->type == TOKEN_KEYWORD || token->type == TOKEN_INT || token->type == TOKEN_FLOAT || token->type == TOKEN_STRING)
        free(token->value);
}

char *token_type_to_string(const Token_Type type) {
    switch (type) {
        case TOKEN_EOF: return "eof";
        case TOKEN_ID: return "identifier";
        case TOKEN_KEYWORD: return "keyword";
        case TOKEN_INT: return "int";
        case TOKEN_FLOAT: return "float";
        case TOKEN_STRING: return "string";
        case TOKEN_LPAREN: return "lparen";
        case TOKEN_RPAREN: return "rparen";
        case TOKEN_LBRACE: return "lbrace";
        case TOKEN_RBRACE: return "rbrace";
        case TOKEN_COMMA: return "comma";
        case TOKEN_SEMICOLON: return "semicolon";
        case TOKEN_EQUAL: return "equal";
        case TOKEN_AND: return "and";
        case TOKEN_OR: return "or";
        case TOKEN_XOR: return "xor";
        case TOKEN_NOT: return "not";
        case TOKEN_PLUS: return "plus";
        case TOKEN_MINUS: return "minus";
        case TOKEN_STAR: return "star";
        case TOKEN_SLASH: return "slash";
        case TOKEN_PERCENT: return "percent";
        case TOKEN_LESS: return "less";
        case TOKEN_LESS_EQUAL: return "less equal";
        case TOKEN_GREATER: return "greater";
        case TOKEN_GREATER_EQUAL: return "greater equal";
        case TOKEN_EQUAL_EQUAL: return "equal equal";
        case TOKEN_NOT_EQUAL: return "not equal";
        case TOKEN_SHIFTL: return "shiftl";
        case TOKEN_SHIFTR: return "shiftr";
        case TOKEN_BOOL_AND: return "boolean and";
        case TOKEN_BOOL_OR: return "boolean or";
        case TOKEN_BOOL_NOT: return "boolean not";
        case TOKEN_LSQUARE: return "lsquare";
        case TOKEN_RSQUARE: return "rsquare";
        default: break;
    }

    log_compiler_error("Uncaught token type %u\n", type);
    return "\0";
}
