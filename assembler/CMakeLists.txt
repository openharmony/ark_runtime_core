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
project(assembler CXX)

panda_add_executable(ark_asm pandasm.cpp)

set(PANDASM_BIN_TESTS ${CMAKE_CURRENT_BINARY_DIR}/tests)
file(MAKE_DIRECTORY "${PANDASM_BIN_TESTS}")

panda_isa_gen(
    TEMPLATES
        "isa.h.erb"
        "ins_emit.h.erb"
        "ins_to_string.cpp.erb"
        "ins_create_api.h.erb"
        "opcode_parsing.h.erb"
        "operand_types_print.h.erb"
    REQUIRES
        "${CMAKE_CURRENT_SOURCE_DIR}/asm_isapi.rb"
        "${PANDA_ROOT}/libpandafile/pandafile_isapi.rb"
)

add_library(arkassembler ${PANDA_DEFAULT_LIB_TYPE}
    lexer.cpp
    annotation.cpp
    assembly-emitter.cpp
    assembly-parser.cpp
    assembly-program.cpp
    assembly-type.cpp
    assembly-ins.cpp
    context.cpp
    meta.cpp
    ins_to_string.cpp
    extensions/extensions.cpp
    extensions/ecmascript/ecmascript_meta.cpp
)

add_dependencies(arkassembler isa_gen_assembler arkfile)

set(META_GEN_H ${CMAKE_CURRENT_BINARY_DIR}/meta_gen.h)
panda_gen_file(
    DATAFILE ${CMAKE_CURRENT_SOURCE_DIR}/metadata.yaml
    TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/meta_gen.cpp.erb
    OUTPUTFILE ${META_GEN_H}
    REQUIRES ${CMAKE_CURRENT_SOURCE_DIR}/asm_metadata.rb
)

set(ECMASCRIPT_META_GEN_H ${CMAKE_CURRENT_BINARY_DIR}/ecmascript_meta_gen.h)
panda_gen_file(
        DATAFILE ${CMAKE_CURRENT_SOURCE_DIR}/extensions/ecmascript/metadata.yaml
        TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/meta_gen.cpp.erb
        OUTPUTFILE ${ECMASCRIPT_META_GEN_H}
        REQUIRES ${CMAKE_CURRENT_SOURCE_DIR}/asm_metadata.rb
)

add_custom_target(meta_gen_assembler DEPENDS ${META_GEN_H} ${ECMASCRIPT_META_GEN_H})

add_dependencies(arkassembler meta_gen_assembler)

target_include_directories(arkassembler
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

panda_add_to_clang_tidy(TARGET ark_asm CHECKS
    "-cert-dcl21-cpp"
    "-cppcoreguidelines-macro-usage"
    "-google-runtime-references"
    "-misc-non-private-member-variables-in-classes"
)

target_link_libraries(arkassembler arkfile)
target_link_libraries(ark_asm arkassembler arkbase)

include_directories(${PANDA_ROOT}/libpandabase/)

panda_add_gtest(
    NAME assembler_tests
    SOURCES
        tests/lexer_test.cpp
        tests/parser_test.cpp
        tests/emitter_test.cpp
        tests/mangling_tests.cpp
    LIBRARIES
        arkbase arkassembler
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)
if(TARGET assembler_tests)
    target_compile_options(assembler_tests PUBLIC "-Wno-ignored-attributes")
endif()

panda_add_sanitizers(TARGET arkassembler SANITIZERS ${PANDA_SANITIZERS_LIST})
panda_add_sanitizers(TARGET ark_asm SANITIZERS ${PANDA_SANITIZERS_LIST})

add_custom_target(pandasm ALL
                  COMMAND cd $<TARGET_FILE_DIR:ark_asm> && ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ark_asm> pandasm)

add_dependencies(pandasm ark_asm)

if (PANDA_ENABLE_AFL)
    include("${PANDA_ROOT}/fuzzing/Fuzzing.cmake")
    panda_substitute_libs(TARGET arkassembler LIBS arkbase c_secshared arkfile)
endif()

add_check_style(".")
