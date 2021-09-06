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

option(PANDA_ENABLE_SANITIZE_TRAP "Enable sanitizer trap generation" false)
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(PANDA_ENABLE_SANITIZE_TRAP true)
endif ()

option(PANDA_ENABLE_ADDRESS_SANITIZER "Address sanitizer enable" false)
option(PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER "Undefined behavior sanitizer enable" false)
option(PANDA_ENABLE_THREAD_SANITIZER "Thread sanitizer enable" false)

set(PANDA_SANITIZERS_LIST)

if (PANDA_ENABLE_ADDRESS_SANITIZER)
    message(STATUS "Enabled ASAN")
    list(APPEND PANDA_SANITIZERS_LIST "address")
endif()

if (PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER)
    message(STATUS "Enabled UBSAN")
    list(APPEND PANDA_SANITIZERS_LIST "undefined")
    # GCC doesn't have built-in macro for UBSAN, bypass by making our own definition.
    add_definitions(-D__SANITIZE_UNDEFINED__)
endif()

if (PANDA_ENABLE_THREAD_SANITIZER)
    message(STATUS "Enabled TSAN")
    list(APPEND PANDA_SANITIZERS_LIST "thread")
endif()

# Workaround for issue 529
# As of the beginning of May 2020 we have checked: gcc 7.5.0, gcc 8.4.0, gcc 9.3.0 and 10.0-rc versions, all have
# some false-positive or another issues when compiling with ASAN or UBSAN in release mode. So, cover this with early-bailout.
if ((PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER) AND
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND (CMAKE_BUILD_TYPE MATCHES Release))
        message(FATAL_ERROR "GCC gives false positives in release builds with ASAN or UBSAN, please use clang or another compiler.")
    unset(build_type)
endif()

# Add options to build with sanitizers to specified target
#
# Example usage:
#   panda_add_sanitizers(TARGET <name> SANITIZERS <sanitizer1>,<sanitizer2>)
#
# Adds options to build target <name> with <sanitizer1> and <sanitizer2>
#
# Available sanitizers: address, thread, undefined

function(panda_add_sanitizers)
    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET)
    set(multiValues SOURCES SANITIZERS)

    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if(ARG_SANITIZERS)
        set(AVAILABLE_SANITIZERS "address" "undefined" "thread")
        foreach(sanitizer ${ARG_SANITIZERS})
            if(NOT ${sanitizer} IN_LIST AVAILABLE_SANITIZERS)
                message(FATAL_ERROR "Unknown sanitizer: ${sanitizer}")
            endif()
        endforeach()
        string(REPLACE ";" "," ARG_SANITIZERS "${ARG_SANITIZERS}")
        target_compile_options(${ARG_TARGET} PUBLIC "-fsanitize=${ARG_SANITIZERS}" -g)
        if (PANDA_ENABLE_SANITIZE_TRAP)
            target_compile_options(${ARG_TARGET} PUBLIC "-fsanitize-undefined-trap-on-error")
        endif()
        set_target_properties(${ARG_TARGET} PROPERTIES LINK_FLAGS "-fsanitize=${ARG_SANITIZERS}")

        if (PANDA_TARGET_MOBILE)
            target_link_libraries(${ARG_TARGET} log)
        endif()
    endif()
endfunction()
