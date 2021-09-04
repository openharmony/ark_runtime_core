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

set(TOOLCHAIN_CLANG_ROOT "" CACHE STRING "Path to clang-<version> directory")
set(TOOLCHAIN_SYSROOT    "" CACHE STRING "Path to sysroot")

set(PANDA_TRIPLET aarch64-linux-ohos)
set(PANDA_SYSROOT ${TOOLCHAIN_SYSROOT})

set(CMAKE_SYSTEM_NAME OHOS)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_PREFIX_PATH ${TOOLCHAIN_SYSROOT})
set(CMAKE_SYSROOT ${TOOLCHAIN_SYSROOT})
set(CMAKE_C_COMPILER_TARGET ${PANDA_TRIPLET})
set(CMAKE_CXX_COMPILER_TARGET ${PANDA_TRIPLET})
set(CMAKE_ASM_COMPILER_TARGET ${PANDA_TRIPLET})

# Select lld early so enable_language() can detect compilers
add_link_options(
  -fuse-ld=lld
  --rtlib=compiler-rt
)
link_libraries(unwind)

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
set_c_compiler("${TOOLCHAIN_CLANG_ROOT}/bin/clang")
set_cxx_compiler("${TOOLCHAIN_CLANG_ROOT}/bin/clang++")
