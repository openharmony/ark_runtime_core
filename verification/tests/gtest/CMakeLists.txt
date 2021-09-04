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

cmake_minimum_required(VERSION 3.10)

project(arkverification_tests)

include(${PANDA_ROOT}/verification/Verification.cmake)

panda_add_gtest(
    NO_CORES
    NAME arkverification_tests
    SOURCES
        ${VERIFIER_TESTS_SOURCES}
    LIBRARIES
        arkruntime
        arkbase
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)
if(TARGET arkverification_tests)
    target_include_directories(arkverification_tests PUBLIC ${PANDA_ROOT})
    target_compile_options(arkverification_tests PUBLIC "-Wno-ignored-attributes")
endif()

panda_add_sanitizers(TARGET arkverification_tests SANITIZERS ${PANDA_SANITIZERS_LIST})

add_check_style(".")
