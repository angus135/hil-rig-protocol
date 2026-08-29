/**
 * @file hil_rig_protocol_ffi_allocator.h
 * @brief Binding-private allocation seam for the Python adapter.
 */
#ifndef HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H
#define HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H

#include <stddef.h>

void* HIL_PY_ADAPTER_Calloc( size_t count, size_t size );
void  HIL_PY_ADAPTER_Free( void* allocation );

#endif /* HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H */
