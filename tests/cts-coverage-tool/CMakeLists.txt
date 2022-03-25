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

cmake_minimum_required(VERSION 3.5.2 FATAL_ERROR)

project(cts_coverage_tool)

# Coverage reports generation

# Source files:
set(ISA_FILE "${CMAKE_BINARY_DIR}/isa/isa.yaml")
set(NON_TESTABLE "${PROJECT_SOURCE_DIR}/non_testable.yaml")
set(TESTS_DIR "${PROJECT_BINARY_DIR}/../cts-generator")
set(TESTS_GLOB_ASSEMBLY "cts-assembly/**/*.pa")
set(TESTS_GLOB_GENERATED "cts-generated/**/*.pa")
file(
    COPY "${PROJECT_SOURCE_DIR}/../cts-assembly"
    DESTINATION "${PROJECT_BINARY_DIR}/../cts-generator"
)

# Generated files:
set(UNCOVERED "${PROJECT_BINARY_DIR}/uncovered.yaml")
set(UNCOVERED_MD "${PROJECT_BINARY_DIR}/uncovered.md")
set(ORPHANED "${PROJECT_BINARY_DIR}/orphaned.yaml")
set(ORPHANED_MD "${PROJECT_BINARY_DIR}/orphaned.md")
set(FULL "${PROJECT_BINARY_DIR}/full.yaml")
set(FULL_MD "${PROJECT_BINARY_DIR}/full.md")
set(REPORT "${PROJECT_BINARY_DIR}/report.yaml")
set(OUTPUT_FILES ${UNCOVERED} ${UNCOVERED_MD} ${ORPHANED} ${ORPHANED_MD} ${FULL} ${FULL_MD} ${REPORT} ALWAYS_REBUILD)

# Coverage tool:
set(SPECTRAC "${PROJECT_SOURCE_DIR}/spectrac.rb")

add_custom_command(OUTPUT ${OUTPUT_FILES}
    COMMENT "Compute ISA spec coverage by CTS tests"
    COMMAND ${SPECTRAC} --spec ${ISA_FILE} --testglob ${TESTS_GLOB_ASSEMBLY} --testglob ${TESTS_GLOB_GENERATED} --testdir ${TESTS_DIR} --non_testable ${NON_TESTABLE} --uncovered ${UNCOVERED} --uncovered_md ${UNCOVERED_MD} --orphaned ${ORPHANED} --orphaned_md ${ORPHANED_MD} --full ${FULL} --full_md ${FULL_MD} --report ${REPORT}
)

add_custom_target(isa_coverage DEPENDS cts-generator ${OUTPUT_FILES})
