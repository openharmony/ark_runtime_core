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

project(dprofstorage)

if(NOT PANDA_TARGET_UNIX)
    message(FATAL_ERROR "Platform ${CMAKE_SYSTEM_NAME} is not supported")
endif ()

add_library(dprofstorage STATIC dprof/storage.cpp)
target_link_libraries(dprofstorage arkbase)
target_include_directories(dprofstorage INTERFACE ".")
panda_set_lib_32bit_property(dprofstorage)
panda_add_to_clang_tidy(TARGET dprofstorage)
add_check_style(".")
