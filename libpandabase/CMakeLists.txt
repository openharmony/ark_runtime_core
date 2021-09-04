# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(arkbase)

include(CheckFunctionExists)
include(cmake/mm_coverage.cmake)

set(SOURCES
    ${PANDA_ROOT}/libpandabase/mem/alloc_tracker.cpp
    ${PANDA_ROOT}/libpandabase/mem/arena.cpp
    ${PANDA_ROOT}/libpandabase/mem/arena_allocator.cpp
    ${PANDA_ROOT}/libpandabase/mem/code_allocator.cpp
    ${PANDA_ROOT}/libpandabase/mem/pool_manager.cpp
    ${PANDA_ROOT}/libpandabase/mem/pool_map.cpp
    ${PANDA_ROOT}/libpandabase/mem/mem_config.cpp
    ${PANDA_ROOT}/libpandabase/mem/base_mem_stats.cpp
    ${PANDA_ROOT}/libpandabase/utils/debug.cpp
    ${PANDA_ROOT}/libpandabase/utils/logger.cpp
    ${PANDA_ROOT}/libpandabase/utils/dfx.cpp
    ${PANDA_ROOT}/libpandabase/utils/utf.cpp
    ${PANDA_ROOT}/libpandabase/utils/time.cpp
    ${PANDA_ROOT}/libpandabase/utils/terminate.cpp
    ${PANDA_ROOT}/libpandabase/utils/type_converter.cpp
    ${PANDA_ROOT}/libpandabase/utils/json_parser.cpp
    ${PANDA_ROOT}/libpandabase/trace/trace.cpp
    ${PANDA_ROOT}/libpandabase/os/filesystem.cpp
    ${PANDA_ROOT}/libpandabase/os/native_stack.cpp
    ${PANDA_ROOT}/libpandabase/os/property.cpp
    ${PANDA_ROOT}/libpandabase/os/dfx_option.cpp
    ${PANDA_ROOT}/libpandabase/os/time.cpp
)

set(UNIX_SOURCES_COMMON
    ${PANDA_ROOT}/libpandabase/os/unix/error.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/thread.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/native_stack.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/property.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/time_unix.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/file.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/filesystem.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/library_loader.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/mem.cpp
    ${PANDA_ROOT}/libpandabase/trace/unix/trace.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/sighooklib/sighook.cpp
)
# Handle pthread and futex mutexes
if(PANDA_USE_FUTEX)
    set(UNIX_SOURCES_COMMON ${UNIX_SOURCES_COMMON}
        ${PANDA_ROOT}/libpandabase/os/unix/futex/mutex.cpp
    )
else()
    set(UNIX_SOURCES_COMMON ${UNIX_SOURCES_COMMON}
        ${PANDA_ROOT}/libpandabase/os/unix/mutex.cpp
    )
endif()

set(WINDOWS_SOURCES
    ${PANDA_ROOT}/libpandabase/utils/logger.cpp
    ${PANDA_ROOT}/libpandabase/utils/debug.cpp
    ${PANDA_ROOT}/libpandabase/utils/dfx.cpp
    ${PANDA_ROOT}/libpandabase/utils/utf.cpp
    ${PANDA_ROOT}/libpandabase/utils/json_parser.cpp
    ${PANDA_ROOT}/libpandabase/os/windows/error.cpp
    ${PANDA_ROOT}/libpandabase/os/windows/file.cpp
    ${PANDA_ROOT}/libpandabase/os/windows/mem.cpp
    ${PANDA_ROOT}/libpandabase/os/windows/thread.cpp
    ${PANDA_ROOT}/libpandabase/trace/windows/trace.cpp
    ${PANDA_ROOT}/libpandabase/os/filesystem.cpp
    ${PANDA_ROOT}/libpandabase/os/unix/mutex.cpp
    ${PANDA_ROOT}/libpandabase/mem/arena.cpp
    ${PANDA_ROOT}/libpandabase/mem/arena_allocator.cpp
    ${PANDA_ROOT}/libpandabase/mem/pool_manager.cpp
    ${PANDA_ROOT}/libpandabase/mem/pool_map.cpp
    ${PANDA_ROOT}/libpandabase/mem/base_mem_stats.cpp
    ${PANDA_ROOT}/libpandabase/mem/mem_config.cpp
)

set(INCLUDE_DIRECTORIES)
set(LINK_LIBRARIES)

# Native stacktraces
if (PANDA_TARGET_WINDOWS)
    # We are not interesting in native stacktraces on windows
    list(APPEND WINDOWS_SOURCES
        ${PANDA_ROOT}/libpandabase/os/stacktrace_stub.cpp
    )
elseif (PANDA_TARGET_OHOS)
    # Not yet supported
    list(APPEND SOURCES
        ${PANDA_ROOT}/libpandabase/os/stacktrace_stub.cpp
    )
elseif (PANDA_PRODUCT_BUILD)
    # Don't add libdwarf dependency for product build
    list(APPEND SOURCES
        ${PANDA_ROOT}/libpandabase/os/stacktrace_stub.cpp
    )
else()
    # Add common sources for native stacktraces
    list(APPEND UNIX_SOURCES_COMMON
        ${PANDA_ROOT}/libpandabase/os/stacktrace.cpp
        ${PANDA_ROOT}/libpandabase/os/debug_info.cpp
    )
    # Handle backtrace function
    if (PANDA_TARGET_MOBILE)
        # Not yet supported
        list(APPEND SOURCES
            ${PANDA_ROOT}/libpandabase/os/stacktrace_stub.cpp
        )
    else()
        list(APPEND LINK_LIBRARIES dwarf z)
    endif()
