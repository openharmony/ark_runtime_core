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

# Add a panda assembly to the project using the specified source file
#
# Example usage:
#
#   add_panda_assembly(TARGET <name> SOURCE <source> INDIR <input directory> OUTDIR <output directory> TARGETNAME <target file name>)
#
# Adds a panda assembly target called <name> to be build from <source> file
# listed in the command invocation.
function(add_panda_assembly)
    set(prefix ARG)
    set(noValues)
    set(singleValues TARGET SOURCE INDIR OUTDIR TARGETNAME)
    set(multiValues)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                          ${ARGN})
    if (NOT DEFINED ARG_TARGET)
        message(FATAL_ERROR "Mandatory TARGET argument is not defined.")
    endif()

    if (NOT DEFINED ARG_SOURCE)
        message(FATAL_ERROR "Mandatory SOURCE argument is not defined.")
    endif()

    set(source_file_dir "${CMAKE_CURRENT_SOURCE_DIR}")

    if (DEFINED ARG_INDIR)
        set(source_file_dir "${ARG_INDIR}")
    endif()

    set(binary_file_dir "${CMAKE_CURRENT_BINARY_DIR}")

    if (DEFINED ARG_OUTDIR)
        set(binary_file_dir "${ARG_OUTDIR}")
    endif()

    set(target_file_name "${ARG_TARGET}")

    if (DEFINED ARG_TARGETNAME)
        set(target_file_name "${ARG_TARGETNAME}")
    endif()

    if (TARGET ARG_TARGET)
        message(FATAL_ERROR "Target ${ARG_TARGET} is already defined.")
    endif()

    set(source_file "${source_file_dir}/${ARG_SOURCE}")
    set(binary_file "${binary_file_dir}/${target_file_name}.abc")

    if(CMAKE_CROSSCOMPILING)
        ExternalProject_Get_Property(panda_host_tools binary_dir)
        set(assembler_target panda_host_tools)
        set(assembler_bin    "${binary_dir}/assembler/ark_asm")
    else()
        set(assembler_target ark_asm)
        set(assembler_bin    $<TARGET_FILE:${assembler_target}>)
    endif()

    add_custom_target(${ARG_TARGET}
                       COMMENT "Building ${ARG_TARGET}"
                       COMMAND "${assembler_bin}" "${source_file}" "${binary_file}"
                       DEPENDS ${assembler_target} "${source_file}")
endfunction()

