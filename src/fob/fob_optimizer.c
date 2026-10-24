// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#include "fob_optimizer.h"
#include "fob_ir.h"
#include <stdio.h>

#define PASS_COUNT 3

#define get_op \
    FOB_Op *this_op = peek(optimizer, 0)

#define get_ops \
    get_op; \
    FOB_Op *next_op = peek(optimizer, 1)

typedef struct {
    FOB_IR *ir;
    size_t index;
} FOB_Generic_Optimizer;

static inline FOB_Generic_Optimizer create_generic_optimizer(FOB_IR *ir) {
    return (FOB_Generic_Optimizer){ .ir = ir, .index = 0 };
}

static inline void omit(FOB_Op *op) {
    op->type = FOB_OP_NOP;
}

static void do_pass(FOB_Generic_Optimizer *optimizer);

void fob_optimize_ir(FOB_IR *ir, const FOB_Optimization_Level level) {
    (void)level;
    FOB_Generic_Optimizer opt = create_generic_optimizer(ir);

    for (size_t i = 0; i < PASS_COUNT; i++)
        do_pass(&opt);
}

static FOB_Op *peek(FOB_Generic_Optimizer *optimizer, const int offset) {
    if (optimizer->index + offset >= optimizer->ir->count || (int)optimizer->index + offset < 0)
        return &optimizer->ir->ops[optimizer->ir->count - 1]; // EOF

    FOB_Op *op = &optimizer->ir->ops[optimizer->index + offset];

    if (op->type == FOB_OP_NOP)
        return peek(optimizer, offset > 0 ? offset + 1 : offset - 1);

    return op;
}

static void eliminate_self_loads(FOB_Generic_Optimizer *optimizer) {
    get_op;

    // LOAD_MEMORY %0, %0 (same types)
    // ->
    // (nop)
    if (this_op->type == FOB_OP_LOAD_MEMORY && this_op->destination.type == FOB_OPER_REGISTER && this_op->source.type == FOB_OPER_REGISTER &&
            this_op->destination.value.register_.number == this_op->source.value.register_.number && 
            this_op->destination.data_type.primitive_type == this_op->source.data_type.primitive_type) {
        omit(this_op);
        return;
    }
}

static void eliminate_stack_operations(FOB_Generic_Optimizer *optimizer) {
    get_ops;

    if (this_op->type == FOB_OP_PUSH && this_op->source.type == FOB_OPER_REGISTER &&
            next_op->type == FOB_OP_POP && next_op->destination.type == FOB_OPER_REGISTER) {

        // PUSH %0
        // POP %0
        // ->
        // (nop)
        if (this_op->source.value.register_.number == next_op->destination.value.register_.number) {
            omit(this_op);
            omit(next_op);
            return;
        }

        // PUSH %0
        // POP %1
        // ->
        // LOAD_MEMORY %1, %0
        omit(this_op);
        next_op->type = FOB_OP_LOAD_MEMORY;
        next_op->source = this_op->source;
        return;
    }
}

static void do_pass(FOB_Generic_Optimizer *optimizer) {
    for (optimizer->index = 0; optimizer->index + 1 < optimizer->ir->count; optimizer->index++) {
        eliminate_self_loads(optimizer);
        eliminate_stack_operations(optimizer);
    }
}
