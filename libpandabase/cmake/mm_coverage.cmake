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

option(ENABLE_MM_COVERAGE "Enable coverage-calculation for libpandabase" false)

find_program(
    LCOV
    NAMES "lcov"
    DOC "Path to lcov executable")
if(NOT LCOV)
    set(ENABLE_MM_COVERAGE false)
endif()

find_program(
    GENHTML
    NAMES "genhtml"
    DOC "Path to genhtml executable")
if(NOT GENHTML)
    set(ENABLE_MM_COVERAGE false)
endif()

if(ENABLE_MM_COVERAGE)
    # Set coverage options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    add_custom_target(mm_coverage DEPENDS arkbase_tests)
    set(ADD_COV_FLAGS --quiet --rc lcov_branch_coverage=1)
    add_custom_command(TARGET mm_coverage POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        # Update current coverage info
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR}/* -iname "*.gcda" -delete
        # run tests
        COMMAND arkbase_tests
        # coverage
        COMMAND lcov --no-external -b ${PANDA_ROOT}/libpandabase -d ${CMAKE_CURRENT_BINARY_DIR} -c -o mm_coverage.info ${ADD_COV_FLAGS}
        # html-file generation
        COMMAND genhtml -o mm_coverage_report mm_coverage.info --ignore-errors source ${ADD_COV_FLAGS}
        COMMAND echo "Coverage report path: ${CMAKE_CURRENT_BINARY_DIR}/mm_coverage_report"
        # Delete temporary files to collect statistics
        COMMAND rm mm_coverage.info
    )
else()
    message(STATUS "Coverage will not be calculated (may be enabled by -DENABLE_MM_COVERAGE=true ).")
endif(ENABLE_MM_COVERAGE)
