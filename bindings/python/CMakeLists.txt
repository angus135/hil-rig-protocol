add_library(hil_rig_protocol_python_adapter STATIC
    hil_rig_protocol_ffi.c
    hil_rig_protocol_ffi_allocator.c
)

target_compile_features(hil_rig_protocol_python_adapter PRIVATE c_std_11)
target_link_libraries(hil_rig_protocol_python_adapter
    PUBLIC
        hil_rig_protocol::hil_rig_protocol
)
target_include_directories(hil_rig_protocol_python_adapter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_python_adapter)
endif()

if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_python_adapter)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_python_adapter)
endif()

# The Python extension links these static targets into a shared module. Keep PIC
# scoped to the Python-enabled build so normal firmware consumers are unchanged.
set_target_properties(hil_rig_protocol hil_rig_protocol_python_adapter
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

get_target_property(HIL_RIG_PROTOCOL_CORE_LIBRARY_TYPE hil_rig_protocol TYPE)
if(NOT HIL_RIG_PROTOCOL_CORE_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR
        "HIL_RIG_PROTOCOL_BUILD_PYTHON requires the hil_rig_protocol core target to be static"
    )
endif()

if(CMAKE_VERSION VERSION_LESS 3.17)
    message(FATAL_ERROR
        "HIL_RIG_PROTOCOL_BUILD_PYTHON requires CMake 3.17 or newer"
    )
endif()

if(CMAKE_VERSION VERSION_LESS 3.18)
    find_package(Python REQUIRED COMPONENTS Interpreter Development)
else()
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
endif()

execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import cffi"
    RESULT_VARIABLE HIL_RIG_PROTOCOL_CFFI_IMPORT_RESULT
    ERROR_VARIABLE HIL_RIG_PROTOCOL_CFFI_IMPORT_ERROR
)
if(NOT HIL_RIG_PROTOCOL_CFFI_IMPORT_RESULT EQUAL 0)
    message(FATAL_ERROR
        "CFFI is required when HIL_RIG_PROTOCOL_BUILD_PYTHON=ON. "
        "Python interpreter: ${Python_EXECUTABLE}\n"
        "${HIL_RIG_PROTOCOL_CFFI_IMPORT_ERROR}"
    )
endif()

set(HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/hil_rig_protocol_native.c"
)
set(HIL_RIG_PROTOCOL_CFFI_DEPENDENCIES
    "${CMAKE_CURRENT_SOURCE_DIR}/build_ffi.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdef.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/hil_rig_protocol_ffi.h"
    "${PROJECT_SOURCE_DIR}/include/hil_rig_protocol/transport/transport.h"
    "${PROJECT_SOURCE_DIR}/include/hil_rig_protocol/transport/transport_types.h"
)

add_custom_command(
    OUTPUT "${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
    COMMAND
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/build_ffi.py"
        --output "${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
    DEPENDS ${HIL_RIG_PROTOCOL_CFFI_DEPENDENCIES}
    COMMENT "Generating HIL-RIG protocol CFFI API-mode source"
    VERBATIM
)

set_source_files_properties("${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
    PROPERTIES GENERATED TRUE
)

# CFFI owns this generated source. Keep any warning relaxation local to that
# file; the core and handwritten adapter retain the repository warning policy.
if(MSVC)
    set_property(SOURCE "${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
        APPEND PROPERTY COMPILE_OPTIONS /W0
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set_property(SOURCE "${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
        APPEND PROPERTY COMPILE_OPTIONS -w
    )
endif()

Python_add_library(hil_rig_protocol_python_native MODULE WITH_SOABI
    "${HIL_RIG_PROTOCOL_CFFI_GENERATED_SOURCE}"
)
set_target_properties(hil_rig_protocol_python_native
    PROPERTIES OUTPUT_NAME "_native"
)
target_compile_features(hil_rig_protocol_python_native PRIVATE c_std_11)
target_include_directories(hil_rig_protocol_python_native
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# The generated module is tagged for the active CPython ABI rather than abi3.
# CFFI otherwise enables Py_LIMITED_API on MSVC, which adds an implicit
# python3.lib dependency alongside FindPython's correct versioned import
# library (for example, python312.lib).
if(WIN32)
    target_compile_definitions(hil_rig_protocol_python_native
        PRIVATE
            _CFFI_NO_LIMITED_API
    )
endif()

target_link_libraries(hil_rig_protocol_python_native
    PRIVATE
        hil_rig_protocol_python_adapter
        hil_rig_protocol::hil_rig_protocol
        Python::Module
)

# Static dependencies carry instrumented object code into this shared module.
# Apply the matching final-link options so their runtime symbols are resolved.
if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_python_native)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_python_native)
endif()

install(TARGETS hil_rig_protocol_python_native
    LIBRARY DESTINATION hil_rig_protocol
    RUNTIME DESTINATION hil_rig_protocol
)

if(HIL_RIG_PROTOCOL_BUILD_TESTS)
    add_executable(hil_rig_protocol_python_adapter_tests
        tests/test_hil_rig_protocol_ffi.c
        tests/test_hil_rig_protocol_ffi_allocator.c
        hil_rig_protocol_ffi.c
    )
    target_compile_features(hil_rig_protocol_python_adapter_tests PRIVATE c_std_11)
    target_include_directories(hil_rig_protocol_python_adapter_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(hil_rig_protocol_python_adapter_tests
        PRIVATE
            hil_rig_protocol::hil_rig_protocol
    )

    if(COMMAND hil_rig_protocol_apply_warnings)
        hil_rig_protocol_apply_warnings(hil_rig_protocol_python_adapter_tests)
    endif()

    if(COMMAND hil_rig_protocol_apply_sanitizers)
        hil_rig_protocol_apply_sanitizers(hil_rig_protocol_python_adapter_tests)
    endif()

    if(COMMAND hil_rig_protocol_apply_coverage)
        hil_rig_protocol_apply_coverage(hil_rig_protocol_python_adapter_tests)
    endif()

    add_test(
        NAME hil_rig_protocol_python_adapter_tests
        COMMAND hil_rig_protocol_python_adapter_tests
    )
    set_tests_properties(hil_rig_protocol_python_adapter_tests
        PROPERTIES LABELS "python-adapter"
    )
endif()
