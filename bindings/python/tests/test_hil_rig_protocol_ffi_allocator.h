/** Test-only controls and observations for the adapter allocator seam. */
#ifndef HIL_RIG_PROTOCOL_BINDINGS_PYTHON_TESTS_TEST_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H
#define HIL_RIG_PROTOCOL_BINDINGS_PYTHON_TESTS_TEST_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H

#include <stddef.h>

void   HIL_PY_TEST_ALLOCATOR_Reset( size_t failing_call );
size_t HIL_PY_TEST_ALLOCATOR_Allocation_Count( void );
size_t HIL_PY_TEST_ALLOCATOR_Free_Count( void );
size_t HIL_PY_TEST_ALLOCATOR_Live_Count( void );
size_t HIL_PY_TEST_ALLOCATOR_Invalid_Free_Count( void );

#endif /* HIL_RIG_PROTOCOL_BINDINGS_PYTHON_TESTS_TEST_HIL_RIG_PROTOCOL_FFI_ALLOCATOR_H */
