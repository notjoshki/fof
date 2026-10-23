#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>

typedef size_t Compiler_Options;

#define COMP_OUTFILE_SPECIFIED 0x01
#define COMP_OBJECT_ONLY 0x02
#define COMP_ASSEMBLY_ONLY 0x04
#define COMP_OPTIMIZE_LEVEL_0 0x08
#define COMP_OPTIMIZE_LEVEL_1 0x10
#define COMP_OPTIMIZE_LEVEL_2 0x20
#define COMP_IR_ONLY 0x40
#define COMP_DEBUGINFO 0x80

void compile(char *infile, char *outfile, const Compiler_Options options);

#endif