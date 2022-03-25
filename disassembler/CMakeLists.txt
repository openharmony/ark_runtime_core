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

cmake_minimum_required(VERSION 3.3.2 FATAL_ERROR)
project(disassembler CXX)

panda_add_executable(ark_disasm disasm.cpp)

panda_isa_gen(
    TEMPLATES
        "opcode_translator.cpp.erb"
        "bc_ins_to_pandasm_ins.cpp.erb"
        "get_ins_info.cpp.erb"
    REQUIRES
        "${PANDA_ROOT}/assembler/asm_isapi.rb"
        "${PANDA_ROOT}/libpandafile/pandafile_isapi.rb"
    EXTRA_DEPENDENCIES
        ${GET_INSTRUCTIONS_CPP}
)

set(GENERATOR ${PANDA_ROOT}/libpandafile/types.rb)
set(TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/templates/type_to_pandasm_type.cpp.erb)
set(DATAFILE ${PANDA_ROOT}/libpandafile/types.yaml)
set(DEPENDENCIES ${GENERATOR} ${TEMPLATE} ${DATAFILE})
set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/type_to_pandasm_type.cpp")
add_custom_command(OUTPUT "${OUTFILE}"
                   COMMENT "Generating type_to_pandasm_type.cpp"
                   COMMAND ${PANDA_ROOT}/isa/gen.rb -d ${DATAFILE} -t ${TEMPLATE} -o "${OUTFILE}" -r ${GENERATOR}
                   DEPENDS ${DEPENDENCIES})
add_custom_target(type_to_pandasm_type_gen_${PROJECT_NAME} ALL DEPENDS "${OUTFILE}")

add_library(arkdisassembler ${PANDA_DEFAULT_LIB_TYPE}
    disassembler.cpp
    opcode_translator.cpp
    bc_ins_to_pandasm_ins.cpp
    get_ins_info.cpp
    type_to_pandasm_type.cpp
)

