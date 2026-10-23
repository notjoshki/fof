#include "lexer.h"
#include "token.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#define IDENTIFIER_BUFFER_CAPACITY 8
#define DIGIT_BUFFER_CAPACITY 8
#define STRING_CAPACITY 8

#define KEYWORD_COUNT 4

static const char *keywords[KEYWORD_COUNT] = {
    "return", "if", "else", "asm"
};

static bool is_keyword(char *identifier) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(keywords[i], identifier) == 0)
            return true;
    }
    
    return false;
}

Lexer create_lexer(char *filename) {
    Lexer lex = (Lexer){ .filename = filename, .index = 0, .ln = 1, .col = 1 };
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        log_error(ERROR_CRITICAL, "No such file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    lex.source = malloc(size + 1);
    lex.source_length = fread(lex.source, sizeof(char), size, file);
    fclose(file);

    if (lex.source_length != size || lex.source_length == 0) {
        log_error(ERROR_CRITICAL, "Failed to read file '%s'\n", filename);
        free(lex.source);
        exit(EXIT_FAILURE);
    }

    lex.source[lex.source_length] = '\0';
    lex.current_char = lex.source[0];
    return lex;
}

void delete_lexer(Lexer *lexer) {
    free(lexer->source);
}

static void step(Lexer *lexer) {
    if (lexer->current_char == '\n') {
        lexer->ln++;
        lexer->col = 1;
    } else
        lexer->col++;

    lexer->current_char = lexer->source[++lexer->index];
}

static char peek(Lexer *lexer, const int offset) {
    if (lexer->index + 1 >= lexer->source_length)
        return '\0';
    
    if ((int)lexer->index + offset <= 0)
        return lexer->source[0];

    return lexer->source[lexer->index + offset];
}

static Token create_token_and_step(Lexer *lexer, Token_Type type, char *value, const size_t value_length) {
    for (size_t i = 0; i < value_length; i++)
        step(lexer);

    return create_token(type, value, lexer->ln, lexer->col - value_length);
}

static Token lex_identifier(Lexer *lexer) {
    char *value = malloc(IDENTIFIER_BUFFER_CAPACITY + 1);
    size_t len = 0;
    size_t capacity = IDENTIFIER_BUFFER_CAPACITY;

    while (isalpha(lexer->current_char) || lexer->current_char == '_' || isdigit(lexer->current_char)) {
        if (len + 1 > capacity) {
            capacity *= 2;
            value = realloc(value, capacity + 1);
        }

        value[len++] = lexer->current_char;
        step(lexer);
    }

    value[len] = '\0';
    return create_token(is_keyword(value) ? TOKEN_KEYWORD : TOKEN_ID, value, lexer->ln, lexer->col - len);
}

static Token lex_digit(Lexer *lexer) {
    char *value = malloc(DIGIT_BUFFER_CAPACITY + 1);
    size_t len = 0;
    size_t capacity = DIGIT_BUFFER_CAPACITY;
    bool is_float = false;

    while (isdigit(lexer->current_char) || (lexer->current_char == '.' && !is_float)) {
        if (lexer->current_char == '.')
            is_float = true;

        if (len + 1 > capacity) {
            capacity *= 2;
            value = realloc(value, capacity + 1);
        }

        value[len++] = lexer->current_char;
        step(lexer);
    }

    value[len] = '\0';
    return create_token(is_float ? TOKEN_FLOAT : TOKEN_INT, value, lexer->ln, lexer->col - len);
}

static void skip_comment(Lexer *lexer) {
    step(lexer);

    if (lexer->current_char == '*') {
        step(lexer);

        while (lexer->current_char != '\0' && (lexer->current_char != '*' || peek(lexer, 1) != '/'))
            step(lexer);

        if (lexer->current_char != '\0')
            step(lexer);

        if (lexer->current_char != '\0')
            step(lexer);

        return;
    }

    assert(lexer->current_char == '/');

    while (lexer->current_char != '\0' && lexer->current_char != '\n')
        step(lexer);

    if (lexer->current_char == '\n')
        step(lexer);
}

static Token lex_string(Lexer *lexer) {
    const size_t ln = lexer->ln;
    const size_t col = lexer->col;
    step(lexer);

    char *value = malloc(STRING_CAPACITY + 1);
    size_t length = 0;
    size_t capacity = STRING_CAPACITY;

    while (lexer->current_char != '\0' && (lexer->current_char != '"' || peek(lexer, -1) == '\\')) {
        if (length + 1 > capacity) {
            capacity *= 2;
            value = realloc(value, capacity);
        }

        value[length++] = lexer->current_char;
        step(lexer);
    }

    value[length] = '\0'; // Need this for continuous string constants to be appended.

    if (lexer->current_char != '"')
        log_error(ERROR_CRITICAL, "Unclosed string literal\n");
    else
        step(lexer);

    // Concatenate the following string if present.
    while (isspace(lexer->current_char))
        step(lexer);

    if (lexer->current_char == '"') {
        Token next;

        while (lexer->current_char == '"') {
            next = lex_string(lexer);
            const size_t len = strlen(next.value);

            while (length + len > capacity)
                capacity *= 2;

            value = realloc(value, capacity + 1);
            strncat(value, next.value, len);
            length += len;
            value[length] = '\0';
            free(next.value);

            while (isspace(lexer->current_char))
                step(lexer);
        }
    }

    return create_token(TOKEN_STRING, value, ln, col);
}

static Token lex_character(Lexer *lexer) {
    const size_t ln = lexer->ln;
    const size_t col = lexer->col;
    step(lexer);

    int code = 0;

    if (lexer->current_char == '\\') {
        step(lexer);

        switch (lexer->current_char) {
            case 'n':
                code = '\n';
                break;
            case 't':
                code = '\t';
                break;
            case 'r':
                code = '\r';
                break;
            case '0': break;
            case '\'':
            case '"':
            case '\\':
                code = (int)lexer->current_char;
                break;
            default:
                log_error(ERROR_CRITICAL, "Unsupported escape sequence '\\%c'\n", lexer->current_char);
                break;
        }
    } else
        code = (int)lexer->current_char;

    step(lexer);

    char *value = malloc(16);
    sprintf(value, "%d", code);

    if (lexer->current_char != '\'')
        log_error(ERROR_CRITICAL, "Unclosed character constant\n");
    else
        step(lexer);

    return create_token(TOKEN_INT, value, ln, col);
}

Token get_next_token(Lexer *lexer) {
    while (isspace(lexer->current_char))
        step(lexer);

    switch (lexer->current_char) {
        case '\0': return create_token(TOKEN_EOF, "\0", lexer->ln, lexer->col);
        case '\'': return lex_character(lexer);
        case '"': return lex_string(lexer);
        case '(': return create_token_and_step(lexer, TOKEN_LPAREN, "(", 1);
        case ')': return create_token_and_step(lexer, TOKEN_RPAREN, ")", 1);
        case '{': return create_token_and_step(lexer, TOKEN_LBRACE, "{", 1);
        case '}': return create_token_and_step(lexer, TOKEN_RBRACE, "}", 1);
        case ',': return create_token_and_step(lexer, TOKEN_COMMA, ",", 1);
        case ';': return create_token_and_step(lexer, TOKEN_SEMICOLON, ";", 1);
        case '=':
            if (peek(lexer, 1) == '=')
                return create_token_and_step(lexer, TOKEN_EQUAL_EQUAL, "==", 2);
            return create_token_and_step(lexer, TOKEN_EQUAL, "=", 1);
        case '&':
            if (peek(lexer, 1) == '&')
                return create_token_and_step(lexer, TOKEN_BOOL_AND, "&&", 2);
            return create_token_and_step(lexer, TOKEN_AND, "&", 1);
        case '|':
            if (peek(lexer, 1) == '|')
                return create_token_and_step(lexer, TOKEN_BOOL_OR, "||", 2);
            return create_token_and_step(lexer, TOKEN_OR, "|", 1);
        case '^': return create_token_and_step(lexer, TOKEN_XOR, "^", 1);
        case '~': return create_token_and_step(lexer, TOKEN_NOT, "~", 1);
        case '+': return create_token_and_step(lexer, TOKEN_PLUS, "+", 1);
        case '-': return create_token_and_step(lexer, TOKEN_MINUS, "-", 1);
        case '*': return create_token_and_step(lexer, TOKEN_STAR, "*", 1);
        case '/':
            if (peek(lexer, 1) == '/' || peek(lexer, 1) == '*') {
                skip_comment(lexer);
                return get_next_token(lexer);
            }

            return create_token_and_step(lexer, TOKEN_SLASH, "/", 1);
        case '%': return create_token_and_step(lexer, TOKEN_PERCENT, "%", 1);
        case '<':
            if (peek(lexer, 1) == '=')
                return create_token_and_step(lexer, TOKEN_LESS_EQUAL, "<=", 2);
            if (peek(lexer, 1) == '<')
                return create_token_and_step(lexer, TOKEN_SHIFTL, "<<", 2);
            return create_token_and_step(lexer, TOKEN_LESS, "<", 1);
        case '>':
            if (peek(lexer, 1) == '=')
                return create_token_and_step(lexer, TOKEN_GREATER_EQUAL, ">=", 2);
            if (peek(lexer, 1) == '>')
                return create_token_and_step(lexer, TOKEN_SHIFTR, ">>", 2);
            return create_token_and_step(lexer, TOKEN_GREATER, ">", 1);
        case '!':
            if (peek(lexer, 1) == '=')
                return create_token_and_step(lexer, TOKEN_NOT_EQUAL, "!=", 2);
            return create_token_and_step(lexer, TOKEN_BOOL_NOT, "!", 1);
        default: break;
    }

    if (isalpha(lexer->current_char) || lexer->current_char == '_')
        return lex_identifier(lexer);

    if (isdigit(lexer->current_char))
        return lex_digit(lexer);

    log_error(ERROR_CRITICAL, "Unknown character '%c'\n", lexer->current_char);
    step(lexer);
    return get_next_token(lexer);
}
