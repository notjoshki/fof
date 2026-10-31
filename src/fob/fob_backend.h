// FOB - Compiler Backend API
// Copyright (c) 2026 notjoshki
// https://github.com/notjoshki/fob.git

#ifndef FOB_BACKEND_H
#define FOB_BACKEND_H

char *fob_emit_ir(const FOB_IR *ir, const char *entrypoint_name);
size_t data_type_to_hardware_byte_size(const FOB_Data_Type *data_type);

#endif