target_include_directories(arkdisassembler
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(arkdisassembler arkfile arkbase arkassembler)

target_link_libraries(ark_disasm arkdisassembler arkbase arkfile arkassembler)

panda_add_to_clang_tidy(TARGET ark_disasm CHECKS
    "-cert-dcl21-cpp"
    "-cppcoreguidelines-macro-usage"
    "-google-runtime-references"
    "-misc-non-private-member-variables-in-classes"
)

panda_add_sanitizers(TARGET arkdisassembler SANITIZERS ${PANDA_SANITIZERS_LIST})
panda_add_sanitizers(TARGET ark_disasm SANITIZERS ${PANDA_SANITIZERS_LIST})

add_check_style(".")

if(NOT PANDA_WITH_TESTS)
    return()
endif()

panda_add_gtest(
   CONTAINS_MAIN
   NAME disasm_test_labels
   SOURCES
       tests/labels_test.cpp
   LIBRARIES
       arkbase arkassembler arkdisassembler arkfile
   SANITIZERS
       ${PANDA_SANITIZERS_LIST}
)

panda_add_gtest(
    CONTAINS_MAIN
    NAME disasm_test_records
    SOURCES
        tests/records_test.cpp
    LIBRARIES
        arkbase arkassembler arkdisassembler arkfile
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)

panda_add_gtest(
   CONTAINS_MAIN
   NAME disasm_test_instructions
   SOURCES
       tests/instructions_test.cpp
   LIBRARIES
       arkbase arkassembler arkdisassembler arkfile
   SANITIZERS
       ${PANDA_SANITIZERS_LIST}
)

##############################

# disasm_bin directory
set(DISASM_BIN_DIR ${PANDA_BINARY_ROOT}/disassembler/bin)
file(MAKE_DIRECTORY "${DISASM_BIN_DIR}")

# disasm_tests directory
set(DISASM_TESTS_DIR ${PANDA_ROOT}/disassembler/tests/sources)

if(CMAKE_CROSSCOMPILING)
    ExternalProject_Get_Property(panda_host_tools binary_dir)
    set(assembler_target panda_host_tools)
    set(assembler_bin    "${binary_dir}/assembler/ark_asm")
else()
    set(assembler_target ark_asm)
    set(assembler_bin    $<TARGET_FILE:${assembler_target}>)
endif()

function(compile_pre_build)
    set(options)
    set(singleValueArgs TARGET FILE_SRC FILE_DST)
    set(multiValueArgs)

    cmake_parse_arguments(CPB "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT DEFINED CPB_TARGET)
        message(FATAL_ERROR "TARGET argument is missing")
    endif()

    if (NOT DEFINED CPB_FILE_SRC)
        message(FATAL_ERROR "FILE_SRC argument is missing")
    endif()

    if (NOT DEFINED CPB_FILE_DST)
        message(FATAL_ERROR "FILE_DST argument is missing")
    endif()

    add_custom_command(OUTPUT "${CPB_FILE_DST}"
                       COMMENT "Compiling ${CPB_FILE_SRC} ( ${CPB_TARGET} ) in c_p_b function"
                       COMMAND ${assembler_bin} "${CPB_FILE_SRC}" "${CPB_FILE_DST}"
                       DEPENDS ${assembler_target} "${CPB_FILE_SRC}")

    add_custom_target(${CPB_TARGET}
                      COMMENT "reached ${CPB_TARGET}"
                      DEPENDS "${CPB_FILE_DST}")
endfunction()

# label_tests files
compile_pre_build(TARGET disasm_binaries-labels1
                  FILE_SRC ${DISASM_TESTS_DIR}/labels1.pa
                  FILE_DST ${DISASM_BIN_DIR}/labels1.bc)
compile_pre_build(TARGET disasm_binaries-labels2
                  FILE_SRC ${DISASM_TESTS_DIR}/labels2.pa
                  FILE_DST ${DISASM_BIN_DIR}/labels2.bc)
compile_pre_build(TARGET disasm_binaries-exceptions
                  FILE_SRC ${DISASM_TESTS_DIR}/exceptions.pa
                  FILE_DST ${DISASM_BIN_DIR}/exceptions.bc)

add_custom_target(disasm_binaries_test_labels)

add_dependencies(disasm_binaries_test_labels disasm_binaries-labels1)
add_dependencies(disasm_binaries_test_labels disasm_binaries-labels2)
add_dependencies(disasm_binaries_test_labels disasm_binaries-exceptions)

add_dependencies(disasm_test_labels disasm_binaries_test_labels)

# record_tests files
compile_pre_build(TARGET disasm_binaries-record_with_fields
                  FILE_SRC ${DISASM_TESTS_DIR}/record_with_fields.pa
                  FILE_DST ${DISASM_BIN_DIR}/record_with_fields.bc)
compile_pre_build(TARGET disasm_binaries-record_in_record
                  FILE_SRC ${DISASM_TESTS_DIR}/record_in_record.pa
                  FILE_DST ${DISASM_BIN_DIR}/record_in_record.bc)
compile_pre_build(TARGET disasm_binaries-empty_record
                  FILE_SRC ${DISASM_TESTS_DIR}/empty_record.pa
                  FILE_DST ${DISASM_BIN_DIR}/empty_record.bc)
add_custom_target(disasm_binaries_test_records)

add_dependencies(disasm_binaries_test_records disasm_binaries-record_with_fields)
add_dependencies(disasm_binaries_test_records disasm_binaries-record_in_record)
add_dependencies(disasm_binaries_test_records disasm_binaries-empty_record)

add_dependencies(disasm_test_records disasm_binaries_test_records)

# instruction_tests files
compile_pre_build(TARGET disasm_binaries-instructions
                  FILE_SRC ${DISASM_TESTS_DIR}/instructions.pa
                  FILE_DST ${DISASM_BIN_DIR}/instructions.bc)
compile_pre_build(TARGET disasm_binaries-returns
                  FILE_SRC ${DISASM_TESTS_DIR}/returns.pa
                  FILE_DST ${DISASM_BIN_DIR}/returns.bc)
compile_pre_build(TARGET disasm_binaries-calls
                  FILE_SRC ${DISASM_TESTS_DIR}/calls.pa
                  FILE_DST ${DISASM_BIN_DIR}/calls.bc)
compile_pre_build(TARGET disasm_binaries-newarrs
                  FILE_SRC ${DISASM_TESTS_DIR}/newarrs.pa
                  FILE_DST ${DISASM_BIN_DIR}/newarrs.bc)

add_custom_target(disasm_binaries_test_instructions)

add_dependencies(disasm_binaries_test_instructions disasm_binaries-instructions)
add_dependencies(disasm_binaries_test_instructions disasm_binaries-returns)
add_dependencies(disasm_binaries_test_instructions disasm_binaries-calls)
add_dependencies(disasm_binaries_test_instructions disasm_binaries-newarrs)

add_dependencies(disasm_test_instructions disasm_binaries_test_instructions)

# metadata_tests files
compile_pre_build(TARGET disasm_binaries-meta
                  FILE_SRC ${DISASM_TESTS_DIR}/meta.pa
                  FILE_DST ${DISASM_BIN_DIR}/meta.bc)

add_custom_target(disasm_binaries_test_metadata)

add_dependencies(disasm_binaries_test_metadata disasm_binaries-meta)

add_custom_target(disasm_tests
                  DEPENDS disasm_test_records
                  DEPENDS disasm_test_labels
                  DEPENDS disasm_test_instructions)

add_dependencies(arkdisassembler isa_gen_${PROJECT_NAME} type_to_pandasm_type_gen_${PROJECT_NAME})

##############################
