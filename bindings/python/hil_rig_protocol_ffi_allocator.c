/**
 * @file hil_rig_protocol_ffi_allocator.c
 * @brief Production allocator implementation for the Python adapter.
 */
#include "hil_rig_protocol_ffi_allocator.h"

#include <stdlib.h>

void* HIL_PY_ADAPTER_Calloc( size_t count, size_t size )
{
    return calloc( count, size );
}

void HIL_PY_ADAPTER_Free( void* allocation )
{
    free( allocation );
}
