#include "parser.h"
#include "error.h"
#include "fob_ast.h"
#include "fob_ir.h"
#include "fob_backend.h"
#include "fob_error.h"
#include "fob_optimizer.h"
#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TAG "0.1.0-prerelease"

static void usage(const char *program) {
    printf("Usage: %s [options...] <file>\n"
           "    -c               Output an object file\n"
           "    -g               Compile with debugging information\n"
           "    -ir              Output an IR file\n"
           "    -o <filename>    Specify the output filename\n"
           "    -O{0,1,2}        Set the optimization level (default=2)\n"
           "    -S               Output an assembly file\n"
           , program);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    char *infile = NULL;
    char *outfile = "a.out";
    Compiler_Options options = COMP_OPTIMIZE_LEVEL_2;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("fof v%s\n", TAG);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-c") == 0)
            options |= COMP_OBJECT_ONLY;
        else if (strcmp(argv[i], "-g") == 0)
            options |= COMP_DEBUGINFO;
        else if (strcmp(argv[i], "-ir") == 0)
            options |= COMP_IR_ONLY;
        else if (strcmp(argv[i], "-o") == 0) {
            if (i == argc - 1) {
                log_error(ERROR_CRITICAL, "Missing argument 'filename' to option '-o'\n");
                return EXIT_FAILURE;
            }

            outfile = argv[++i];
            options |= COMP_OUTFILE_SPECIFIED;
        } else if (strcmp(argv[i], "-O0") == 0)
            options |= COMP_OPTIMIZE_LEVEL_0;
        else if (strcmp(argv[i], "-O1") == 0)
            options |= COMP_OPTIMIZE_LEVEL_1;
        else if (strcmp(argv[i], "-O2") == 0)
            options |= COMP_OPTIMIZE_LEVEL_2;
        else if (strcmp(argv[i], "-S") == 0)
            options |= COMP_ASSEMBLY_ONLY;
        else if (i == argc - 1)
            infile = argv[i];
        else
            log_error(ERROR_CRITICAL, "Unknown option '%s'\n", argv[i]);
    }

    if (infile == NULL) {
        log_error(ERROR_CRITICAL, "Missing input file\n");
        return EXIT_FAILURE;
    }

    compile(infile, outfile, options);
    return EXIT_SUCCESS;
}
