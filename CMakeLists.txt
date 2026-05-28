cmake_minimum_required(VERSION 3.16)

project(hil_rig_protocol VERSION 0.1.0 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(HIL_RIG_PROTOCOL_IS_TOP_LEVEL ON)
else()
    set(HIL_RIG_PROTOCOL_IS_TOP_LEVEL OFF)
endif()

option(HIL_RIG_PROTOCOL_BUILD_TESTS
    "Build HIL-RIG protocol C/C++ tests"
    ${HIL_RIG_PROTOCOL_IS_TOP_LEVEL}
)
option(HIL_RIG_PROTOCOL_BUILD_EXAMPLES
    "Build HIL-RIG protocol examples"
    OFF
)
option(HIL_RIG_PROTOCOL_ENABLE_WARNINGS
    "Enable warnings for HIL-RIG protocol targets"
    ON
)
option(HIL_RIG_PROTOCOL_WARNINGS_AS_ERRORS
    "Treat warnings as errors for HIL-RIG protocol targets"
    OFF
)
option(HIL_RIG_PROTOCOL_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer for host builds"
    OFF
)
option(HIL_RIG_PROTOCOL_ENABLE_COVERAGE
    "Enable coverage instrumentation for host GCC/Clang builds"
    OFF
)

set(HIL_RIG_PROTOCOL_SOURCES
    src/version.c
    src/transport/transport_crc.c
    src/transport/transport_frame.c
    src/transport/transport_parser.c
    src/transport/transport_session.c
)

add_library(hil_rig_protocol ${HIL_RIG_PROTOCOL_SOURCES})
add_library(hil_rig_protocol::hil_rig_protocol ALIAS hil_rig_protocol)

target_include_directories(hil_rig_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(hil_rig_protocol PUBLIC c_std_11)

function(hil_rig_protocol_apply_warnings target_name)
    if(NOT HIL_RIG_PROTOCOL_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(HIL_RIG_PROTOCOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wsign-conversion
        )
        if(HIL_RIG_PROTOCOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(hil_rig_protocol_apply_sanitizers target_name)
    if(NOT HIL_RIG_PROTOCOL_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CROSSCOMPILING)
        message(STATUS "Skipping HIL-RIG protocol sanitizers while cross-compiling")
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target_name}
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(${target_name}
            PRIVATE
                -fsanitize=address,undefined
        )
    else()
        message(STATUS "HIL-RIG protocol sanitizers are only configured for GCC/Clang")
    endif()
endfunction()

function(hil_rig_protocol_apply_coverage target_name)
    if(NOT HIL_RIG_PROTOCOL_ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CROSSCOMPILING)
        message(STATUS "Skipping HIL-RIG protocol coverage while cross-compiling")
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target_name}
            PRIVATE
                -O0
                -g
                --coverage
        )
        target_link_options(${target_name}
            PRIVATE
                --coverage
        )
    else()
        message(STATUS "HIL-RIG protocol coverage is only configured for GCC/Clang")
    endif()
endfunction()

hil_rig_protocol_apply_warnings(hil_rig_protocol)
hil_rig_protocol_apply_sanitizers(hil_rig_protocol)
hil_rig_protocol_apply_coverage(hil_rig_protocol)

if(HIL_RIG_PROTOCOL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/c)
endif()

if(HIL_RIG_PROTOCOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
