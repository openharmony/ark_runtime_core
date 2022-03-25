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
# Convenience functions for testing Panda.

include(${CMAKE_CURRENT_LIST_DIR}/PandaCmakeFunctions.cmake)

function(common_add_gtest)
    set(prefix ARG)
    set(noValues CONTAINS_MAIN NO_CORES RAPIDCHECK_ON USE_CATCH2)
    set(singleValues NAME OUTPUT_DIRECTORY ENCLOSING_TARGET TSAN_EXTRA_OPTIONS PANDA_STD_LIB ARK_BOOTCLASSPATH)
    set(multiValues SOURCES INCLUDE_DIRS LIBRARIES SANITIZERS)

    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    if (ARG_RAPIDCHECK_ON AND DEFINED DONT_USE_RAPIDCHECK)
      return()
    endif()

    if (NOT DEFINED ARG_ENCLOSING_TARGET)
        message(FATAL_ERROR "Enclosing target is not defined")
    endif()
    if (NOT DEFINED ARG_OUTPUT_DIRECTORY)
        message(FATAL_ERROR "OUTPUT_DIRECTORY is not defined")
    endif()

    if(ARG_USE_CATCH2 AND NOT ARG_CONTAINS_MAIN)
        set(ARG_SOURCES ${ARG_SOURCES} ${PANDA_ROOT}/third_party/rapidcheck/test/main.cpp)
    endif()

    if (ARG_RAPIDCHECK_ON)
        panda_add_executable(${ARG_NAME} RAPIDCHECK_ON ${ARG_SOURCES})
        set_target_properties(${ARG_NAME} PROPERTIES LINK_FLAGS "-frtti -fexceptions")
        target_compile_definitions(${ARG_NAME} PRIVATE PANDA_RAPIDCHECK)
        target_compile_options(${ARG_NAME} PRIVATE "-frtti" "-fexceptions" "-fPIC")
        target_compile_definitions(${ARG_NAME} PUBLIC PANDA_RAPIDCHECK)
    else()
        panda_add_executable(${ARG_NAME} ${ARG_SOURCES})
    endif()

    if (ARG_USE_CATCH2)
        target_compile_definitions(${ARG_NAME} PUBLIC PANDA_CATCH2)
    else()
        target_compile_definitions(${ARG_NAME} PUBLIC PANDA_GTEST)
    endif()

    if(PANDA_CI_TESTING_MODE STREQUAL "Nightly")
        target_compile_definitions(${ARG_NAME} PUBLIC PANDA_NIGHTLY_TEST_ON)
    endif()
    # By default tests are just built, running is available either via an
    # umbrella target or via `ctest -R ...`. But one can always do something
    # like this if really needed:
    # add_custom_target(${ARG_NAME}_run
    #                  COMMENT "Run ${ARG_NAME}"
    #                  COMMAND ${CMAKE_CTEST_COMMAND}
    #                  DEPENDS ${ARG_NAME})
    if (ARG_USE_CATCH2)
        foreach(include_dir ${ARG_INCLUDE_DIRS} ${PANDA_ROOT}/third_party/rapidcheck/include)
            target_include_directories(${ARG_NAME} PUBLIC ${include_dir})
        endforeach()
    else()
        foreach(include_dir ${ARG_INCLUDE_DIRS} ${PANDA_THIRD_PARTY_SOURCES_DIR}/googletest/googlemock/include)
            target_include_directories(${ARG_NAME} PUBLIC ${include_dir})
        endforeach()
    endif()

    if (NOT ARG_USE_CATCH2)
        if (ARG_CONTAINS_MAIN)
            target_link_libraries(${ARG_NAME} gtest)
        else()
            target_link_libraries(${ARG_NAME} gtest_main)
        endif()
    endif()

    if (NOT (PANDA_TARGET_MOBILE OR PANDA_TARGET_OHOS))
       target_link_libraries(${ARG_NAME} pthread)
    endif()
    target_link_libraries(${ARG_NAME} ${ARG_LIBRARIES})
    add_dependencies(${ARG_ENCLOSING_TARGET} ${ARG_NAME})

    if (ARG_RAPIDCHECK_ON)
        target_link_libraries(${ARG_NAME} rapidcheck)
        target_link_libraries(${ARG_NAME} rapidcheck_catch)
        target_link_libraries(${ARG_NAME} rapidcheck_gtest)
        target_link_libraries(${ARG_NAME} rapidcheck_gmock)
        add_dependencies(${ARG_NAME} rapidcheck)
    endif()

    panda_add_sanitizers(TARGET ${ARG_NAME} SANITIZERS ${ARG_SANITIZERS})

    set(prlimit_prefix "")
    if (ARG_NO_CORES)
        set(prlimit_prefix prlimit --core=0)
    endif()
    set_target_properties(${ARG_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}")

    set(tsan_options "")
    if ("thread" IN_LIST PANDA_SANITIZERS_LIST)
        if (DEFINED ENV{TSAN_OPTIONS})
            set(tsan_options "TSAN_OPTIONS=$ENV{TSAN_OPTIONS},${ARG_TSAN_EXTRA_OPTIONS}")
        endif()
    endif()

    # Yes, this is hardcoded. No, do not ask for an option. Go and fix your tests.
    if (PANDA_CI_TESTING_MODE STREQUAL "Nightly")
        set(timeout_prefix timeout --preserve-status --signal=USR1 --kill-after=10s 40m)
    else ()
        set(timeout_prefix timeout --preserve-status --signal=USR1 --kill-after=10s 20m)
    endif()

    if (ARG_RAPIDCHECK_ON)
        add_custom_target(${ARG_NAME}_rapidcheck_tests
                          COMMAND ${tsan_options} ${prlimit_prefix} ${timeout_prefix}
                                  ${PANDA_RUN_PREFIX} "${ARG_OUTPUT_DIRECTORY}/${ARG_NAME}"
                          DEPENDS ${ARG_ENCLOSING_TARGET}
        )
        add_dependencies(gtests ${ARG_NAME}_rapidcheck_tests)
    else()
        set(PANDA_STD_LIB "")
        if (DEFINED ARG_PANDA_STD_LIB)
            set(PANDA_STD_LIB "PANDA_STD_LIB=${ARG_PANDA_STD_LIB}")
        endif()
        set(ARK_BOOTCLASSPATH "")
        if (DEFINED ARG_ARK_BOOTCLASSPATH)
            set(ARK_BOOTCLASSPATH "ARK_BOOTCLASSPATH=${ARG_ARK_BOOTCLASSPATH}")
        endif()
        add_custom_target(${ARG_NAME}_gtests
                          COMMAND ${PANDA_STD_LIB} ${ARK_BOOTCLASSPATH}
                                  ${tsan_options} ${prlimit_prefix} ${timeout_prefix}
                                  ${PANDA_RUN_PREFIX} "${ARG_OUTPUT_DIRECTORY}/${ARG_NAME}"
                                  --gtest_shuffle --gtest_death_test_style=threadsafe
                          DEPENDS ${ARG_ENCLOSING_TARGET}
       )
       add_dependencies(gtests ${ARG_NAME}_gtests)
    endif()
endfunction()
