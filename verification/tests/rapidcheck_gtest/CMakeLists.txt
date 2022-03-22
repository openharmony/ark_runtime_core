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

cmake_minimum_required(VERSION 3.10)

project(arkverification_rapidcheck_gtest_tests)

include(${PANDA_ROOT}/verification/Verification.cmake)

# note: no USE_CATCH2
panda_add_gtest(
    RAPIDCHECK_ON
    NO_CORES
    NAME arkverification_rapidcheck_gtest_tests
    SOURCES
        ${VERIFIER_RAPIDCHECK_TESTS_SOURCES}
    LIBRARIES
        arkbase
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)
if(TARGET arkverification_rapidcheck_gtest_tests)
    target_include_directories(arkverification_rapidcheck_gtest_tests PUBLIC ${PANDA_ROOT} ${PANDA_ROOT}/libpandabase)
    add_verification_includes(TARGET arkverification_rapidcheck_gtest_tests)
    target_compile_options(arkverification_rapidcheck_gtest_tests PUBLIC "-Wno-ignored-attributes")
    panda_add_sanitizers(TARGET arkverification_rapidcheck_gtest_tests SANITIZERS ${PANDA_SANITIZERS_LIST})
endif()


add_check_style(".")
