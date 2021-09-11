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
# Test suite for Panda.

# Targets for running separate suites:
add_custom_target(cts-assembly COMMENT "Running compliance tests for Panda Assembly")

if(NOT PANDA_TARGET_AMD64 OR PANDA_TARGET_WINDOWS)
  add_dependencies(tests cts-assembly)
  add_subdirectory(cts-coverage-tool)
else()
  add_custom_target(verifier-tests COMMENT "Regression tests for verifier")
  add_dependencies(tests cts-assembly cts-generator-build verifier-tests)
  if (PANDA_ENABLE_THREAD_SANITIZER)  # See issue #4332
    add_dependencies(tests cts-generator)
  endif()
  add_subdirectory(cts-coverage-tool)
  add_subdirectory(cts-generator)
endif()

# Add a single test run to the build tree.
#
# Usage:
#
#   panda_add_test_run(
#       FILE <source>
#       TARGET <target>
#       [SUBDIR <subdir>]
#       [EXPECTED_STDOUT <out_value>]
#       [SEARCH_DEBUG_STDERR <msg_value>]
#       [DEPENDS <dependency>...]
#       [RUNTIME_OPTIONS <option>...]
#       [GC_OPTIONS <option>]
#       [ARGUMENTS <argument>...]
#       [AOT_MODE TRUE|FALSE]
#       [LANGUAGE_CONTEXT <context>]
#   )
#
# Adds a test run target <target> with <source>
#
# Options:
#
# SUBDIR
#   Subdirectory in the current binary directory that is used to store build artifacts.
#   Full path to directory with artifacts: ${CMAKE_CURRENT_BINARY_DIR}/<subdir>/<target>-execute
#
# EXPECTED_STDOUT
#   Excepted value of test's stdout
#
# SEARCH_DEBUG_STDERR
#   Message which is expected to be present in stderr output
#
# DEPENDS
#   List of additional dependences (exclude assembler and interpreter)
#
# RUNTIME_OPTIONS
#   Runtime options
#
# GC_OPTIONS
#  Type of a gc
#
# ARGUMENTS
#   List of arguments that will be passed to program's entry point
#
# AOT_MODE, LANGUAGE_CONTEXT
#   See panda_add_asm_file
function(panda_add_test_run)
    set(prefix ARG)
    set(singleValues FILE TARGET SUBDIR SEARCH_DEBUG_STDERR AOT_MODE LANGUAGE_CONTEXT GC_OPTIONS)
    set(multiValues EXPECTED_STDOUT DEPENDS RUNTIME_OPTIONS ARGUMENTS PRLIMIT_OPTIONS)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (NOT DEFINED ARG_FILE)
        message(FATAL_ERROR "Mandatory FILE argument is not defined.")
    endif()

    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    if (TARGET ARG_TARGET)
        message(FATAL_ERROR "Target ${ARG_TARGET} is already defined.")
    endif()

    if(DEFINED ARG_GC_OPTIONS)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "${ARG_GC_OPTIONS}")
    endif()

    set(output_file)
    set(error_file)

    panda_add_asm_file(
        FILE ${ARG_FILE}
        TARGET ${ARG_TARGET}-execute
        SUBDIR "${ARG_SUBDIR}"
        OUTPUT_FILE_VARIABLE output_file
        ERROR_FILE_VARIABLE error_file
        LANGUAGE_CONTEXT "${ARG_LANGUAGE_CONTEXT}"
        AOT_MODE ${ARG_AOT_MODE}
        DEPENDS "${ARG_DEPENDS}"
        RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}"
        GC_OPTIONS "${ARG_GC_OPTIONS}"
        ENTRY_ARGUMENTS "${ARG_ARGUMENTS}"
        PRLIMIT_OPTIONS "${ARG_PRLIMIT_OPTIONS}"
    )

    add_custom_target(${ARG_TARGET} COMMENT "Running ${ARG_TARGET} test")
    add_dependencies(${ARG_TARGET} ${ARG_TARGET}-execute)

    if (DEFINED ARG_EXPECTED_STDOUT)
        set(expected_output_file "${output_file}.expected")
        file(WRITE "${expected_output_file}" "${ARG_EXPECTED_STDOUT}")

        add_custom_target(${ARG_TARGET}-check-stdout
                          COMMENT "Check ${ARG_TARGET} stdout"
                          COMMAND ${CMAKE_COMMAND} -E compare_files "${output_file}" "${expected_output_file}"
                          DEPENDS ${ARG_TARGET}-execute)

        add_dependencies(${ARG_TARGET} ${ARG_TARGET}-check-stdout)
    endif()

    if (DEFINED ARG_SEARCH_DEBUG_STDERR AND NOT (CMAKE_BUILD_TYPE MATCHES Release))
        #   file(READ "${error_file}" STDERR_AS_STRING)
        #   string(FIND ${STDERR_AS_STRING} ${ARG_SEARCH_DEBUG_STDERR})
        # but my cmake knowledge is not enough.

        add_custom_target(${ARG_TARGET}-check-logmsg
                          COMMENT "Check ${ARG_TARGET} log message"
                          COMMAND grep "${ARG_SEARCH_DEBUG_STDERR}" ${error_file} >/dev/null
                          DEPENDS ${ARG_TARGET}-execute)

        add_dependencies(${ARG_TARGET} ${ARG_TARGET}-check-logmsg)
    endif()

