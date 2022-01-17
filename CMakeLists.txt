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

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(PANDA NONE)

# Add our custom configuration types to
# multi-configuration generators (i.e. Visual Studio):
if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES "FastVerify" "DebugDetailed")
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES}
        CACHE STRING "CMake configuration types" FORCE)
endif()

enable_language(CXX ASM)

# NB! For God's sake do not touch the command below.
# See https://gitlab.kitware.com/cmake/cmake/issues/16588.
# and https://clang.llvm.org/docs/JSONCompilationDatabase.html
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----- Default flags ----------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----- Global variables -------------------------------------------------------
# Please don't use CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR to allow building
# Panda as a cmake subdirectory. You can use the following variables if you
# need to refer the Panda root directories
set(PANDA_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PANDA_BINARY_ROOT ${CMAKE_CURRENT_BINARY_DIR})
set(PANDA_THIRD_PARTY_SOURCES_DIR ${PANDA_ROOT}/ark-third-party)
set(PANDA_THIRD_PARTY_CONFIG_DIR ${PANDA_ROOT}/cmake/ark-third-party)

# ----- Platform definitions ---------------------------------------------------
include(cmake/Definitions.cmake)

if (NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT PANDA_TARGET_WINDOWS)
    # Needed for stacktrace printing
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Werror -Wshadow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
if(PANDA_TARGET_MACOS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.13")
endif()

set(PANDA_PGO_PROFGEN_PATH "/data/local/tmp")

if (PANDA_TARGET_MOBILE AND (PANDA_TARGET_ARM64 OR PANDA_TARGET_ARM32))
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()

if (PANDA_PGO_INSTRUMENT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate=${PANDA_PGO_PROFGEN_PATH}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-generate=${PANDA_PGO_PROFGEN_PATH}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate=${PANDA_PGO_PROFGEN_PATH}")
endif()

if (PANDA_PGO_OPTIMIZE)
    if (NOT PANDA_PGO_PROFILE_DATA)
        message(FATAL_ERROR "PANDA_PGO_PROFILE_DATA is not set")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-use=${PANDA_PGO_PROFILE_DATA}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-use=${PANDA_PGO_PROFILE_DATA}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use=${PANDA_PGO_PROFILE_DATA}")
endif()

if (PANDA_ENABLE_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto=thin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin")
endif()

if (PANDA_LLVM_REGALLOC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -regalloc=${PANDA_LLVM_REGALLOC}")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    add_compile_options(-O2 -ggdb3 -fno-omit-frame-pointer)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "FastVerify")
    add_compile_options(-O2 -ggdb3)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "DebugDetailed")
    add_compile_options(-Og -ggdb3)
endif()

if (PANDA_THREAD_SAFETY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wthread-safety")
endif()

if (PANDA_ARK_JS_VM)
    add_definitions(-DPANDA_ARK_JS_VM)
endif()

# ----- Deliverable executables and libraries ----------------------------------
# Please override with per-target properties if your artifact should reside
# elsewhere, like this:
# set_target_properties(... PROPERTIES RUNTIME_OUTPUT_DIRECTORY ...)
# Applicable for tests and all "internal" artifacts.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PANDA_BINARY_ROOT}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PANDA_BINARY_ROOT}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PANDA_BINARY_ROOT}/bin)

# ----- Panda CMake functions --------------------------------------------------
include(cmake/PandaCmakeFunctions.cmake)

# ----- Bootstrapping (for parts of platform written in managed code ) ---------
include(cmake/HostTools.cmake)

# ----- Enable CCache ----------------------------------------------------------
include(cmake/PandaCCache.cmake)

# ----- Code analysis and style ------------------------------------------------
include(cmake/ClangTidy.cmake)
include(cmake/CodeStyle.cmake)

# ----- Sanitizers testing -----------------------------------------------------
include(cmake/Sanitizers.cmake)

# ----- Enable testing ---------------------------------------------------------

# Umbrella target for testing:
add_custom_target(tests COMMENT "Running all test suites")

include(cmake/Testing.cmake)

# ----- Template Based Generator -----------------------------------------------
include(cmake/TemplateBasedGen.cmake)

# ----- Enable panda assemblies building ---------------------------------------
include(cmake/PandaAssembly.cmake)

# Some compilers use x87 fp instructions by default in 32-bit mode.
# We need to use SSE one to correspond x86_64 build.
if (PANDA_TARGET_X86)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -msse3")
endif()

# ----- Targets ----------------------------------------------------------------

execute_process(COMMAND ${PANDA_ROOT}/scripts/install-third-party
                WORKING_DIRECTORY ${PANDA_ROOT}
                RESULT_VARIABLE THIRD_PARTY_OK)
if (NOT THIRD_PARTY_OK EQUAL 0)
    message(FATAL_ERROR "Unable to install required third-party dependencies")
endif()

if(PANDA_WITH_TOOLCHAIN)
    add_subdirectory(isa)

    set(SECUREC_ROOT ${PANDA_THIRD_PARTY_SOURCES_DIR}/securec)
    add_subdirectory(${PANDA_THIRD_PARTY_CONFIG_DIR}/securec)
    add_subdirectory(libpandabase)

    set(MINIZ_ROOT ${PANDA_THIRD_PARTY_SOURCES_DIR}/miniz/amalgamation)
    add_subdirectory(${PANDA_THIRD_PARTY_CONFIG_DIR}/miniz)
    add_subdirectory(libziparchive)

    add_subdirectory(libpandafile)
    if(NOT PANDA_TARGET_WINDOWS)
        add_subdirectory(libpandafile/external)
    endif()

    add_subdirectory(assembler)
    add_subdirectory(disassembler)

    if(PANDA_WITH_RUNTIME)
        if (PANDA_TARGET_X86 OR PANDA_TARGET_AMD64)
            add_subdirectory(verification/tests)
        endif()
        add_subdirectory(verification/gen)
        add_subdirectory(verification)
    endif()
endif()

if(PANDA_WITH_RUNTIME)
    add_subdirectory(pandastdlib)

    if(NOT PANDA_TARGET_WINDOWS)
        add_subdirectory(dprof)
    endif()

    add_subdirectory(runtime)

    add_subdirectory(panda)

    add_subdirectory(verification/verifier)

endif()

# ----- Testing ----------------------------------------------------------------

if(PANDA_WITH_TESTS)
    add_subdirectory(${PANDA_THIRD_PARTY_SOURCES_DIR}/googletest)
    target_compile_options(gtest PUBLIC "-Wno-shadow" "-Wno-pedantic")

    add_subdirectory(tests)

    add_custom_target(tests_full COMMENT "Running all test suites and code checks")
    add_dependencies(tests_full
        check_concurrency_format
        tests
    )
    if(NOT PANDA_TARGET_MACOS)
        # add_dependencies(tests_full clang_format)
    endif()
endif()

# ----- Benchmarking -----------------------------------------------------------

if(PANDA_WITH_BENCHMARKS)
    # NB! Please do not merge benchmarks and tests unless you want to mess with
    # slow builds, etc. If you want some coupling, you might want to make benchmarks
    # depend on tests some day.

    add_custom_target(benchmarks COMMENT "Running all benchmark suites")
    add_subdirectory(tests/benchmarks)
endif()

# ----- Aliases ----------------------------------------------------------------

add_custom_target(panda_bins COMMENT "Build all common Panda binaries")
add_dependencies(panda_bins panda pandasm)
