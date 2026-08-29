/** Test-only allocator implementation used by the adapter test executable. */
#include "test_hil_rig_protocol_ffi_allocator.h"

#include <stdlib.h>

#include "hil_rig_protocol_ffi_allocator.h"

#define HIL_PY_TEST_MAX_LIVE_ALLOCATIONS 4u

static void*  live_allocations[HIL_PY_TEST_MAX_LIVE_ALLOCATIONS];
static size_t failing_allocation_call;
static size_t allocation_count;
static size_t free_count;
static size_t invalid_free_count;

void HIL_PY_TEST_ALLOCATOR_Reset( size_t failing_call )
{
    size_t index;

    for ( index = 0u; index < HIL_PY_TEST_MAX_LIVE_ALLOCATIONS; ++index )
    {
        if ( live_allocations[index] != NULL )
        {
            free( live_allocations[index] );
            live_allocations[index] = NULL;
        }
    }
    failing_allocation_call = failing_call;
    allocation_count        = 0u;
    free_count              = 0u;
    invalid_free_count      = 0u;
}

void* HIL_PY_ADAPTER_Calloc( size_t count, size_t size )
{
    void*  allocation;
    size_t index;

    ++allocation_count;
    if ( allocation_count == failing_allocation_call )
    {
        return NULL;
    }

    allocation = calloc( count, size );
    if ( allocation == NULL )
    {
        return NULL;
    }

    for ( index = 0u; index < HIL_PY_TEST_MAX_LIVE_ALLOCATIONS; ++index )
    {
        if ( live_allocations[index] == NULL )
        {
            live_allocations[index] = allocation;
            return allocation;
        }
    }

    free( allocation );
    return NULL;
}

void HIL_PY_ADAPTER_Free( void* allocation )
{
    size_t index;

    for ( index = 0u; index < HIL_PY_TEST_MAX_LIVE_ALLOCATIONS; ++index )
    {
        if ( live_allocations[index] == allocation )
        {
            free( allocation );
            live_allocations[index] = NULL;
            ++free_count;
            return;
        }
    }
    ++invalid_free_count;
}

size_t HIL_PY_TEST_ALLOCATOR_Allocation_Count( void )
{
    return allocation_count;
}

size_t HIL_PY_TEST_ALLOCATOR_Free_Count( void )
{
    return free_count;
}

size_t HIL_PY_TEST_ALLOCATOR_Live_Count( void )
{
    size_t index;
    size_t count = 0u;

    for ( index = 0u; index < HIL_PY_TEST_MAX_LIVE_ALLOCATIONS; ++index )
    {
        if ( live_allocations[index] != NULL )
        {
            ++count;
        }
    }
    return count;
}

size_t HIL_PY_TEST_ALLOCATOR_Invalid_Free_Count( void )
{
    return invalid_free_count;
}
