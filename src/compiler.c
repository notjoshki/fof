#include "compiler.h"
#include "parser.h"
#include "error.h"
#include "symbol.h"
#include "fob_ast.h"
#include "fob_ir.h"
#include "fob_backend.h"
#include "fob_error.h"
#include "fob_optimizer.h"
#include "fob_ir_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

static void cleanup(FOB_AST_List *root, FOB_IR *ir, char *code) {
    fob_delete_ast_list(root, true);

    if (ir != NULL)
        fob_delete_ir(ir);

    if (code != NULL)
        free(code);

    delete_symbols();
}

static char *replace_file_extension(const char *filename, const char *extension) {
    char *last_dot = strrchr(filename, '.');
    char *copy = malloc(strlen(filename) + strlen(extension) + 2);

    if (last_dot == NULL)
        strcpy(copy, filename);
    else {
        size_t diff = (size_t)(last_dot - filename);
        strncpy(copy, filename, diff);
        copy[diff] = '\0';
    }

    strcat(copy, ".");
    strcat(copy, extension);
    return copy;
}

void compile(char *infile, char *outfile, const Compiler_Options options) {
    FOB_AST_List root = parse_file(infile);

    if (get_total_error_count() > 0 || fob_error_thrown()) {
        fprintf(stderr, "Aborting...\n");
        cleanup(&root, NULL, NULL);
        return;
    }

    FOB_IR ir = fob_ast_root_to_ir(&root);

    if (fob_error_thrown()) {
        fprintf(stderr, "Aborting...\n");
        cleanup(&root, &ir, NULL);
        return;
    }

    fob_optimize_ir(&ir, options & COMP_OPTIMIZE_LEVEL_0 ? FOB_OPTIMIZATION_NONE : 
        (options & COMP_OPTIMIZE_LEVEL_1 ? FOB_OPTIMIZATION_GENERIC : FOB_OPTIMIZATION_ALL));

    char *code = options & COMP_IR_ONLY ? fob_ir_to_string(&ir) : fob_emit_ir(&ir, "main");

    if (fob_error_thrown()) {
        fprintf(stderr, "Aborting...\n");
        cleanup(&root, &ir, code);
        return;
    }

    char *outasm = replace_file_extension(options & COMP_OUTFILE_SPECIFIED ? outfile : infile, options & COMP_IR_ONLY ? "ir" : "asm");
    FILE *f = fopen(outasm, "w");
    assert(f != NULL);
    fputs(code, f);
    fclose(f);

    if (options & COMP_ASSEMBLY_ONLY || options & COMP_IR_ONLY) {
        free(outasm);
        cleanup(&root, &ir, code);
        return;
    }

    char *outobj = replace_file_extension(outasm, "o");
    char *cmd = malloc(strlen(outobj) + strlen(outasm) + 25);
    sprintf(cmd, "nasm %s-felf64 -o %s %s", options & COMP_DEBUGINFO ? "-g " : " ", outobj, outasm);

    if (system(cmd) != 0) {
        log_error(ERROR_CRITICAL, "Failed to assemble '%s'\n", outasm);
        remove(outasm);
        free(cmd);
        free(outasm);
        free(outobj);
        cleanup(&root, &ir, code);
        return;
    }

    if (options & COMP_OBJECT_ONLY) {
        if (!(options & COMP_DEBUGINFO))
            remove(outasm);

        free(cmd);
        free(outasm);
        free(outobj);
        cleanup(&root, &ir, code);
        return;
    }

    cmd = realloc(cmd, strlen(outobj) + strlen(outfile) + 12);
    sprintf(cmd, "ld -o %s %s", outfile, outobj);

    if (system(cmd) != 0) {
        log_error(ERROR_CRITICAL, "Failed to link '%s'\n", outfile);

        if (!(options & COMP_DEBUGINFO))
            remove(outasm);

        remove(outobj);
        free(cmd);
        free(outasm);
        free(outobj);
        cleanup(&root, &ir, code);
        return;
    }

    if (!(options & COMP_DEBUGINFO))
        remove(outasm);

    remove(outobj);
    free(cmd);
    free(outasm);
    free(outobj);
    cleanup(&root, &ir, code);
}