endfunction()

# Add a single test run for verifier to the build tree.
#
# Usage:
#
#   verifier_add_test_run(
#       FILE <source>
#       TARGET <target>
#       [SUBDIR <subdir>]
#       [EXPECTED_STDOUT <out_value>]
#       [SEARCH_DEBUG_STDERR <msg_value>]
#       [DEPENDS <dependency>...]
#       [LANGUAGE_CONTEXT <context>]
#   )
#
# Adds a test run target <target> with <source>
#
# Options:
#
# SUBDIR
#   Subdirectory in the current binary directory that is used to store build artifacts.
#   Full path to directory with artifacts: ${CMAKE_CURRENT_BINARY_DIR}/<subdir>/<target>-execute
#
# EXPECTED_STDOUT
#   Excepted value of test's stdout
#
# SEARCH_DEBUG_STDERR
#   Message which is expected to present in stderr output
#
# DEPENDS
#   List of additional dependencies (exclude assembler and interpreter)
function(verifier_add_test_run)
    set(prefix ARG)
    set(noValues VERIFIER_FAIL_TEST)
    set(singleValues FILE TARGET SUBDIR SEARCH_DEBUG_STDERR LANGUAGE_CONTEXT GC_OPTIONS)
    set(multiValues RUNTIME_OPTIONS VERIFIER_OPTIONS EXPECTED_STDOUT DEPENDS)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (NOT DEFINED ARG_FILE)
        message(FATAL_ERROR "Mandatory FILE argument is not defined.")
    endif()

    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    if (TARGET ARG_TARGET)
        message(FATAL_ERROR "Target ${ARG_TARGET} is already defined.")
    endif()

    if(DEFINED ARG_GC_OPTIONS)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "${ARG_GC_OPTIONS}")
    endif()

    set(output_file)
    set(error_file)

    if(DEFINED ARG_SEARCH_DEBUG_STDERR)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "--log-level=debug --log-components verifier --verification-debug-config-file ${CMAKE_CURRENT_SOURCE_DIR}/cts-generator/verifier.debug.config")
    endif()

    if(ARG_VERIFIER_FAIL_TEST)
        set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS},show-status,verifier-does-not-fail")
    else()
        set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS},show-status")
    endif()

    set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS} --verification-debug-config-file ${CMAKE_CURRENT_SOURCE_DIR}/cts-generator/verifier.debug.config")

    verifier_add_asm_file(
        FILE ${ARG_FILE}
        TARGET ${ARG_TARGET}-verify
        SUBDIR "${ARG_SUBDIR}"
        RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}"
        VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS}"
        OUTPUT_FILE_VARIABLE output_file
        ERROR_FILE_VARIABLE error_file
        LANGUAGE_CONTEXT "${ARG_LANGUAGE_CONTEXT}"
        DEPENDS "${ARG_DEPENDS}"
    )

    add_custom_target(${ARG_TARGET} COMMENT "Verifying ${ARG_TARGET} test")
    add_dependencies(${ARG_TARGET} ${ARG_TARGET}-verify)

    if (DEFINED ARG_EXPECTED_STDOUT)
        set(expected_output_file "${output_file}.expected")
        file(WRITE "${expected_output_file}" "${ARG_EXPECTED_STDOUT}")

        add_custom_target(${ARG_TARGET}-check-stdout
                          COMMENT "Check ${ARG_TARGET} stdout"
                          COMMAND ${CMAKE_COMMAND} -E compare_files "${output_file}" "${expected_output_file}"
                          DEPENDS ${ARG_TARGET}-verify)

        add_dependencies(${ARG_TARGET} ${ARG_TARGET}-check-stdout)
    endif()

    if (DEFINED ARG_SEARCH_DEBUG_STDERR AND NOT (CMAKE_BUILD_TYPE MATCHES Release))
        #   file(READ "${error_file}" STDERR_AS_STRING)
        #   string(FIND ${STDERR_AS_STRING} ${ARG_SEARCH_DEBUG_STDERR})
        # but my cmake knowledge is not enough.

        add_custom_target(${ARG_TARGET}-check-logmsg
                          COMMENT "Check ${ARG_TARGET} log message"
                          COMMAND grep -zo "${ARG_SEARCH_DEBUG_STDERR}" ${error_file} >/dev/null
                          DEPENDS ${ARG_TARGET}-verify)

        add_dependencies(${ARG_TARGET} ${ARG_TARGET}-check-logmsg)
    endif()

