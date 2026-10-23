#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdio.h>

typedef struct {
    char *filename;
    char *source;
    size_t source_length;
    size_t index;
    char current_char;
    size_t ln;
    size_t col;
} Lexer;

Lexer create_lexer(char *filename);
void delete_lexer(Lexer *lexer);
Token get_next_token(Lexer *lexer);

#endif
