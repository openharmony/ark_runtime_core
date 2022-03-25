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
# Common functions for add environment for compiling and cross-compiling.
# Used by different toolchains.

function(set_c_compiler c_compiler_id)
    find_program(c_compiler ${c_compiler_id})
    if(NOT c_compiler)
        message(FATAL_ERROR "Unable to find C compiler ${c_compiler_id}")
    endif()

    set(CMAKE_C_COMPILER ${c_compiler} PARENT_SCOPE)
endfunction()

function(set_cxx_compiler cxx_compiler_id)
    find_program(cxx_compiler ${cxx_compiler_id})
    if(NOT cxx_compiler)
        message(FATAL_ERROR "Unable to find C++ compiler ${cxx_compiler_id}")
    endif()

    set(CMAKE_CXX_COMPILER ${cxx_compiler} PARENT_SCOPE)
endfunction()

function(set_cross_amd64_x86)
    if(NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        message(FATAL_ERROR "Unsupported host processor ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()

    # Caveats about the code below:
    # * The "if" is needed because the toolchain file is parsed twice.
    # * add_compile_options is not used intentionally as it does not
    #   propagate the options to the linker, which is needed.
    if (NOT PANDA_CROSS_AMD64_X86)
        set(PANDA_CROSS_AMD64_X86 1 CACHE STRING "" FORCE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32" CACHE STRING "" FORCE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "" FORCE)
        set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32" CACHE STRING "" FORCE)
    endif()
endfunction()

