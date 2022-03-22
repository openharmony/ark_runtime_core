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

# Currently we fix a certain version of clang-format to avoid unstable linting,
# which may occur if different versions of the tools are used by developers.
set(PANDA_CLANG_FORMAT "clang-format-9")

# Require clang-format
find_program(
  CLANG_FORMAT
  NAMES "${PANDA_CLANG_FORMAT}"
  DOC "Path to clang-format executable"
  )
if(NOT CLANG_FORMAT)
  message(WARNING "Clang-format not found.")
else()
  message(STATUS "clang-format found: ${CLANG_FORMAT}")
endif()

# Function to add targets for clang_format, clang_force_format
function(add_check_style dir)
    file(GLOB_RECURSE dir_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${dir}/*.cpp ${dir}/*.cc)
    file(GLOB_RECURSE dir_headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${dir}/*.h)

    if (CLANG_FORMAT)
        if(TARGET clang_format)
        else()
            add_custom_target(clang_format)
        endif()
        if(TARGET clang_force_format)
        else()
            add_custom_target(clang_force_format)
        endif()
    endif()

    if(NOT TARGET check_concurrency_format)
        add_custom_target(check_concurrency_format)
    endif()

    foreach(src ${dir_sources})
        get_filename_component(source ${src} ABSOLUTE)
        file(RELATIVE_PATH src ${PANDA_ROOT} ${source})
        STRING(REGEX REPLACE "/" "_" src ${src})
        if (CLANG_FORMAT)
            add_clang_format(${source} ${src})
            add_clang_force_format(${source} ${src})
        endif()
        add_check_concurrency_format(${source} ${src})
    endforeach()

    # Also add format-target for headers
    foreach(src ${dir_headers})
        get_filename_component(source ${src} ABSOLUTE)
        file(RELATIVE_PATH src ${PANDA_ROOT} ${source})
        STRING(REGEX REPLACE "/" "_" src ${src})
        if (CLANG_FORMAT)
            add_clang_format(${source} ${src})
            add_clang_force_format(${source} ${src})
            add_dependencies(clang_format clang_format_${src})
            add_dependencies(clang_force_format clang_force_format_${src})
        endif()
        add_check_concurrency_format(${source} ${src})
        add_dependencies(check_concurrency_format check_concurrency_format_${src})
    endforeach()
endfunction()

# Function to check through clang-format
function(add_clang_format src tgt)
    if (TARGET clang_format_${tgt})
        return()
    endif()
    add_custom_target(clang_format_${tgt}
        COMMAND ${PANDA_ROOT}/scripts/run-clang-format ${PANDA_CLANG_FORMAT} ${src})
    add_dependencies(clang_format clang_format_${tgt})
endfunction()

# Function to check correct usage of std primitives.
function(add_check_concurrency_format src tgt)
    set(CHECK_CONCURRENCY_FORMAT "${PANDA_ROOT}/scripts/run-check-concurrency-format.sh")

    if (NOT TARGET check_concurrency_format_${tgt})
        add_custom_target(check_concurrency_format_${tgt}
            COMMAND ${CHECK_CONCURRENCY_FORMAT} ${src}
        )
        add_dependencies(check_concurrency_format check_concurrency_format_${tgt})
    endif()
endfunction()

# Function to force style through clang-format
function(add_clang_force_format src tgt)
    if (NOT TARGET clang_force_format_${tgt})
        add_custom_target(clang_force_format_${tgt}
            COMMAND ${CLANG_FORMAT} -i -style=file ${src}
        )
        add_dependencies(clang_force_format clang_force_format_${tgt})
    endif()
endfunction()

