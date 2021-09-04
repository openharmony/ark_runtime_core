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

set(VERIFICATION_SOURCES_DIR ${PANDA_ROOT}/verification)

include(${VERIFICATION_SOURCES_DIR}/type/Type.cmake)
include(${VERIFICATION_SOURCES_DIR}/value/Value.cmake)
include(${VERIFICATION_SOURCES_DIR}/cflow/Cflow.cmake)
include(${VERIFICATION_SOURCES_DIR}/util/Util.cmake)
include(${VERIFICATION_SOURCES_DIR}/absint/AbsInt.cmake)
include(${VERIFICATION_SOURCES_DIR}/debug/Debug.cmake)
include(${VERIFICATION_SOURCES_DIR}/job_queue/JobQueue.cmake)
include(${VERIFICATION_SOURCES_DIR}/thread/VerifierThread.cmake)
include(${VERIFICATION_SOURCES_DIR}/cache/Cache.cmake)

set(VERIFIER_SOURCES
    ${VERIFICATION_SOURCES_DIR}/verification_options.cpp
    ${TYPE_SOURCES}
    ${VALUE_SOURCES}
    ${CFLOW_SOURCES}
    ${UTIL_SOURCES}
    ${ABSINT_SOURCES}
    ${DEBUG_SOURCES}
    ${JOB_QUEUE_SOURCES}
    ${VERIFIER_THREAD_SOURCES}
    ${VERIFIER_CACHE_SOURCES}
)

set(VERIFIER_TESTS_SOURCES
    ${TYPE_TESTS_SOURCES}
    ${VALUE_TESTS_SOURCES}
    ${CFLOW_TESTS_SOURCES}
    ${UTIL_TESTS_SOURCES}
    ${ABSINT_TESTS_SOURCES}
    ${JOB_QUEUE_TESTS_SOURCES}
)

set(VERIFIER_RAPIDCHECK_TESTS_SOURCES
    ${UTIL_RAPIDCHECK_TESTS_SOURCES}
)

set(VERIFIER_GEN_INCLUDE_DIR ${PANDA_BINARY_ROOT}/verification/gen/include)

set(VERIFIER_INCLUDE_DIR ${VERIFICATION_SOURCES_DIR})

function(add_verification_includes)
    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET)
    set(multiValues)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    add_dependencies(${ARG_TARGET} isa_gen_pandaverification_gen)

    target_include_directories(${ARG_TARGET}
        PUBLIC ${VERIFIER_INCLUDE_DIR}
        PUBLIC ${VERIFIER_GEN_INCLUDE_DIR}
    )
endfunction()

function(add_verification_sources)
    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET)
    set(multiValues)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    target_sources(${ARG_TARGET}
        PUBLIC ${VERIFIER_SOURCES}
    )
endfunction()

function(add_pandaverification)
    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET)
    set(multiValues)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    add_verification_sources(TARGET ${ARG_TARGET})
    add_verification_includes(TARGET ${ARG_TARGET})
endfunction()
