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
# Building and running benchmarks in Panda Assembly from the Panda source tree.

if(NOT DEFINED PANDA_ROOT)
    message(FATAL_ERROR "Not a Panda build")
endif()

define_property(SOURCE
                PROPERTY panda_benchmark_name
                BRIEF_DOCS "Short benchmark name"
                FULL_DOCS  "Short benchmark name")
define_property(SOURCE
                PROPERTY panda_benchmark_vmb_name
                BRIEF_DOCS "Short VMB name"
                FULL_DOCS  "Short VMB name")
define_property(SOURCE
                PROPERTY panda_benchmark_stack_limit
                BRIEF_DOCS "Stack limit for this benchmark"
                FULL_DOCS "Stack limit for this benchmark in bytes. Set 0 to use default value")
define_property(SOURCE
                PROPERTY panda_benchmark_run_interpreter
                BRIEF_DOCS "Run this benchmark with interpreter?"
                FULL_DOCS  "Run this benchmark with interpreter?")
define_property(SOURCE
                PROPERTY panda_benchmark_run_enable_gc
                BRIEF_DOCS "Run with enabled gc?"
                FULL_DOCS  "Run with enabled gc?")
define_property(SOURCE
                PROPERTY panda_benchmark_run_default
                BRIEF_DOCS "Run in default mode?"
                FULL_DOCS  "Run in default mode?")

set(PANDA_BENCHMARKS)
set(PANDA_BENCHMARKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
function(panda_add_benchmark
         name vmb_name stack_limit interpreter enable_gc default)
    # Allow to write something like this: 64 * 1024 * 1024
    math(EXPR stack_limit "${stack_limit}")

    set(file_name "${PANDA_BENCHMARKS_DIR}/${name}.pa")

    set_source_files_properties(${file_name} PROPERTIES
        panda_benchmark_name            "${name}"
        panda_benchmark_vmb_name        "${vmb_name}"
        panda_benchmark_stack_limit     "${stack_limit}"
        panda_benchmark_run_interpreter ${interpreter}
        panda_benchmark_run_enable_gc ${enable_gc}
        panda_benchmark_run_default   ${default})

    list(APPEND PANDA_BENCHMARKS ${file_name})
    set (PANDA_BENCHMARKS ${PANDA_BENCHMARKS} PARENT_SCOPE)
endfunction()

# Arguments:
# * 1. Panda benchmark name
# * 2. VMB benchmark name; if empty, not a part of VMB yet
# * 3. Stack limit
# * 4. Run with interpreter (without compiler at all)?
# * 5. Gc type (default - for default gc)
# * 6. Run in default mode?
#                    1.name                     2.vmb_name            3.stack_limit       4.interpreter
#                    |                          |                     |                   |     5.gc_type
#                    |                          |                     |                   |     |        6.default
panda_add_benchmark("3d-morph"                 "Morph3d"              0                   TRUE  default  TRUE)
panda_add_benchmark("access-binary-trees"      "AccessBinaryTrees"    0                   TRUE  default  TRUE)
panda_add_benchmark("access-fannkuch"          "AccessFannkuch"       0                   TRUE  default  TRUE)
panda_add_benchmark("access-nbody"             "AccessNBody"          0                   TRUE  default  TRUE)
panda_add_benchmark("access-nsieve"            "AccessNSieve"         0                   TRUE  default  TRUE)
panda_add_benchmark("bitops-3bit-bits-in-byte" "Bitops3BitBitsInByte" 0                   TRUE  default  TRUE)
panda_add_benchmark("bitops-bits-in-byte"      "BitopsBitsInByte"     0                   TRUE  default  TRUE)
panda_add_benchmark("bitops-bitwise-and"       "BitopsBitwiseAnd"     0                   TRUE  default  TRUE)
panda_add_benchmark("bitops-nsieve-bits"       "BitopsNSieveBits"     0                   TRUE  default  TRUE)
panda_add_benchmark("controlflow-recursive"    "ControlFlowRecursive" "384 * 1024 * 1024" TRUE  default  TRUE)
panda_add_benchmark("math-cordic"              "MathCordic"           0                   TRUE  default  TRUE)
panda_add_benchmark("math-partial-sums"        "MathPartialSums"      0                   TRUE  default  TRUE)
panda_add_benchmark("math-spectral-norm"       "MathSpectralNorm"     0                   TRUE  default  TRUE)

add_custom_target(benchmarks-panda-assembly-interpreter
                  COMMENT "Running benchmarks in Panda Assembly in interpreter")
add_custom_target(benchmarks-panda-assembly-default
                  COMMENT "Running benchmarks in Panda Assembly")

add_dependencies(benchmarks
    benchmarks-panda-assembly-interpreter
    benchmarks-panda-assembly-default
)

function(add_benchmark
         common_target_basename name file interpreter stack_limit gc_type default)

    if (stack_limit)
        set(PANDA_RUN_PREFIX prlimit --stack=${stack_limit} ${PANDA_RUN_PREFIX})
    endif()

    if (NOT "${gc_type}" STREQUAL "default")
        set(ENABLE_GC_OPTION "--gc-type=${gc_type}")
    endif()

    set(source_language "panda-assembly")
    set(entry_point "_GLOBAL::main")
    set(skip_build FALSE)

    string(TOLOWER "${CMAKE_BUILD_TYPE}" cm_build_type)
    if ("${cm_build_type}" STREQUAL "")
        set(cm_build_type "debug")
    endif()
    if ("${cm_build_type}" STREQUAL "debug")
        set(benchmark_timeout "5s")
    endif()

    set(subdir_prefix "benchmarks/${common_target_basename}")

    if (interpreter)
        panda_add_asm_file(TARGET "${name}-interpreter"
            FILE "${file}"
            LANGUAGE_CONTEXT "${source_language}"
            SUBDIR "${subdir_prefix}"
            ENTRY "${entry_point}"
            TIMEOUT "${benchmark_timeout}"
            SKIP_BUILD ${skip_build}
            DEPENDS ${deps}
            AOT_MODE FALSE
            RUNTIME_OPTIONS ${ENABLE_GC_OPTION} "--compiler-enable-jit=false" "--limit-standard-alloc=true")
        add_dependencies(${common_target_basename}-interpreter "${name}-interpreter")
    endif()

    if (default)
        panda_add_asm_file(TARGET "${name}-default"
            FILE "${file}"
            LANGUAGE_CONTEXT "${source_language}"
            SUBDIR "${subdir_prefix}"
            ENTRY "${entry_point}"
            TIMEOUT "${benchmark_timeout}"
            SKIP_BUILD ${skip_build}
            DEPENDS ${deps}
            AOT_MODE FALSE
            RUNTIME_OPTIONS ${ENABLE_GC_OPTION} "--compiler-enable-jit=true")
        add_dependencies(${common_target_basename}-default "${name}-default")
    endif()

endfunction()

foreach(benchmark ${PANDA_BENCHMARKS})
    get_source_file_property(name "${benchmark}" panda_benchmark_name)
    get_source_file_property(interpreter "${benchmark}" panda_benchmark_run_interpreter)
    get_source_file_property(enable_gc "${benchmark}" panda_benchmark_run_enable_gc)
    get_source_file_property(stack_limit "${benchmark}" panda_benchmark_stack_limit)
    get_source_file_property(default "${benchmark}" panda_benchmark_run_default)

    add_benchmark(benchmarks-panda-assembly "${name}" "${benchmark}"
        ${interpreter}
        ${stack_limit}
        ${enable_gc}
        ""
        ${default})
endforeach()