# Add a single buildable and runnable Panda Assembly file to the build tree.
#
# Usage:
#
#   panda_add_asm_file(
#        FILE <source>
#        TARGET <target>
#        [ENTRY <entry_point>]
#        [SUBDIR <subdir>]
#        [OUTPUT_FILE_VARIABLE <variable>]
#        [ERROR_FILE_VARIABLE <variable>]
#        [SKIP_BUILD TRUE|FALSE]
#        [AOT_MODE TRUE|FALSE]
#        [DEPENDS <dependency>...]
#        [RUNTIME_OPTIONS <option>...]
#        [COMPILER_OPTIONS <option>...]
#        [GC_OPTIONS <option>]
#        [ENTRY_ARGUMENTS <argument>...]
#        [TIMEOUT <timeout>]
#        [LANGUAGE_CONTEXT <language>]
#    )
#
# Adds a target <target> which assembles the assembly file <source>
# with Panda assembler and runs it with Panda interpreter.
#
# Options:
#
# ENTRY
#   Entry point to execute in format <Class>::<method>. By default _GLOBAL::main is used
#
# SUBDIR
#   Subdirectory in the current binary directory that is used to store build artifacts.
#   Full path to directory with artifacts: ${CMAKE_CURRENT_BINARY_DIR}/<subdir>/<target>
#
# OUTPUT_FILE_VARIABLE, ERROR_FILE_VARIABLE
#   The variable named will be set with the paths to files with contents of the stdout and
#   stderr of the program respectively
#
# DEPENDS
#   List of additional dependences (exclude assembler and interpreter)
#
# RUNTIME_OPTIONS
#   Runtime options
#
# COMPILER_OPTIONS
#   Options for compiler, given both to panda and paoc
#
# GC_OPTIONS
#  Type of a gc
#
# ENTRY_ARGUMENTS
#   List of arguments that will be passed to program's entry point
#
# TIMEOUT
#   If specified, the program will be run and terminated with the signal 10 (corresponds
#   to SIGUSR1 on most platforms) after the given timeout. The format of the value
#   must match the `timeout` command. The run will be considered successful if the program
#   exits before the timeout with the successful exit code or if it is terminated
#   after the timeout with the signal 10.
#
# LANGUAGE_CONTEXT
#   Set the language-dependent semantics for the code. Possible values: panda-assembly.
#
function(panda_add_asm_file)
    set(prefix ARG)
    set(noValues)
    set(singleValues FILE ENTRY TARGET SUBDIR OUTPUT_FILE_VARIABLE ERROR_FILE_VARIABLE SKIP_BUILD AOT_MODE TIMEOUT LANGUAGE_CONTEXT GC_OPTIONS)
    set(multiValues DEPENDS RUNTIME_OPTIONS COMPILER_OPTIONS ENTRY_ARGUMENTS PRLIMIT_OPTIONS ADDITIONAL_STDLIBS)
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

    set(build_dir   "${CMAKE_CURRENT_BINARY_DIR}/${ARG_SUBDIR}/${ARG_TARGET}")
    set(build_log   "${build_dir}/build.log")
    set(build_out   "${build_dir}/build.out")
    set(build_err   "${build_dir}/build.err")
    set(binary_file "${build_dir}/test.abc")
    set(launch_file "${build_dir}/launch.sh")
    set(output_file "${build_dir}/run.out")
    set(error_file  "${build_dir}/run.err")

    file(MAKE_DIRECTORY "${build_dir}")

    if (DEFINED ARG_OUTPUT_FILE_VARIABLE)
        set(${ARG_OUTPUT_FILE_VARIABLE} "${output_file}" PARENT_SCOPE)
    endif()

    if (DEFINED ARG_ERROR_FILE_VARIABLE)
        set(${ARG_ERROR_FILE_VARIABLE} "${error_file}" PARENT_SCOPE)
    endif()

    if (ARG_SKIP_BUILD)
        set(binary_file "${ARG_FILE}")
    else()
        set(assembler ark_asm)
        add_custom_command(OUTPUT "${binary_file}"
                          COMMENT "Building ${ARG_TARGET}"
                          COMMAND ${PANDA_RUN_PREFIX} $<TARGET_FILE:${assembler}> --verbose --log-file "${build_log}" "${ARG_FILE}" "${binary_file}" 1>"${build_out}" 2>"${build_err}"
                          DEPENDS ${assembler} "${ARG_FILE}")
    endif()

    if (DEFINED ARG_TIMEOUT AND NOT "${ARG_TIMEOUT}" STREQUAL "")
        set(timeout_signal 10)
        set(timeout_prefix "timeout --preserve-status --signal=${timeout_signal} --kill-after=10s ${ARG_TIMEOUT}")
        set(timeout_suffix "|| [ `expr \$? % 128` -eq ${timeout_signal} ]")
    else()
        set(timeout_prefix "")
        set(timeout_suffix "")
    endif()

    if (NOT DEFINED ARG_ENTRY)
        set(ARG_ENTRY "_GLOBAL::main")
    endif()

    set(panda_stdlib   arkstdlib)
    set(panda_cli      ark)
    set(panda_verifier verifier)

    set(stdlibs "${${panda_stdlib}_BINARY_DIR}/${panda_stdlib}.abc")
    if (DEFINED ARG_ADDITIONAL_STDLIBS)
        list(APPEND stdlibs ${ARG_ADDITIONAL_STDLIBS})
    endif()

    set(spaces  "core")
    set(runtime_type "core")
    if (DEFINED ARG_LANGUAGE_CONTEXT AND NOT "${ARG_LANGUAGE_CONTEXT}" STREQUAL "panda-assembly")
        list(APPEND spaces "${ARG_LANGUAGE_CONTEXT}")
        set(runtime_type "${ARG_LANGUAGE_CONTEXT}")
    endif()

    string(REPLACE ";" ":" boot_stdlibs "${stdlibs}")
    string(REPLACE ";" ":" boot_spaces  "${spaces}")

    # Note well! The lines below imply that we cannot handle ";" properly
    # in both Panda's own options and the running program's options.
    string(REPLACE ";" " " runtime_options "${ARG_COMPILER_OPTIONS} ${ARG_RUNTIME_OPTIONS}")
    string(REPLACE ";" " " entry_arguments "${ARG_ENTRY_ARGUMENTS}")

    set(prlimit_cmd "")
    if (DEFINED ARG_PRLIMIT_OPTIONS)
        set(prlimit_cmd "prlimit ${ARG_PRLIMIT_OPTIONS}")
        string(REPLACE ";" " " prlimit_cmd "${prlimit_cmd}")
    endif()

    if (${runtime_options} MATCHES ".*events-output=csv.*")
        set(runtime_options "${runtime_options} --events-file=${build_dir}/events.csv")
    endif()

    set(launcher
        "${timeout_prefix}"
        "${prlimit_cmd}"
        "${PANDA_RUN_PREFIX}"
        $<TARGET_FILE:${panda_cli}>
        "--boot-panda-files \"${boot_stdlibs}\""
        "--boot-intrinsic-spaces=${boot_spaces}"
        "--boot-class-spaces=${boot_spaces}"
        "--runtime-type=${runtime_type}"
        "${runtime_options}"
        "\"${binary_file}\""
        "\"${ARG_ENTRY}\""
        "${entry_arguments}"
        "1>\"${output_file}\""
        "2>\"${error_file}\""
        "${timeout_suffix}"
    )
    string(REPLACE ";" " " launcher "${launcher}")
    file(GENERATE OUTPUT ${launch_file} CONTENT "${launcher}")

    add_custom_target(${ARG_TARGET}
                      COMMENT "Running ${ARG_TARGET}"
                      COMMAND . ${launch_file} || (cat ${error_file} && false)
                      DEPENDS ${panda_cli} ${panda_stdlib} "${binary_file}" ${aot_file})

    if (DEFINED ARG_DEPENDS)
        add_dependencies(${ARG_TARGET} ${ARG_DEPENDS})
    endif()


