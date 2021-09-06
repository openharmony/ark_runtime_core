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

add_library(asm_defines defines.cpp)

add_dependencies(asm_defines
        intrinsics_gen_arkruntime)

set(ASM_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(ASM_FILE libasm_defines.S)
set(OUTPUT_FILE asm_defines.h)
set(OUTPUT_DIR ${GEN_INCLUDE_DIR})
file(MAKE_DIRECTORY ${OUTPUT_DIR})

set_target_properties(asm_defines
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ASM_DIR}
    SUFFIX .S
)

target_include_directories(asm_defines
    PUBLIC "$<TARGET_PROPERTY:arkruntime,INTERFACE_INCLUDE_DIRECTORIES>"
    PUBLIC "$<TARGET_PROPERTY:arkruntime_test_interpreter_impl,INTERFACE_INCLUDE_DIRECTORIES>"
)

# We have to disable LTO, because it generates llvm IR instead of assembly for '-S' option.
# Without this restriction, we can just use generator expressions and inherit compile flags from arkruntime (that looks
# best way to do that):
# target_compile_options(asm_defines
#     PUBLIC "$<TARGET_PROPERTY:arkruntime,COMPILE_OPTIONS>"
# )
# But it seems that we can't remove options from the generator expressions.
# (-fno-lto doesn't work because compilation fails with inappropriate regalloc)
string(REPLACE "-mllvm -regalloc=${PANDA_LLVM_REGALLOC}" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-flto=thin" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
target_compile_options(asm_defines PUBLIC "-Wno-invalid-offsetof")
target_compile_options(asm_defines PUBLIC "-S")

add_custom_command(OUTPUT ${OUTPUT_DIR}/${OUTPUT_FILE}
    COMMAND ruby ${CMAKE_CURRENT_SOURCE_DIR}/defines_generator.rb "${ASM_DIR}/${ASM_FILE}" ${OUTPUT_DIR}/${OUTPUT_FILE}
    DEPENDS asm_defines ${ASM_DIR}/${ASM_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/defines_generator.rb
)

add_custom_target(asm_defines_generator
    DEPENDS ${OUTPUT_DIR}/${OUTPUT_FILE}
)