endif()

if(PANDA_TARGET_UNIX)
    set(SOURCES ${SOURCES} ${UNIX_SOURCES_COMMON})
    if(NOT PANDA_TARGET_MACOS)
        set(SOURCES ${SOURCES}
            ${PANDA_ROOT}/libpandabase/os/unix/exec.cpp
            ${PANDA_ROOT}/libpandabase/os/unix/pipe.cpp)
    endif()
elseif (PANDA_TARGET_WINDOWS)
    set(SOURCES ${WINDOWS_SOURCES})
else()
    message(FATAL_ERROR "Platform ${CMAKE_SYSTEM_NAME} is not supported")
endif ()

add_library(arkbase ${PANDA_DEFAULT_LIB_TYPE} ${SOURCES})

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(EVENTS_GEN_H ${GENERATED_DIR}/events_gen.h)
panda_gen_file(
        DATAFILE ${PANDA_ROOT}/libpandabase/events/events.yaml
        TEMPLATE ${PANDA_ROOT}/libpandabase/events/events_gen.h.erb
        OUTPUTFILE ${EVENTS_GEN_H}
        REQUIRES ${PANDA_ROOT}/libpandabase/events/events.rb
)
add_custom_target(events_gen DEPENDS ${EVENTS_GEN_H})
add_dependencies(arkbase events_gen)

panda_add_to_clang_tidy(TARGET arkbase)

if (NOT (PANDA_TARGET_WINDOWS OR PANDA_TARGET_MOBILE OR PANDA_TARGET_MACOS OR PANDA_TARGET_OHOS))
    target_link_libraries(arkbase stdc++fs)
endif()

if(NOT PANDA_TARGET_WINDOWS)
    list(APPEND LINK_LIBRARIES dl)
endif()

if(PANDA_NEED_LIBATOMIC)
    list(APPEND LINK_LIBRARIES atomic)
endif()

if(NOT (PANDA_TARGET_MOBILE OR PANDA_TARGET_OHOS))
    list(APPEND LINK_LIBRARIES pthread)
endif()

list(APPEND LINK_LIBRARIES c_secshared)

target_link_libraries(arkbase ${LINK_LIBRARIES})

check_function_exists(gettid HAVE_GETTID)

if (HAVE_GETTID)
    add_definitions(-DHAVE_GETTID)
endif (HAVE_GETTID)

target_include_directories(arkbase
    INTERFACE .
    PUBLIC ${PANDA_ROOT}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${GENERATED_DIR}
    PUBLIC ${INCLUDE_DIRECTORIES}
)

panda_add_gtest(
    NO_CORES
    NAME arkbase_tests
    SOURCES
        tests/list_test.cpp
        tests/bit_helpers_test.cpp
        tests/bit_memory_region_test.cpp
        tests/bit_table_test.cpp
        tests/bit_utils_test.cpp
        tests/bit_vector_test.cpp
        tests/string_helpers_test.cpp
        tests/type_converter_tests.cpp
        tests/logger_test.cpp
        tests/dfx_test.cpp
        tests/leb128_test.cpp
        tests/utf_test.cpp
        tests/arena_test.cpp
        tests/arena_allocator_test.cpp
        tests/expected_test.cpp
        tests/code_allocator_test.cpp
        tests/small_vector_test.cpp
        tests/span_test.cpp
        tests/pandargs_test.cpp
        tests/pool_map_test.cpp
        tests/hash_test.cpp
        tests/math_helpers_test.cpp
        tests/serializer_test.cpp
        tests/base_mem_stats_test.cpp
        tests/unique_fd_test.cpp
        tests/mem_range_test.cpp
        tests/mmap_fixed_test.cpp
        tests/mmap_mem_pool_test.cpp
        tests/native_bytes_from_mallinfo_test.cpp
        tests/json_parser_test.cpp
        tests/alloc_tracker_test.cpp
    LIBRARIES
        arkbase
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)
if (TARGET arkbase_tests)
    target_compile_options(arkbase_tests PUBLIC "-Wno-unused")
endif()

if(PANDA_TARGET_32)
    # This test is considered passed as soon as it's built,
    # so it's enough to add building it to the umbrella tests target.
    add_executable(memory_literals_test tests/memory_literals_test.cpp)
    target_link_libraries(memory_literals_test arkbase)
    set_target_properties(memory_literals_test
        PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY "${PANDA_BINARY_ROOT}/bin-gtests"
    )
    add_dependencies(tests memory_literals_test)
    add_test(
        NAME memory_literals_test
        COMMAND ${PANDA_RUN_PREFIX} ${PANDA_BINARY_ROOT}/bin-gtests/memory_literals_test
    )
endif()

panda_add_sanitizers(TARGET arkbase SANITIZERS ${PANDA_SANITIZERS_LIST})

if (PANDA_ENABLE_AFL)
    include("${PANDA_ROOT}/fuzzing/Fuzzing.cmake")
    panda_substitute_libs(TARGET arkbase LIBS c_secshared)
endif()

add_check_style("mem")
add_check_style("utils")
add_check_style("os")
add_check_style("tests")
add_check_style("trace")
add_check_style("serializer")