endfunction()

# Add a single buildable and verifiable Panda Assembly file to the build tree.
#
# Usage:
#
#   verifier_add_asm_file(
#        FILE <source>
#        TARGET <target>
#        [ENTRY <entry_point>]
#        [RUNTIME_OPTIONS <runtime options>]
#        [VERIFIER_OPTIONS <verifier options>]
#        [SUBDIR <subdir>]
#        [OUTPUT_FILE_VARIABLE <variable>]
#        [ERROR_FILE_VARIABLE <variable>]
#        [DEPENDS <dependency>...]
#        [TIMEOUT <timeout>]
#        [LANGUAGE_CONTEXT <language>]
#    )
#
# Adds a target <target> which assembles the assembly file <source>
# with Panda assembler and verifies it with verifier.
#
# Options:
#
# ENTRY
#   Entry point to execute in format <Class>::<method>. By default _GLOBAL::main is used
#
# SUBDIR
#   Subdirectory in the current binary directory that is used to store build artifacts.
#   Full path to directory with artifacts: ${CMAKE_CURRENT_BINARY_DIR}/<subdir>/<target>
#
# OUTPUT_FILE_VARIABLE, ERROR_FILE_VARIABLE
#   The variable named will be set with the paths to files with contents of the stdout and
#   stderr of the program respectively
#
# DEPENDS
#   List of additional dependences (exclude assembler and interpreter)
#
# RUNTIME_OPTIONS
#   Runtime initialization options
#
# VERIFIER_OPTIONS
#   Verifier CLI options
#
# TIMEOUT
#   If specified, the program will be run and terminated with the signal 10 (corresponds
#   to SIGUSR1 on most platforms) after the given timeout. The format of the value
#   must match the `timeout` command. The run will be considered successful if the program
#   exits before the timeout with the successful exit code or if it is terminated
#   after the timeout with the signal 10.
#
# LANGUAGE_CONTEXT
#   Set the language-dependent semantics for the code. Possible values: panda-assembly.
#
function(verifier_add_asm_file)
    set(prefix ARG)
    set(noValues)
    set(singleValues FILE ENTRY TARGET SUBDIR OUTPUT_FILE_VARIABLE ERROR_FILE_VARIABLE TIMEOUT LANGUAGE_CONTEXT)
    set(multiValues DEPENDS RUNTIME_OPTIONS VERIFIER_OPTIONS)
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

    set(build_dir   "${CMAKE_CURRENT_BINARY_DIR}/${ARG_SUBDIR}/${ARG_TARGET}")
    set(build_out   "${build_dir}/build.out")
    set(build_err   "${build_dir}/build.err")
    set(binary_file "${build_dir}/test.abc")
    set(launch_file "${build_dir}/verifier_launch.sh")
    set(output_file "${build_dir}/verify.out")
    set(error_file  "${build_dir}/verify.err")

    file(MAKE_DIRECTORY "${build_dir}")

    if (DEFINED ARG_OUTPUT_FILE_VARIABLE)
        set(${ARG_OUTPUT_FILE_VARIABLE} "${output_file}" PARENT_SCOPE)
    endif()

    if (DEFINED ARG_ERROR_FILE_VARIABLE)
        set(${ARG_ERROR_FILE_VARIABLE} "${error_file}" PARENT_SCOPE)
    endif()

    set(assembler ark_asm)
    add_custom_command(OUTPUT "${binary_file}"
                      COMMENT "Building ${ARG_TARGET}"
                      COMMAND ${PANDA_RUN_PREFIX} $<TARGET_FILE:${assembler}> "${ARG_FILE}" "${binary_file}" 1>"${build_out}" 2>"${build_err}"
                      DEPENDS ${assembler} "${ARG_FILE}")

    if (DEFINED ARG_TIMEOUT AND NOT "${ARG_TIMEOUT}" STREQUAL "")
        set(timeout_signal 10)
        set(timeout_prefix "timeout --preserve-status --signal=${timeout_signal} --kill-after=10s ${ARG_TIMEOUT}")
        set(timeout_suffix "|| [ `expr \$? % 128` -eq ${timeout_signal} ]")
    else()
        set(timeout_prefix "")
        set(timeout_suffix "")
    endif()

    if (NOT DEFINED ARG_ENTRY)
        set(ARG_ENTRY "_GLOBAL::main")
    endif()

    set(panda_stdlib arkstdlib)
    set(verifier_cli ark)

    set(stdlibs "${${panda_stdlib}_BINARY_DIR}/${panda_stdlib}.abc")
    set(spaces  "core")
    set(runtime_type "core")
    if (NOT DEFINED ARG_LANGUAGE_CONTEXT)
        set(ARG_LANGUAGE_CONTEXT "panda-assembly")
    endif()

    string(REPLACE ";" ":" boot_stdlibs "${stdlibs}")
    string(REPLACE ";" ":" boot_spaces  "${spaces}")

    if(NOT "${ARG_VERIFIER_OPTIONS}" STREQUAL "")
        set(ARG_VERIFIER_OPTIONS ",${ARG_VERIFIER_OPTIONS}")
    endif()

    set(launcher_verifier
        "${PANDA_RUN_PREFIX}"
        $<TARGET_FILE:${verifier_cli}>
        "--verification-enabled"
        "--verification-options only-verify${ARG_VERIFIER_OPTIONS}"
        "${ARG_RUNTIME_OPTIONS}"
        "--boot-panda-files \"${boot_stdlibs}\""
        "--boot-intrinsic-spaces=${boot_spaces}"
        "--boot-class-spaces=${boot_spaces}"
        "--runtime-type=${runtime_type}"
        "\"${binary_file}\""
        "\"${ARG_ENTRY}\""
        "1>\"${output_file}\""
        "2>\"${error_file}\""
    )
    string(REPLACE ";" " " launcher_verifier "${launcher_verifier}")
    file(GENERATE OUTPUT ${launch_file} CONTENT "${launcher_verifier}")

    add_custom_target(${ARG_TARGET}
                      COMMENT "Verifying ${ARG_TARGET}"
                      COMMAND . ${launch_file}
                      DEPENDS ${verifier_cli} "${binary_file}")

    if (DEFINED ARG_DEPENDS)
        add_dependencies(${ARG_TARGET} ${ARG_DEPENDS})
    endif()

endfunction()
