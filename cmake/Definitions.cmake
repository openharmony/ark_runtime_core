# Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(PANDA_TARGET_LINUX 1)
    set(PANDA_TARGET_UNIX 1)
if (NOT PANDA_ENABLE_ADDRESS_SANITIZER)
    set(PANDA_USE_FUTEX 1)
endif()
    add_definitions(-DPANDA_TARGET_LINUX)
    add_definitions(-DPANDA_TARGET_UNIX)
if (NOT PANDA_ENABLE_ADDRESS_SANITIZER)
    add_definitions(-DPANDA_USE_FUTEX)
endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL OHOS)
    set(PANDA_TARGET_OHOS 1)
    set(PANDA_TARGET_UNIX 1)
    add_definitions(-DPANDA_TARGET_OHOS)
    add_definitions(-DPANDA_TARGET_UNIX)
elseif(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set(PANDA_TARGET_WINDOWS 1)
    add_definitions(-DPANDA_TARGET_WINDOWS)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(PANDA_TARGET_MACOS 1)
    set(PANDA_TARGET_UNIX 1)
    add_definitions(-DPANDA_TARGET_MACOS)
    add_definitions(-DPANDA_TARGET_UNIX)
else()
    message(FATAL_ERROR "Platform ${CMAKE_SYSTEM_NAME} is not supported")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
    if(NOT PANDA_CROSS_AMD64_X86)
        set(PANDA_TARGET_AMD64 1)
        add_definitions(-DPANDA_TARGET_AMD64)
    else()
        set(PANDA_TARGET_X86 1)
        add_definitions(-DPANDA_TARGET_X86)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[356]86")
    set(PANDA_TARGET_X86 1)
    add_definitions(-DPANDA_TARGET_X86)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(PANDA_TARGET_ARM64 1)
    add_definitions(-DPANDA_TARGET_ARM64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(PANDA_TARGET_ARM32 1)
    add_definitions(-DPANDA_TARGET_ARM32)
    if(PANDA_TARGET_ARM32_ABI_SOFT)
        add_definitions(-DPANDA_TARGET_ARM32_ABI_SOFT)
    elseif(PANDA_TARGET_ARM32_ABI_HARD)
        add_definitions(-DPANDA_TARGET_ARM32_ABI_HARD)
    else()
        message(FATAL_ERROR "PANDA_TARGET_ARM32_ABI_* is not set")
    endif()
else()
    message(FATAL_ERROR "Processor ${CMAKE_SYSTEM_PROCESSOR} is not supported")
endif()

if(PANDA_TARGET_AMD64 OR PANDA_TARGET_ARM64)
    set(PANDA_TARGET_64 1)
    add_definitions(-DPANDA_TARGET_64)
elseif(PANDA_TARGET_X86 OR PANDA_TARGET_ARM32)
    set(PANDA_TARGET_32 1)
    add_definitions(-DPANDA_TARGET_32)
else()
    message(FATAL_ERROR "Unknown bitness of the target platform")
endif()

if (PANDA_TRACK_INTERNAL_ALLOCATIONS)
    message(STATUS "Track internal allocations")
    add_definitions(-DTRACK_INTERNAL_ALLOCATIONS=${PANDA_TRACK_INTERNAL_ALLOCATIONS})
endif()

# Enable global register variables usage only for clang >= 9.0.0 and gcc >= 8.0.0.
# Clang 8.0.0 doesn't support all necessary options -ffixed-<reg>. Gcc 7.5.0 freezes
# when compiling release interpreter.
#
# Also calling conventions of functions that use global register variables are different:
# clang stores and restores registers that are used for global variables in the prolog
# and epilog of such functions and gcc doesn't do it. So it's necessary to inline all
# function that refers to global register variables to interpreter loop.

# For this reason we disable global register variables usage for clang debug builds as
# ALWAYS_INLINE macro expands to nothing in this mode and we cannot guarantee that all
# necessary function will be inlined.
#
if(PANDA_TARGET_ARM64 AND ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
                           AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0.0
                           AND CMAKE_BUILD_TYPE MATCHES Release)
                           OR
                           (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
                           AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0.0)))
    set(PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES 1)
    add_definitions(-DPANDA_ENABLE_GLOBAL_REGISTER_VARIABLES)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    # Additional debug information about fp in each frame
    add_compile_options(-fno-omit-frame-pointer)
endif()

if (PANDA_PGO_INSTRUMENT OR PANDA_PGO_OPTIMIZE)
    if (NOT PANDA_TARGET_MOBILE OR NOT PANDA_TARGET_ARM64)
        message(FATAL_ERROR "PGO supported is not supported on this target")
    endif()

    set(PANDA_ENABLE_LTO true)
endif()

if(PANDA_TARGET_64)
    set(PANDA_USE_32_BIT_POINTER 1)
    add_definitions(-DPANDA_USE_32_BIT_POINTER)
endif()