endfunction()


# Add a test with default and enforced jit runs
#
# Usage:
#
#   add_test_file(
#       FILE <source>
#       [RUNTIME_OPTIONS <option>...]
#       [ARGUMENTS <argument>...]
#       [EXPECTED_STDOUT <out_value>]
#       [DEBUG_LOG_MESSAGE <msg_value>]
#       [CTS_TEST]
#       [ENABLE_ECMASCRIPT]
#       [DISABLE_LIMIT_STD_ALLOC]
#       [SKIP_AOT]
#       [SKIP_VERIFICATION]
#   )
#
# Adds a test <source>
#
# Options:
#
# RUNTIME_OPTIONS
#   Runtime options
#
# GC_OPTIONS
#  Type of a gc
#
# ARGUMENTS
#   List of arguments that will be passed to program's entry point
#
# EXPECTED_STDOUT
#   Excepted value of test's stdout
#
# DEBUG_LOG_MESSAGE
#   Message which is expected to present in stderr output with --log-level=debug
#
# CTS_TEST
#   Add runtime options related to all cts tests
#
# CTS_IGNORE_IR_FAILURES
#   Ignore failures occurred on IR-build and optimization stages
#
#
# ENABLE_ECMASCRIPT
#  Add runtime options related to ECMAScript runtime
#
# DISABLE_LIMIT_STD_ALLOC
#   Do not check malloc usage
#
# SKIP_AOT
#   Skip AOT run
#
# SKIP_OSR
#   Skip OSR run
#
# SKIP_VERIFICATION
#   Skip verification run
function(add_test_file)
    set(prefix ARG)
    set(noValues CTS_TEST CTS_IGNORE_IR_FAILURES ENABLE_ECMASCRIPT DISABLE_LIMIT_STD_ALLOC SKIP_AOT SKIP_OSR SKIP_VERIFICATION VERIFIER_FAIL_TEST)
    set(singleValues FILE VERIFIER_DEBUG_LOG_MESSAGE DEBUG_LOG_MESSAGE GC_OPTIONS)
    set(multiValues EXPECTED_STDOUT VERIFIER_EXPECTED_STDOUT ARGUMENTS RUNTIME_OPTIONS PRLIMIT_OPTIONS)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})


    if (NOT DEFINED ARG_FILE)
        message(FATAL_ERROR "Mandatory FILE argument is not defined.")
    endif()

    get_filename_component(target "${ARG_FILE}" NAME)
    get_filename_component(path "${ARG_FILE}" DIRECTORY)
    get_filename_component(suite ${path} NAME)

    if (NOT ARG_DISABLE_LIMIT_STD_ALLOC)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS} --limit-standard-alloc=true")
    endif()

    if(DEFINED ARG_GC_OPTIONS)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "${ARG_GC_OPTIONS}")
    endif()

    if (PANDA_ENABLE_EVENTS)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS} --events-output=csv")
    endif()

    set(DEPEND_LIST)

    set(language_context "panda-assembly")
    if (ARG_ENABLE_ECMASCRIPT)
        set(language_context "ecmascript")
        set(ARG_SKIP_AOT TRUE) # aot mode is not supported yet
        set(ARG_SKIP_VERIFICATION TRUE) #verification is not supported yet
        set(ARG_SKIP_OSR TRUE) #osr is not supported yet
        set(CUR_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "--run-gc-in-place=true") # ecma vm doesn't support concurrent GC
    endif()

    if (NOT ARG_SKIP_VERIFICATION)
        if (NOT ARG_VERIFIER_FAIL_TEST)
            verifier_add_test_run(
                FILE "${ARG_FILE}"
                TARGET ${target}-verifier
                SUBDIR ${suite}-verifier
                EXPECTED_STDOUT "${ARG_VERIFIER_EXPECTED_STDOUT}"
                SEARCH_DEBUG_STDERR ${ARG_VERIFIER_DEBUG_LOG_MESSAGE}
                LANGUAGE_CONTEXT "${language_context}"
                DEPENDS "${DEPEND_LIST}"
            )
        else()
            verifier_add_test_run(
                VERIFIER_FAIL_TEST
                FILE "${ARG_FILE}"
                TARGET ${target}-verifier
                SUBDIR ${suite}-verifier
                EXPECTED_STDOUT "${ARG_VERIFIER_EXPECTED_STDOUT}"
                SEARCH_DEBUG_STDERR ${ARG_VERIFIER_DEBUG_LOG_MESSAGE}
                LANGUAGE_CONTEXT "${language_context}"
                DEPENDS "${DEPEND_LIST}"
            )
            set(CUR_RUNTIME_OPTIONS "${CUR_RUNTIME_OPTIONS}" "--verification-options=verifier-does-not-fail,debug-enable,show-status")
        endif()
        add_dependencies(${suite} ${target}-verifier)
    endif()

    panda_add_test_run(
        FILE "${ARG_FILE}"
        TARGET ${target}-stw
        SUBDIR ${suite}-stw
        EXPECTED_STDOUT "${ARG_EXPECTED_STDOUT}"
        SEARCH_DEBUG_STDERR ${CUR_DEBUG_LOG_MESSAGE}
        RUNTIME_OPTIONS "${CUR_RUNTIME_OPTIONS}" "--compiler-enable-jit=false --gc-type=stw"
        ARGUMENTS "${ARG_ARGUMENTS}"
        LANGUAGE_CONTEXT "${language_context}"
        AOT_MODE FALSE
        PRLIMIT_OPTIONS "${ARG_PRLIMIT_OPTIONS}"
        DEPENDS "${DEPEND_LIST}"
    )
    add_dependencies(${suite} ${target}-stw)


    panda_add_test_run(
        FILE "${ARG_FILE}"
        TARGET ${target}-gengc
        SUBDIR ${suite}-gengc
        EXPECTED_STDOUT "${ARG_EXPECTED_STDOUT}"
        SEARCH_DEBUG_STDERR ${CUR_DEBUG_LOG_MESSAGE}
        RUNTIME_OPTIONS "${CUR_RUNTIME_OPTIONS}" "--compiler-enable-jit=false --gc-type=gen-gc"
        ARGUMENTS "${ARG_ARGUMENTS}"
        LANGUAGE_CONTEXT "${language_context}"
        AOT_MODE FALSE
        PRLIMIT_OPTIONS "${ARG_PRLIMIT_OPTIONS}"
        DEPENDS "${DEPEND_LIST}"
    )
    add_dependencies(${suite} ${target}-gengc)

    if (PANDA_ENABLE_G1GC_TESTS)
    panda_add_test_run(
            FILE "${ARG_FILE}"
            TARGET ${target}-g1gc
            SUBDIR ${suite}-g1gc
            EXPECTED_STDOUT "${ARG_EXPECTED_STDOUT}"
            SEARCH_DEBUG_STDERR ${CUR_DEBUG_LOG_MESSAGE}
            RUNTIME_OPTIONS "${CUR_RUNTIME_OPTIONS}" "--compiler-enable-jit=false --gc-type=g1-gc"
            ARGUMENTS "${ARG_ARGUMENTS}"
            LANGUAGE_CONTEXT "${language_context}"
            AOT_MODE FALSE
            PRLIMIT_OPTIONS "${ARG_PRLIMIT_OPTIONS}"
            DEPENDS "${DEPEND_LIST}"
    )
    add_dependencies(${suite} ${target}-g1gc)
    endif()

