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

set(PANDA_TRIPLET aarch64-linux-gnu)
set(PANDA_SYSROOT /usr/${PANDA_TRIPLET})
set(PANDA_RUN_PREFIX qemu-aarch64 -L ${PANDA_SYSROOT})

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_PREFIX_PATH ${PANDA_SYSROOT})
set(CMAKE_C_COMPILER_TARGET ${PANDA_TRIPLET})
set(CMAKE_CXX_COMPILER_TARGET ${PANDA_TRIPLET})
add_compile_options(
    -isystem ${PANDA_SYSROOT}/include/c++/8/${PANDA_TRIPLET}
    --sysroot=${PANDA_SYSROOT}
    --target=${PANDA_TRIPLET}
)

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
set_c_compiler(clang-8)
set_cxx_compiler(clang++-8)
