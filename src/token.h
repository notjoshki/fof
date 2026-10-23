#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_ID,
    TOKEN_KEYWORD,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_XOR,
    TOKEN_NOT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_SHIFTL,
    TOKEN_SHIFTR,
    TOKEN_BOOL_AND,
    TOKEN_BOOL_OR,
    TOKEN_BOOL_NOT
} Token_Type;

typedef struct {
    Token_Type type;
    char *value;
    size_t ln;
    size_t col;
} Token;

static inline Token create_token(Token_Type type, char *value, size_t ln, size_t col) {
    return (Token){ .type = type, .value = value, .ln = ln, .col = col };
}

void delete_token(Token *token);
char *token_type_to_string(const Token_Type type);

#endif