endfunction()

# Add a test for verifier
#
# Usage:
#
#   add_verifier_test(
#       FILE <source>
#       [RUNTIME_OPTIONS <option>...]
#       [ARGUMENTS <argument>...]
#       [EXPECTED_STDOUT <out_value>]
#       [DEBUG_LOG_MESSAGE <msg_value>]
#   )
#
# Adds a test <source>
#
# Options:
#
# RUNTIME_OPTIONS
#   Runtime (interpreter and compiler) options
#
# GC_OPTIONS
#  Type of a gc
#
# VERIFER_OPTIONS
#   Verifier CLI additional options
#
# EXPECTED_STDOUT
#   Excepted value of test's stdout
#
# VERIFIER_DEBUG_LOG_MESSAGE
#   Message which is expected to present in stderr output with --log-level=debug
#
#
# VERIFIER_FAIL_TEST
#   Force verifier do not set exit code
#
function(add_verifier_test)
    set(prefix ARG)
    set(noValues VERIFIER_FAIL_TEST VERIFIER_ONLY_ENTRY_POINT)
    set(singleValues FILE VERIFIER_DEBUG_LOG_MESSAGE GC_OPTIONS)
    set(multiValues EXPECTED_STDOUT RUNTIME_OPTIONS VERIFIER_OPTIONS)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})


    if (NOT DEFINED ARG_FILE)
        message(FATAL_ERROR "Mandatory FILE argument is not defined.")
    endif()

    get_filename_component(target "${ARG_FILE}" NAME)
    get_filename_component(path "${ARG_FILE}" DIRECTORY)
    get_filename_component(suite ${path} NAME)

    # variable_watch(ARG_VERIFIER_OPTIONS)

    if(DEFINED ARG_VERIFIER_DEBUG_LOG_MESSAGE)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "--log-level=debug --log-components verifier")
    endif()

    set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS},show-status")

    if(DEFINED ARG_GC_OPTIONS)
        set(ARG_RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}" "${ARG_GC_OPTIONS}")
    endif()

    if(ARG_VERIFIER_FAIL_TEST)
        set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS},verifier-does-not-fail")
    endif()

    if(ARG_VERIFIER_ONLY_ENTRY_POINT)
        set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS},verify-only-entry-point")
    endif()

    set(ARG_VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS} --verification-debug-config-file ${CMAKE_CURRENT_SOURCE_DIR}/cts-generator/verifier.debug.config")

    set(DEPEND_LIST)

    set(language_context "panda-assembly")

    verifier_add_test_run(
        FILE "${ARG_FILE}"
        TARGET ${target}-verifier
        SUBDIR ${suite}-verifier
        RUNTIME_OPTIONS "${ARG_RUNTIME_OPTIONS}"
        VERIFIER_OPTIONS "${ARG_VERIFIER_OPTIONS}"
        EXPECTED_STDOUT "${ARG_EXPECTED_STDOUT}"
        SEARCH_DEBUG_STDERR ${ARG_VERIFIER_DEBUG_LOG_MESSAGE}
        LANGUAGE_CONTEXT "${language_context}"
        DEPENDS "${DEPEND_LIST}"
    )
    add_dependencies(${suite} ${target}-verifier)

