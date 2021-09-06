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

project(arkziparchive)

add_library(arkziparchive ${PANDA_DEFAULT_LIB_TYPE} zip_archive.cpp)
target_include_directories(arkziparchive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arkziparchive arkbase miniz)

panda_add_gtest(
    NAME arkziparchive_tests
    SOURCES
        tests/libziparchive_tests.cpp
    LIBRARIES
        arkbase
        arkfile
        arkziparchive
        arkassembler
        miniz
    SANITIZERS
        ${PANDA_SANITIZERS_LIST}
)

panda_add_to_clang_tidy(TARGET arkziparchive)
panda_add_sanitizers(TARGET arkziparchive SANITIZERS ${PANDA_SANITIZERS_LIST})

if (PANDA_ENABLE_AFL)
    include("${PANDA_ROOT}/fuzzing/Fuzzing.cmake")
    panda_substitute_libs(TARGET arkziparchive LIBS arkbase miniz)
endif()

add_check_style(".")
