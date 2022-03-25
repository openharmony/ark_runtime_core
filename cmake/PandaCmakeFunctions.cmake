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

# 32-bits pointers optimization in Panda for objects => addresses usage low 4GB
# We need use linker scripts for section replacement above 4GB
# Note: AddressSanitizer reserves (mmap) addresses for its own needs,
#       so we have difference start addresses for asan and default buildings
function(panda_add_executable target)
    # When using rapidcheck we should use linker scripts with
    # definitions for exception handling sections
    cmake_parse_arguments(ARG "RAPIDCHECK_ON" "" "" ${ARGN})

    add_executable(${target} ${ARG_UNPARSED_ARGUMENTS})

    if(PANDA_USE_32_BIT_POINTER AND NOT (PANDA_TARGET_MOBILE OR PANDA_TARGET_WINDOWS OR PANDA_TARGET_MACOS OR PANDA_TARGET_OHOS))
        if(ARG_RAPIDCHECK_ON)
            set(LINKER_SCRIPT_ARG "-Wl,-T,${PANDA_ROOT}/ldscripts/panda_test_asan.ld")
        else()
            set(LINKER_SCRIPT_ARG "-Wl,-T,${PANDA_ROOT}/ldscripts/panda.ld")
        endif()
        if(PANDA_ENABLE_ADDRESS_SANITIZER)
            # For cross-aarch64 with ASAN with linker script we need use additional path-link
            # Here we use default addresses space (without ASAN flag). It is nuance of cross-building.
            if(PANDA_TARGET_ARM64)
                set(LINKER_SCRIPT_ARG "-Wl,-rpath-link,${PANDA_SYSROOT}/lib ${LINKER_SCRIPT_ARG}")
            else()
                set(LINKER_SCRIPT_ARG "-Wl,--defsym,LSFLAG_ASAN=1 ${LINKER_SCRIPT_ARG}")
            endif()
        endif()
        # We need use specific options for AMD64 building with Clang compiler
        # because it is necessary for placement of sections above 4GB
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND PANDA_TARGET_AMD64)
            set(LINKER_SCRIPT_ARG "${LINKER_SCRIPT_ARG} -pie")
            # -mcmodel=large with clang and asan on x64 leads to bugs in rapidcheck with high-mem mappings
            # so for rapidcheck test binaries panda_test_asan.ld is used which does not require
            # -mcmodel=large option
            if(NOT ARG_RAPIDCHECK_ON)
                target_compile_options(${target} PUBLIC "-mcmodel=large")
            endif()
        endif()
        target_link_libraries(${target} ${LINKER_SCRIPT_ARG})
    endif()
endfunction()

# This function need for non-SHARED libraries
# It is necessary for 32-bits pointers via amd64 building with Clang compiler
function(panda_set_lib_32bit_property target)
    if(PANDA_USE_32_BIT_POINTER AND PANDA_TARGET_AMD64 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
endfunction()