endfunction()

add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/env-01.pa" ARGUMENTS -- 1 2 EXPECTED_STDOUT "1\n2\n" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-07.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-09.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-10.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-11.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-12.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-13.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-14.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-15.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-16.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-17.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-18.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-21.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-22.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-23.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-24.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-25.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-26.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-27.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-28.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-29.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-30.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-31.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-32.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-33.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-34.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-35.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-36.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-40.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-41.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-42.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-43.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-44.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-45.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-46.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-47.pa" EXPECTED_STDOUT "/ by zero" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-48.pa" EXPECTED_STDOUT "/ by zero" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-49.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-50.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-51.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-52.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-53.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-54.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-55.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-56.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-57.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-58.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-59.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-60.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-61.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-62.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-63.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-64.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-65.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-66.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/math-67.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-07.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-09.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-10.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-11.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-12.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-13.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-14.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-15.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-16.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-17.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-18.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-19.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-20.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-21.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-22.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-25.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-26.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-jeq-obj.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-jne-obj.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-jeqz-obj.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/op-jnez-obj.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-07.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-09.pa" CTS_TEST VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Verifier warning 4\"")
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-10.pa" CTS_TEST VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Verifier warning 4\"")
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-11.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-12.pa" EXPECTED_STDOUT "idx = 2; length = 1" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-13.pa" EXPECTED_STDOUT "idx = 2; length = 1" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/arrays-14.pa" EXPECTED_STDOUT "size = -1" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/exceptions-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/exceptions-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/exceptions-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-07.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-09.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-10.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-11.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-12.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-13.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-14.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-15.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-16.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-17.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-18.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/far-jump-19.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-05.pa" CTS_TEST)
#add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-07.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-09.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-10.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-11.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-12.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-13.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-14.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-15.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-16.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-17.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-18.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-23.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-24.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-25.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-26.pa" CTS_TEST VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Verifier warning 4\"")
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-27.pa" CTS_TEST VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Verifier warning 4\"")
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-28.pa" EXPECTED_STDOUT "R1 cannot be cast to R2" CTS_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Accumulator seems to be of incompatible type 'R1'.\"")
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-29.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-30.pa" EXPECTED_STDOUT "abstract method \"R.foo\"" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-31.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-32.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-33.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-34.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-35.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/obj-36.pa" CTS_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Accumulator seems to be of incompatible type 'R2'.\"")

