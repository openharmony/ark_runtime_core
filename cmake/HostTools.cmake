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

# To build parts of the platform written in Panda Assembly,
# we need to have the assembler binary. If we are not
# cross-compiling, we are good to go. Otherwise a subset
# of source tree is exposed as a separate project for
# "host tools", which is built solely for host.

function(panda_configure_host_tools)
    if(NOT CMAKE_CROSSCOMPILING)
        return()
    endif()

    include(ExternalProject)

    set(host_tools_dir "${CMAKE_CURRENT_BINARY_DIR}/host-tools")

    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/host-tools-CMakeLists.txt"
        "${host_tools_dir}/CMakeLists.txt"
        @ONLY ESCAPE_QUOTES)

    if ($ENV{NPROC_PER_JOB})
        set(PANDA_HOST_TOOLS_JOBS_NUMBER $ENV{NPROC_PER_JOB})
    else()
        set(PANDA_HOST_TOOLS_JOBS_NUMBER 16)
    endif()

    add_custom_target(compiler_host_tools-depend)

    ExternalProject_Add(panda_host_tools
        DEPENDS           compiler_host_tools-depend
        SOURCE_DIR        "${host_tools_dir}"
        BINARY_DIR        "${host_tools_dir}-build"
        BUILD_IN_SOURCE   FALSE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} <SOURCE_DIR> -G "${CMAKE_GENERATOR}"
                          -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                          -DPANDA_ROOT_SOURCE_DIR=${PANDA_ROOT}
                          -DPANDA_ROOT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                          -DPANDA_THIRD_PARTY_SOURCES_DIR=${PANDA_THIRD_PARTY_SOURCES_DIR}
                          -DPANDA_THIRD_PARTY_CONFIG_DIR=${PANDA_THIRD_PARTY_CONFIG_DIR}
                          -DPANDA_PRODUCT_BUILD=true
        BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR> --target all -- -j${PANDA_HOST_TOOLS_JOBS_NUMBER}
        INSTALL_COMMAND   ${CMAKE_COMMAND} -E echo "Skipping install step"
    )
endfunction()

panda_configure_host_tools()