if(PANDA_TARGET_LINUX)
    execute_process(COMMAND grep PRETTY_NAME= /etc/os-release
                    OUTPUT_VARIABLE PANDA_TARGET_LINUX_DISTRO
                    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(PANDA_TARGET_LINUX_DISTRO MATCHES "Ubuntu")
        set(PANDA_TARGET_LINUX_UBUNTU 1)
        add_definitions(-DPANDA_TARGET_LINUX_UBUNTU)
    endif()

    if(PANDA_TARGET_LINUX_DISTRO MATCHES "Ubuntu 18\\.04")
        set(PANDA_TARGET_LINUX_UBUNTU_18_04 1)
        add_definitions(-DPANDA_TARGET_LINUX_UBUNTU_18_04)
    elseif(PANDA_TARGET_LINUX_DISTRO MATCHES "Ubuntu 20\\.04")
        set(PANDA_TARGET_LINUX_UBUNTU_20_04 1)
        add_definitions(-DPANDA_TARGET_LINUX_UBUNTU_20_04)
    endif()
endif()

set(PANDA_WITH_RUNTIME    true)
set(PANDA_WITH_TOOLCHAIN  true)
set(PANDA_WITH_TESTS      true)
set(PANDA_WITH_BENCHMARKS true)
set(PANDA_DEFAULT_LIB_TYPE "SHARED")
set(DONT_USE_RAPIDCHECK true)

option(PANDA_ARK_JS_VM "Build with C interpreter in ecmascript folder" OFF)

if(PANDA_TARGET_WINDOWS)
    set(PANDA_WITH_BENCHMARKS false)
    set(PANDA_DEFAULT_LIB_TYPE "STATIC")
endif()

if(PANDA_TARGET_MACOS)
    set(PANDA_DEFAULT_LIB_TYPE "STATIC")
    #introduced for "std::filesystem::create_directories"
    add_compile_options(-mmacosx-version-min=10.15)
endif()

if(PANDA_TARGET_OHOS)
    set(PANDA_WITH_BENCHMARKS false)
endif()

if(CMAKE_BUILD_TYPE STREQUAL FastVerify)
    add_definitions(-DFAST_VERIFY)
endif()

# The definition is set for the build which will be delivered to customers.
# Currently this build doesn't contain dependencies to debug libraries
# (like libdwarf.so)
if (NOT DEFINED PANDA_PRODUCT_BUILD)
    set(PANDA_PRODUCT_BUILD false)
endif()

if (PANDA_PRODUCT_BUILD)
    add_definitions(-DPANDA_PRODUCT_BUILD)
endif()

message(STATUS "PANDA_TARGET_UNIX                      = ${PANDA_TARGET_UNIX}")
message(STATUS "PANDA_TARGET_LINUX                     = ${PANDA_TARGET_LINUX}")
message(STATUS "PANDA_TARGET_MOBILE                    = ${PANDA_TARGET_MOBILE}")
message(STATUS "PANDA_USE_FUTEX                        = ${PANDA_USE_FUTEX}")
message(STATUS "PANDA_TARGET_WINDOWS                   = ${PANDA_TARGET_WINDOWS}")
message(STATUS "PANDA_TARGET_OHOS                      = ${PANDA_TARGET_OHOS}")
message(STATUS "PANDA_TARGET_MACOS                     = ${PANDA_TARGET_MACOS}")
message(STATUS "PANDA_CROSS_AMD64_X86                  = ${PANDA_CROSS_AMD64_X86}")
message(STATUS "PANDA_TARGET_AMD64                     = ${PANDA_TARGET_AMD64}")
message(STATUS "PANDA_TARGET_X86                       = ${PANDA_TARGET_X86}")
message(STATUS "PANDA_TARGET_ARM64                     = ${PANDA_TARGET_ARM64}")
message(STATUS "PANDA_TARGET_ARM32                     = ${PANDA_TARGET_ARM32}")
if(PANDA_TARGET_ARM32)
message(STATUS "PANDA_TARGET_ARM32_ABI_SOFT            = ${PANDA_TARGET_ARM32_ABI_SOFT}")
message(STATUS "PANDA_TARGET_ARM32_ABI_HARD            = ${PANDA_TARGET_ARM32_ABI_HARD}")
endif()
message(STATUS "PANDA_TARGET_64                        = ${PANDA_TARGET_64}")
message(STATUS "PANDA_TARGET_32                        = ${PANDA_TARGET_32}")
message(STATUS "PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES = ${PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES}")
message(STATUS "PANDA_ENABLE_LTO                       = ${PANDA_ENABLE_LTO}")
if(PANDA_TARGET_MOBILE)
message(STATUS "PANDA_LLVM_REGALLOC                    = ${PANDA_LLVM_REGALLOC}")
endif()
message(STATUS "PANDA_WITH_RUNTIME                     = ${PANDA_WITH_RUNTIME}")
message(STATUS "PANDA_WITH_COMPILER                    = ${PANDA_WITH_COMPILER}")
message(STATUS "PANDA_WITH_TOOLCHAIN                   = ${PANDA_WITH_TOOLCHAIN}")
message(STATUS "PANDA_WITH_TESTS                       = ${PANDA_WITH_TESTS}")
message(STATUS "PANDA_WITH_BENCHMARKS                  = ${PANDA_WITH_BENCHMARKS}")
message(STATUS "PANDA_PGO_INSTRUMENT                   = ${PANDA_PGO_INSTRUMENT}")
message(STATUS "PANDA_PGO_OPTIMIZE                     = ${PANDA_PGO_OPTIMIZE}")
message(STATUS "PANDA_PRODUCT_BUILD                    = ${PANDA_PRODUCT_BUILD}")
