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

set(PANDA_TRIPLET x86_64-w64-mingw32)
set(PANDA_SYSROOT /usr/${PANDA_TRIPLET})

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_PREFIX_PATH ${PANDA_SYSROOT})
set(CMAKE_C_COMPILER_TARGET ${PANDA_TRIPLET})
set(CMAKE_CXX_COMPILER_TARGET ${PANDA_TRIPLET})

# NB! Do not use "win32" threading model, it does not provide
# std::thread and std::mutex. Use "posix" instead.
set(MINGW_THREADING_MODEL posix)
set(MINGW_CXX_BIN_NAME ${PANDA_TRIPLET}-g++-${MINGW_THREADING_MODEL})

find_program(MINGW_CXX_BIN ${MINGW_CXX_BIN_NAME})
if("${MINGW_CXX_BIN}" STREQUAL "MINGW_CXX_BIN-NOTFOUND")
    message(FATAL_ERROR "Unable to find MinGW ${MINGW_CXX_BIN_NAME}")
endif()

execute_process(COMMAND ${MINGW_CXX_BIN} -dumpversion
                OUTPUT_VARIABLE MINGW_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(MINGW_CXX_INC /usr/lib/gcc/${PANDA_TRIPLET}/${MINGW_VERSION}/include/c++)

add_compile_options(
    -isystem ${MINGW_CXX_INC}
    -I ${MINGW_CXX_INC}/${PANDA_TRIPLET} # For #include <bits/...>
    --sysroot=${PANDA_SYSROOT}
    --target=${PANDA_TRIPLET}
)

# NB! For Windows we link everything statically (incl. standard library, pthread, etc.):
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/lib/gcc/${PANDA_TRIPLET}/${MINGW_VERSION} -static-libstdc++ -static-libgcc -Wl,-Bstatic")

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
set_c_compiler(clang-9)
set_cxx_compiler(clang++-9)
