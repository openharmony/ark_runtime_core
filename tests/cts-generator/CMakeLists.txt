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

cmake_minimum_required(VERSION 3.5.2 FATAL_ERROR)

if(NOT CMAKE_CROSSCOMPILING)

    set(CMAKE_VERBOSE_MAKEFILE ON)

    project(cts_generator)

    if(NOT PANDA_CTS_JOBS_NUMBER)
        if(PANDA_ENABLE_THREAD_SANITIZER)
          # Decrease threads for build with TSAN to deal with increased memory usage
          # 04.12.20 set to 16 to check behavior
          set(PANDA_CTS_JOBS_NUMBER 16)
        else()
          set(PANDA_CTS_JOBS_NUMBER 16)
        endif()
    endif()

    set(CTS_TSAN_TEST_SELECTION "--include-tag tsan")

    # Set timeout for the entire CTS test suite
    if(NOT PANDA_CTS_TESTING_TIMEOUT)
        # Increase timeout for build with sanitizers
        if(PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER OR PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_THREAD_SANITIZER)
            set(PANDA_CTS_TESTING_TIMEOUT 3000)
        else()
            set(PANDA_CTS_TESTING_TIMEOUT 1000)
        endif()
    endif()

    # Set timeout for each single test
    if(NOT PANDA_CTS_TEST_TIMEOUT)
        # Increase single test timeout for build with sanitizers
        if(PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER OR PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_THREAD_SANITIZER)
            set(PANDA_CTS_TEST_TIMEOUT 200)   # 200 seconds
        else()
            set(PANDA_CTS_TEST_TIMEOUT 30)    # 30 seconds
        endif()
    endif()

    # Source files:
    set(CTS_TEMPLATE_DIR_SRC "${PROJECT_SOURCE_DIR}/cts-template")
    set(CTS_TEMPLATE "${PROJECT_BINARY_DIR}/cts-template/template.yaml")
    set(CTS_TEMPLATE_SCHEMA "${PROJECT_BINARY_DIR}/cts-template/yaml-schema.json")
    set(CTS_GENERATED_DIR "${PROJECT_BINARY_DIR}/cts-generated/")
    set(VERIFIER_DEBUG_CONFIG "${PROJECT_SOURCE_DIR}/verifier.debug.config")

    file(
        COPY "${CTS_TEMPLATE_DIR_SRC}"
        DESTINATION "${PROJECT_BINARY_DIR}"
    )

    file(
        COPY "${VERIFIER_DEBUG_CONFIG}"
        DESTINATION "${PROJECT_BINARY_DIR}"
    )

    # Generated files:
    set(CTS_GENERATED "${PROJECT_BINARY_DIR}/cts-generated")
    set(CTS_REPORT_RUN "${PROJECT_BINARY_DIR}/cts-generated/report.txt")

    set(OUTPUT_FILES ${CTS_GENERATED})
    set(OUTPUT_RUN ${CTS_REPORT_RUN})

    # CTS Generator
    set(CTS_GENERATOR "${PROJECT_SOURCE_DIR}/generate-cts.rb")
    set(CTS_TEST_RUNNER "${PROJECT_SOURCE_DIR}/test-runner.rb")

    # CTS_TEST_SELECT_OPTION is not defined, use predefined value for exclude tag and include tag options.
    if (NOT DEFINED CTS_TEST_SELECT_OPTION)
      if (CMAKE_BUILD_TYPE STREQUAL "Release")
        if (PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER OR PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_THREAD_SANITIZER)
          set(CTS_TEST_SELECT_OPTION "--exclude-tag clang_release_sanitizer")
        else()
          set(CTS_TEST_SELECT_OPTION "--exclude-tag release")
        endif()
      else()
        set(CTS_TEST_SELECT_OPTION " ")
      endif()
    endif()

    # CTS_TEST_SELECT_OPTION_JIT is not defined, use predefined value for exclude tag and include tag options.
    if (NOT DEFINED CTS_TEST_SELECT_OPTION_JIT)
      if (CMAKE_BUILD_TYPE STREQUAL "Release")
        if (PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER OR PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_THREAD_SANITIZER)
          set(CTS_TEST_SELECT_OPTION_JIT "--exclude-tag clang_release_sanitizer --exclude-tag clang_release_sanitizer_jit")
        else()
          set(CTS_TEST_SELECT_OPTION_JIT "--exclude-tag release")
        endif()
      else()
        set(CTS_TEST_SELECT_OPTION_JIT " ")
      endif()
    endif()

    # Limit tests for sanitizer build
    if (PANDA_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER OR PANDA_ENABLE_ADDRESS_SANITIZER OR PANDA_ENABLE_THREAD_SANITIZER)
        set(CTS_TEST_SELECT_OPTION "${CTS_TEST_SELECT_OPTION} ${CTS_TSAN_TEST_SELECTION}")
        set(CTS_TEST_SELECT_OPTION_JIT "${CTS_TEST_SELECT_OPTION_JIT} ${CTS_TSAN_TEST_SELECTION}")
    endif()

    if (NOT DEFINED CTS_TEST_PRLIMIT_OPTIONS)
      if (PANDA_ENABLE_ADDRESS_SANITIZER)
        set(CTS_TEST_PRLIMIT_OPTIONS "--prlimit='--stack=100000000'")
      endif()
    endif()

    # Exclude Arm-specific failures
    if ((PANDA_TARGET_ARM32) OR (PANDA_TARGET_ARM64))
      set(CTS_TEST_SELECT_OPTION_JIT "${CTS_TEST_SELECT_OPTION_JIT} --exclude-tag arm64-fail --exclude-tag arm64-jit")
    endif()

    add_custom_command(OUTPUT ${OUTPUT_FILES}
        COMMENT "Generate CTS files"
        COMMAND ${CTS_GENERATOR}
            -t "${CTS_TEMPLATE}"
            -s "${CTS_TEMPLATE_SCHEMA}"
            -o "${CTS_GENERATED_DIR}"
            -k
        DEPENDS "$<TARGET_FILE:ark_asm>" "$<TARGET_FILE:ark>"
    )

    # Convert arg string to separate args
    string(REPLACE " " ";" CTS_TEST_SELECT_OPTION_ARGS ${CTS_TEST_SELECT_OPTION})
    string(REPLACE " " ";" CTS_TEST_SELECT_OPTION_JIT_ARGS ${CTS_TEST_SELECT_OPTION_JIT})

    # Run tests without enforced JIT and with enforced JIT
    add_custom_command(OUTPUT ${OUTPUT_RUN}
        COMMENT "Run generated CTS tests"
        COMMAND ${CTS_TEST_RUNNER}
             -t "${CTS_GENERATED_DIR}"
             -p "${PROJECT_BINARY_DIR}/../../"
             -v 1
             --global-timeout ${PANDA_CTS_TESTING_TIMEOUT}
             --timeout ${PANDA_CTS_TEST_TIMEOUT}
             -j ${PANDA_CTS_JOBS_NUMBER}
             --verifier-debug-config "${PROJECT_BINARY_DIR}/verifier.debug.config"
             ${CTS_TEST_PRLIMIT_OPTIONS}
             ${CTS_TEST_SELECT_OPTION_ARGS} VERBATIM
        COMMAND ${CTS_TEST_RUNNER}
            -t "${CTS_GENERATED_DIR}"
            -p "${PROJECT_BINARY_DIR}/../../"
            -v 1
            --global-timeout ${PANDA_CTS_TESTING_TIMEOUT}
            --timeout ${PANDA_CTS_TEST_TIMEOUT}
            -j ${PANDA_CTS_JOBS_NUMBER}
            --verifier-debug-config "${PROJECT_BINARY_DIR}/verifier.debug.config"
            -o "--compiler-hotness-threshold=0"
            -o "--no-async-jit"
            ${CTS_TEST_PRLIMIT_OPTIONS}
            --exclude-tag verifier
            ${CTS_TEST_SELECT_OPTION_JIT_ARGS} VERBATIM
        DEPENDS "$<TARGET_FILE:ark_asm>" "$<TARGET_FILE:ark>" "${OUTPUT_FILES}"
    )

    if (PANDA_EXPORT_CTS_OPTIONS)
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cts-options" "CTS_TEST_SELECT_OPTION_ARGS=\"${CTS_TEST_SELECT_OPTION}\"\n")
      file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/cts-options" "CTS_TEST_SELECT_OPTION_JIT_ARGS=\"${CTS_TEST_SELECT_OPTION_JIT}\"\n")
      file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/cts-options" "CTS_TEST_PRLIMIT_OPTIONS=\"${CTS_TEST_PRLIMIT_OPTIONS}\"\n")
    endif()

    add_custom_target(cts-generator-build DEPENDS ${OUTPUT_FILES})
    add_custom_target(cts-generator DEPENDS cts-generator-build ${OUTPUT_RUN})

    # Added dependency on gtests - this is necessary to avoid memory allocation problem during test-runner work.
    # At this moment (10/22/2020) JNI test can consume all available memory during build on clang 8 with thread safety analysis.
    # So this fix is workaround for this problem to allow test-runner successfully work.
    # This dependency can be removed and only arkstdlib java_libbase can be kept as dependency after JNI issue will be fixed.
    if (TARGET gtests)
      if(PANDA_ENABLE_THREAD_SANITIZER)
        # 04.12.20 add dependency in gtests only for build with thread sanitizers
        add_dependencies(cts-generator-build gtests arkstdlib)
      else()
        add_dependencies(cts-generator-build arkstdlib)
      endif()
    else()
      add_dependencies(cts-generator-build arkstdlib)
    endif()

endif()
