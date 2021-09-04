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

project(verifier)

set(SOURCES
    verifier.cpp
)

#add_library(verifier ${PANDA_DEFAULT_LIB_TYPE} ${SOURCES})

panda_add_executable(verifier ${SOURCES})

panda_add_to_clang_tidy(TARGET verifier CHECKS
    "-cppcoreguidelines-macro-usage"
    "-cppcoreguidelines-non-private-member-variables-in-classes"
    "-cppcoreguidelines-pro-bounds-pointer-arithmetic"
    "-google-explicit-constructor"
    "-hicpp-explicit-conversions"
    "-misc-non-private-member-variables-in-classes"
    "-readability-identifier-naming"
    "-readability-named-parameter"
)

include_directories(${PANDA_ROOT}/libpandabase/)

target_include_directories(verifier
    PUBLIC ../runtime
)

target_link_libraries(verifier arkruntime arkbase)

panda_add_sanitizers(TARGET verifier SANITIZERS ${PANDA_SANITIZERS_LIST})

#add_custom_target(panda ALL
#                  COMMAND cd $<TARGET_FILE_DIR:ark> && ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ark> panda)

#add_dependencies(verifier ark)

add_check_style(".")
