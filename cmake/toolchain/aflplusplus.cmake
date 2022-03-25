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

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

set(ENV{AFL_CC_COMPILER} "LLVM")
set(ENV{AFL_LLVM_LAF_ALL} "1")
set(PANDA_ENABLE_CLANG_TIDY false CACHE BOOL "Enable clang-tidy checks during compilation" FORCE)

set_c_compiler(afl-clang-fast)
set_cxx_compiler(afl-clang-fast++)

add_definitions(-DFUZZING_EXIT_ON_FAILED_ASSERT=1)
add_definitions(-DFUZZING_EXIT_ON_FAILED_ASSERT_FOR="/libpandafile/,/libpandabase/")

set(PANDA_ENABLE_AFL true)
set(PANDA_FUZZING_SANITIZERS false)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-stack-address")
