cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HIL_RIG_PROTOCOL_VERSION)
string(STRIP "${HIL_RIG_PROTOCOL_VERSION}" HIL_RIG_PROTOCOL_VERSION)
if(NOT HIL_RIG_PROTOCOL_VERSION MATCHES
    "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"
)
    message(FATAL_ERROR
        "VERSION must contain exactly one numeric MAJOR.MINOR.PATCH value; "
        "found '${HIL_RIG_PROTOCOL_VERSION}'"
    )
endif()

project(hil_rig_protocol VERSION "${HIL_RIG_PROTOCOL_VERSION}" LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(HIL_RIG_PROTOCOL_IS_TOP_LEVEL ON)
else()
    set(HIL_RIG_PROTOCOL_IS_TOP_LEVEL OFF)
endif()

option(HIL_RIG_PROTOCOL_BUILD_TESTS
    "Build HIL-RIG protocol C/C++ tests"
    ${HIL_RIG_PROTOCOL_IS_TOP_LEVEL}
)
option(HIL_RIG_PROTOCOL_BUILD_PYTHON
    "Build HIL-RIG protocol Python binding support"
    OFF
)
option(HIL_RIG_PROTOCOL_BUILD_EXAMPLES
    "Build HIL-RIG protocol examples"
    ${HIL_RIG_PROTOCOL_IS_TOP_LEVEL}
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

include(src/transport/transport_profiles.cmake)

set(HIL_RIG_PROTOCOL_SOURCES
    src/version.c
    src/transport/transport.c
    ${HIL_RIG_PROTOCOL_TRANSPORT_COMMON_SOURCES}
    ${HIL_RIG_PROTOCOL_TRANSPORT_PROFILE_SOURCES}
)

add_library(hil_rig_protocol ${HIL_RIG_PROTOCOL_SOURCES})
add_library(hil_rig_protocol::hil_rig_protocol ALIAS hil_rig_protocol)

add_subdirectory(src/application)

set(HIL_RIG_PROTOCOL_PUBLIC_VERSION_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hil_rig_protocol/version.h"
)
file(READ "${HIL_RIG_PROTOCOL_PUBLIC_VERSION_HEADER}" HIL_RIG_PROTOCOL_VERSION_HEADER_TEXT)
foreach(component MAJOR MINOR PATCH)
    if(NOT HIL_RIG_PROTOCOL_VERSION_HEADER_TEXT MATCHES
        "#define[ \t]+HIL_RIG_PROTOCOL_VERSION_${component}[ \t]+${PROJECT_VERSION_${component}}u"
    )
        message(FATAL_ERROR
            "include/hil_rig_protocol/version.h does not match VERSION for ${component}"
        )
    endif()
endforeach()
if(NOT HIL_RIG_PROTOCOL_VERSION_HEADER_TEXT MATCHES
    "#define[ \t]+HIL_RIG_PROTOCOL_VERSION_STRING[ \t]+\"${PROJECT_VERSION}\""
)
    message(FATAL_ERROR "include/hil_rig_protocol/version.h does not match VERSION string")
endif()
unset(HIL_RIG_PROTOCOL_VERSION_HEADER_TEXT)

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

# Python wheels install only the private extension. Normal C consumers may
# install the static/shared library and checked-in public headers directly.
if(HIL_RIG_PROTOCOL_IS_TOP_LEVEL AND NOT HIL_RIG_PROTOCOL_BUILD_PYTHON)
    include(GNUInstallDirs)
    install(TARGETS hil_rig_protocol
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/hil_rig_protocol"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(HIL_RIG_PROTOCOL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/c)
endif()

if(HIL_RIG_PROTOCOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HIL_RIG_PROTOCOL_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()