add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-01.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-02.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-06.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-07.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-08.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-09.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-10.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-11.pa" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-12.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-13.pa" EXPECTED_STDOUT "23" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-14.pa" EXPECTED_STDOUT "24" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-15.pa" EXPECTED_STDOUT "26.6" CTS_TEST SKIP_VERIFICATION)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-16.pa" EXPECTED_STDOUT "26.6" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-17.pa" EXPECTED_STDOUT "27" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-18.pa" EXPECTED_STDOUT "28" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-19.pa" EXPECTED_STDOUT "asdf\n" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-20.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-21.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-22.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-23.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-24.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-25.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-26.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-28.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-29.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/intrinsics-f32-01.pa" CTS_TEST)

add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-01.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-02.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-03.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-04.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-05.pa" CTS_TEST)
add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-06.pa" CTS_TEST)

add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/initobj-bad-02.pa" CTS_TEST CTS_IGNORE_IR_FAILURES SKIP_VERIFICATION)

add_test_file(FILE "${CMAKE_CURRENT_SOURCE_DIR}/cts-assembly/big_ark_option_value.pa" RUNTIME_OPTIONS --code-pool-size=3221225472 CTS_TEST)

if (TARGET verifier-tests)
# Verifier regression tests
# Negative (must fail)
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_err_beyond_end_of_function.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Invalid last instruction in method, execution beyond the method code boundary.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_jump_on_exc_handler_from_body.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Details: 'Prohibited jump from method body to/into exception handler.'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_jump_out_of_body.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Jump offset is out of bounds of method body.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_execution_may_go_beyond_the_end.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Invalid last instruction in method, execution beyond the method code boundary.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_fallthrough_from_exc_handler_to_exc_handler.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Invalid last instruction of exception handler. Control flow may go into other exception handler.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_fallthrough_to_exc_handler_from_body.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Invalid instruction in the method body, execution may go into exception handler.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/cflow_jump_from_exc_handler_to_exc_handler.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Details: 'Prohibited jump out from exception handler detected.'\"")
# regression
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1926.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Cannot make a call to 'A::foo : i32 ( A )'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1940.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Cannot make a call to '_GLOBAL::nextVal : i32 ( i32 )' because of undefined register.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1863.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1745.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/issue_1163.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type: 'i64'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2084.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is undefined\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/issue_1981.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1697.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1702.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is undefined.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1813.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1826.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Inconsistency between type in register and field class type.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1827.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Inconsistency between type in register and field class type.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1834.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Field type is not expected one. Field is 'R.fi64 : i64'. Field type is 'i64'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1828.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Expected instance field.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2072.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2075.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v1 is of wrong type: 'NullRefType'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2085.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Inconsistency between type in register and field class type.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2086_1.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Expected instance field.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2086_2.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Inconsistency between type in register and field class type.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2088.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Field type is not expected one.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2090.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2107_1.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2107_2.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'i32'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2136.pa")
#add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2256.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2260.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type: 'i64'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2374.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Attempt to call static method 'A::func : void ( A )' as virtual.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/issue_964.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type: 'NullRefType'.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_1833.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Inconsistency between type in register and field class type.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/jumps_1.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: There was a type conflict on register v0. It cannot be used.\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/jumps_2.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type: 'NullRefType'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/jumps_3.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2089.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type: 'i32'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2702_1.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2702_2.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2702_3.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2702_4.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2740.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2787.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is undefined\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2816.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is of wrong type\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2817.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is of wrong type\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2818_1.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Register v0 is undefined\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2818_2.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"verifier: Accumulator is undefined\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_2921.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_3060.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_3133.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"Accumulator is of wrong type: 'f64'\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_3197.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"NPE exception will always be thrown here\"")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_3219.pa")
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/bug_3228.pa" VERIFIER_FAIL_TEST VERIFIER_DEBUG_LOG_MESSAGE "\"constant null in register.*NPE exception will always be thrown here\"")
# simple test for check option 'verify-only-entry-point'
add_verifier_test(FILE "${CMAKE_CURRENT_SOURCE_DIR}/verifier-tests/mr_3176.pa" VERIFIER_ONLY_ENTRY_POINT)
endif()
