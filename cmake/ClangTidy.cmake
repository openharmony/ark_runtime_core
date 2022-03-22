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

option(PANDA_ENABLE_CLANG_TIDY "Enable clang-tidy checks during compilation" true)

# There seems to be a bug in either clang-tidy or CMake:
# When clang/gcc is used for cross-compilation, it is ran on host and use definitions and options for host
# For example for arm32 cross-compilation Clang-Tidy:
#   - don't know about -march=armv7-a
#   - believes that size of pointer is 64 instead of 32 for aarch32
if(CMAKE_CROSSCOMPILING)
    set(PANDA_ENABLE_CLANG_TIDY false)
endif()

if(PANDA_TARGET_MACOS)
    set(PANDA_ENABLE_CLANG_TIDY false)
endif()

if(PANDA_ENABLE_CLANG_TIDY)
    # Currently we fix a certain version of clang-tidy to avoid unstable linting,
    # which may occur if different versions of the tools are used by developers.
    set(panda_clang_tidy "clang-tidy-9")

    # Require clang-tidy
    find_program(
        CLANG_TIDY
        NAMES ${panda_clang_tidy}
        DOC "Path to clang-tidy executable")
    if(NOT CLANG_TIDY)
        message(FATAL_ERROR "clang-tidy not found, but requested for build. Use -DPANDA_ENABLE_CLANG_TIDY=false to suppress.")
    endif()

    unset(panda_clang_tidy)

    message(STATUS "clang-tidy found: ${CLANG_TIDY}")
    # NB! Even if config is malformed, clang-tidy -dump-config returns 0 on failure.
    # Hence we check for ERROR_VARIABLE instead of RESULT_VARIABLE.
    execute_process(
        COMMAND ${CLANG_TIDY} -dump-config
        WORKING_DIRECTORY ${PANDA_ROOT}
        ERROR_VARIABLE dump_config_stderr)
    if (dump_config_stderr AND NOT "${dump_config_stderr}" STREQUAL "")
        message(FATAL_ERROR "${dump_config_stderr}")
    endif()
    # See https://gitlab.kitware.com/cmake/cmake/issues/18926.
    # Create a preprocessor definition that depends on .clang-tidy content so
    # the compile command will change when .clang-tidy changes.  This ensures
    # that a subsequent build re-runs clang-tidy on all sources even if they
    # do not otherwise need to be recompiled.  Nothing actually uses this
    # definition.  We add it to targets on which we run clang-tidy just to
    # get the build dependency on the .clang-tidy file.
    file(SHA1 ${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy clang_tidy_sha1)
    set(CLANG_TIDY_DEFINITIONS "CLANG_TIDY_SHA1=${clang_tidy_sha1}")
    unset(clang_tidy_sha1)
    configure_file(${PANDA_ROOT}/.clang-tidy ${PANDA_BINARY_ROOT}/.clang-tidy COPYONLY)
endif()

# Add a target to clang-tidy checks.
#
# Example usage:
#
#   panda_add_to_clang_tidy(TARGET target_name
#     CHECKS
#       "-check-to-be-disabled"
#       "-glob-to-be-disabled-*"
#       "check-to-be-enabled"
#       "glob-to-be-enabled-*"
#   )
#
# This function makes target_name to be co-compiled with clang-tidy.
# The list of CHECKS allows to pass per-target checks in additions to
# global ones (see below). CHECKS follow clang-tidy syntax of checks.
# By default all checks are enabled globally, so the most reasonable use
# case for CHECKS is to pass checks to be suppressed.

# NB! Important caveats:
# * We use permissive policy for checks, i.e. everything is enabled by default,
#   then exceptions are suppressed explicitly.
# * We maintain the list of global exceptions in this function (not in .clang-tidy)
#   because of its syntax limitations (in particular, all checks should be passed
#   as a single string in YAML, which is not readable).
# * -header-filter is set to check only headers of the target_name. It is supposed
#   that each components is responsible for QA'ing only its code base.
function(panda_add_to_clang_tidy)
    if(NOT PANDA_ENABLE_CLANG_TIDY)
        return()
    endif()

    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET)
    set(multiValues CHECKS)

    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})

    set(clang_tidy_params
        "${CLANG_TIDY}"
        "-config="
        "-format-style=file"
        "-header-filter='^(${CMAKE_SOURCE_DIR}|${CMAKE_BINARY_DIR}).*/(assembler|compiler|debugger|libpandabase|libpandafile|runtime|class2panda)/.*'"
        "-p='${PANDA_BINARY_ROOT}'"
    )

    set(clang_tidy_default_exceptions
        # aliases for other checks(here full list: https://clang.llvm.org/extra/clang-tidy/checks/list.html):
        "-hicpp-braces-around-statements"  # alias for readability-braces-around-statements
        "-google-readability-braces-around-statements"  # alias for readability-braces-around-statements
        "-google-readability-function-size"  # alias for readability-function-size
        "-hicpp-explicit-conversions"  # alias for google-explicit-constructor
        "-hicpp-function-size"  # alias for readability-function-size
        "-hicpp-no-array-decay"  # alias for cppcoreguidelines-pro-bounds-array-to-pointer-decay
        "-hicpp-avoid-c-arrays"  # alias for modernize-avoid-c-arrays
        "-cppcoreguidelines-avoid-c-arrays"  # alias for modernize-avoid-c-arrays
        "-cppcoreguidelines-avoid-magic-numbers"  # alias for readability-magic-numbers
        "-cppcoreguidelines-non-private-member-variables-in-classes" # alias for misc-non-private-member-variables-in-classes
        "-cert-dcl03-c"  # alias for misc-static-assert
        "-hicpp-static-assert"  # alias for misc-static-assert
        "-hicpp-no-malloc"  # alias for cppcoreguidelines-no-malloc
        "-hicpp-vararg"  # alias for cppcoreguidelines-pro-type-vararg
        "-hicpp-member-init" # alias for cppcoreguidelines-pro-type-member-init
        "-hicpp-move-const-arg" # alias for performance-move-const-arg
        # explicitly disabled checks
        "-bugprone-macro-parentheses"  # disabled because it is hard to write macros with types with it
        "-llvm-header-guard"  # disabled because of incorrect root prefix
        "-llvm-include-order"  # disabled because conflicts with the clang-format
        "-readability-identifier-naming" # disabled because we will use little-hump-style
        "google-runtime-references" # disabled to use non-const references
        "-fuchsia-trailing-return"  # disabled because we have a lot of false positives and it is stylistic check
        "-fuchsia-default-arguments-calls" # disabled because we use functions with default arguments a lot
        "-fuchsia-default-arguments-declarations" # disabled because we use functions with default arguments a lot
        "-modernize-use-trailing-return-type" # disabled as a stylistic check
        "-clang-analyzer-optin.cplusplus.UninitializedObject" # disabled due to instability on clang-9 and clang-10
        "-readability-static-accessed-through-instance"
        "-readability-convert-member-functions-to-static"
        "-bugprone-sizeof-expression"
        "-bugprone-branch-clone"
        "-cppcoreguidelines-owning-memory"
        "-cppcoreguidelines-pro-bounds-array-to-pointer-decay"
        "-cppcoreguidelines-pro-bounds-constant-array-index"
        "-cppcoreguidelines-pro-type-const-cast"
        "-cppcoreguidelines-pro-type-reinterpret-cast"
        "-cppcoreguidelines-pro-type-static-cast-downcast"
        "-fuchsia-default-arguments"
        "-fuchsia-overloaded-operator"
        "-modernize-use-nodiscard"
        "-cert-dcl50-cpp"  # ailas for cppcoreguidelines-pro-type-vararg
        # candidates for removal:
        "-hicpp-noexcept-move"  # For some reason become failed in DEFAULT_MOVE_SEMANTIC
        "-performance-noexcept-move-constructor"  # Same as above
    )
    # NB! Replace with list(JOIN ...) after switching to CMake 3.12+
    string(REPLACE ";" "," default_exceptions "${clang_tidy_default_exceptions}")

    set(clang_tidy_checks "-checks=*,${default_exceptions}")

    if(DEFINED ARG_CHECKS)
        # NB! Replace with list(JOIN ...) after switching to CMake 3.12+
        string(REPLACE ";" "," custom_checks "${ARG_CHECKS}")
        set(clang_tidy_checks "${clang_tidy_checks},${custom_checks}")
    endif()

    list(APPEND clang_tidy_params "${clang_tidy_checks}")

    set_target_properties(${ARG_TARGET} PROPERTIES CXX_CLANG_TIDY "${clang_tidy_params}")
endfunction()
