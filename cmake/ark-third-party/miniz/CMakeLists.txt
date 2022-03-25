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

project(miniz)

add_library(miniz ${MINIZ_ROOT}/miniz.c)
target_include_directories(miniz PUBLIC ${MINIZ_ROOT})

target_compile_options(miniz PUBLIC -Wno-return-type-c-linkage)

# NB! We always build miniz statically, but there seems
# no obvious reasons for that. If we reconsider, the logic
# below should be replaced with something like:
#
# add_library(miniz ${PANDA_DEFAULT_LIB_TYPE} miniz.c)
#
# **Besides** build of host tools should be fixed to
# take into account new shared library
if(NOT PANDA_TARGET_WINDOWS)
    target_compile_options(miniz PRIVATE -fPIC)
endif()

if (PANDA_ENABLE_AFL)
    include("${PANDA_ROOT}/fuzzing/Fuzzing.cmake")
    panda_substitute_libs(TARGET miniz)
endif()
