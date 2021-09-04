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

cmake_minimum_required(VERSION 3.3.2 FATAL_ERROR)
project(arkfileExt)

set(SOURCES
    file_ext.cpp)

add_library(arkfileExt SHARED ${SOURCES})

panda_add_to_clang_tidy(TARGET arkfileExt)

target_link_libraries(arkfileExt arkfile arkbase)
target_include_directories(arkfileExt
                          PUBLIC ${PANDA_ROOT}
                          PUBLIC ${PANDA_ROOT}/libpandabase)

panda_add_sanitizers(TARGET arkfileExt SANITIZERS ${PANDA_SANITIZERS_LIST})

set(PANDA_SUPPORT_SOURCE
    panda_file_support.cpp)

add_library(arksupport STATIC SHARED ${PANDA_SUPPORT_SOURCE})
set_property(TARGET arksupport PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(arksupport arkfile arkbase)


panda_add_to_clang_tidy(TARGET arksupport)

target_include_directories(arksupport
                          PUBLIC ${PANDA_ROOT}
                          PUBLIC ${PANDA_ROOT}/libpandabase)

panda_add_sanitizers(TARGET arksupport SANITIZERS ${PANDA_SANITIZERS_LIST})

add_check_style(".")
