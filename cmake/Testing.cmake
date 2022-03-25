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
# Convenience functions for testing Panda.

include(${CMAKE_CURRENT_LIST_DIR}/CommonTesting.cmake)

set(PANDA_CI_TESTING_MODE "Default")
if(DEFINED ENV{SCHEDULE_NAME})
    if($ENV{SCHEDULE_NAME} STREQUAL "Nightly")
        set(PANDA_CI_TESTING_MODE "Nightly")
    endif()
endif()
message(STATUS "PANDA_CI_TESTING_MODE         = ${PANDA_CI_TESTING_MODE}")

if(PANDA_WITH_TESTS)
    # Target for building all Googletest-based tests:
    add_custom_target(gtests_build COMMENT "Building gtests")

    # Use a custom target instead of `test` to ensure that running
    # Googletest-based tests depends on building them:
    add_custom_target(gtests
                      COMMENT "Running gtests after building them"
                      DEPENDS gtests_build)

    if(NOT PANDA_SKIP_GTESTS)
        add_dependencies(tests gtests)
    endif()
endif()

# Add Googletest-based tests to the source tree.
#
# Example usage:
#
#   panda_add_gtest(NAME test_name
#     OUTPUT_DIRECTORY /path/to/output/dir
#     SOURCES
#       tests/unit1_test.c
#       tests/unit2_test.c
#     INCLUDE_DIRS
#       path/to/include1
#       path/to/include2
#     LIBRARIES
#       component1 component2
#     SANITIZERS sanitizer1,sanitizer2,..
#   )
#
# Available sanitizers: address, thread, undefined
#
# This will create a target test_name which consists of the sources defined
# in SOURCES and linked with libraries defined in LIBRARIES. If the list of
# paths is specified in INCLUDE_DIRS, these paths will be added as include
# directories for the test_name target.
#
# If OUTPUT_DIRECTORY is not defined, the binary will be put to bin-gtests
# directory at the build tree root.
#
# Notes:
#    * This function is a no-op if Googletest is not found.
#    * test_name behaves as a standard CMake target, i.e. such operations as
#      target_compile_options, etc. are supported.
#
# Additional actions on test_name include:
#    * Target-specific definition PANDA_GTEST is added.
#    * Googletest-specific libraries are linked to test_name by default,
#      and there's no need to set them explicitly.

function(panda_add_gtest)
    if(NOT PANDA_WITH_TESTS)
        return()
    endif()
    if(NOT "OUTPUT_DIRECTORY" IN_LIST ARGV)
        list(APPEND ARGV "OUTPUT_DIRECTORY" "${PANDA_BINARY_ROOT}/bin-gtests")
    endif()
    common_add_gtest(ENCLOSING_TARGET gtests_build ${ARGV})
endfunction()
